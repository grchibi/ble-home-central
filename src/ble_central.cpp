/**
 * ble_central.cpp
 *
 *    2020/10/09
 */

#include <iostream>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ble_central.h"
#include "tph.h"
#include "tt_lib.h"


using namespace std;


volatile int ble_central::_s_signal_received = 0;

int ble_central::check_report_filter(uint8_t procedure, le_advertising_info* adv)
{
    uint8_t flags;

    // if no discovery procedure is set, all reports are treat as valid
    if (procedure == 0) return 1;

    // read flags AD type value from the advertising report if it exists
    if (read_flags(&flags, adv->data, adv->length)) return 0;

    switch (procedure) {
        case 'l':   // Limited Discovery Procedure
            if (flags & static_cast<uint8_t>(read_flag::LIMITED_MODE_BIT))
                return 1;
            break;
        case 'g':   // General Discovery Procedure
            if (flags & (static_cast<uint8_t>(read_flag::LIMITED_MODE_BIT) | static_cast<uint8_t>(read_flag::GENERAL_MODE_BIT)))
                return 1;
            break;
        default:
            cerr << "unknown discovery procedure." << endl;
    }

    return 0;
}

void ble_central::open_device() {
	_current_hci_state.device_id = hci_get_route(NULL);

    if ((_current_hci_state.device_handle = hci_open_dev(_current_hci_state.device_id)) < 0) {
        throw tt_ble_exception(string("Could not open device: ") + strerror(errno));
    }

    // set fd non-blocking
    /*int on = 1;
    if (ioctl(_current_hci_state.device_handle, FIONBIO, (char*)&on) < 0) {
		throw tt_ble_exception(string("Could not set device to non-blocking: ") + strerror(errno));
    }*/

    _current_hci_state.state = hci_state::OPEN;
}

void ble_central::scan_advertising_devices(struct pollfd* fds, tph_datastore& datastore, int dev_handle, uint8_t f_type)
{
    struct hci_filter of, nf;

    socklen_t olen = sizeof(of);
    if (getsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
		throw tt_ble_exception("Could not get socket options.");
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		throw tt_ble_exception("Could not set socket options.");
    }

	fds[1].fd = dev_handle;
	fds[1].events = POLLIN;

	int len;
    try {
		while (1) {
			if (poll(fds, 2, -1) < 0 && errno != EINTR) {
				cerr << "poll error occurred in scanner worker. " << strerror(errno) << endl;
				continue;
			}

			if (fds[0].revents & POLLIN) {
				DEBUG_PUTS("SIGNAL NOTIFIED");
				throw tt_ble_exception("read signal notify from pipe.");
			}

			if (fds[1].revents & POLLIN) {
            	evt_le_meta_event* meta;
            	unsigned char buff[HCI_MAX_EVENT_SIZE] = {0};
				
				if ((len = read(dev_handle, buff, sizeof(buff))) < 0) {
					if (errno == EAGAIN) {
						DEBUG_PUTS("EAGAIN");
						continue;
					} else if (errno == EINTR) {
						DEBUG_PUTS("read RECEIVED ANY SIGNAL");
						continue;
					} else {
                    	throw tt_ble_exception("read error.");
					}
				}

            	unsigned char* ptr = buff + (1 + HCI_EVENT_HDR_SIZE);
            	len -= (1 + HCI_EVENT_HDR_SIZE);

            	meta = (evt_le_meta_event*)ptr;

				if (meta->subevent != 0x02)
					throw tt_ble_exception("evt_le_meta_event.subevent != 0x02");

				// ignoring multiple reports
				le_advertising_info* adv_info = (le_advertising_info*)(meta->data + 1);
				if (check_report_filter(f_type, adv_info)) {
					tph_data tphdata = datastore.store(*adv_info);
					if (tphdata.is_valid())		// has BME280 data.
						cout << tphdata.create_json_data() << endl;
				}

				DEBUG_PUTS("RE-POLL(READ EVENTS)");

			} else {
				DEBUG_PUTS("RE-POLL(NO EVENTS)...");
			}
		}	// while loop

    } catch (tt_ble_exception& exp) {
		setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, sizeof(of));
		throw exp;
	}

    setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, sizeof(of));
}

int ble_central::read_flags(uint8_t *flags, const uint8_t *data, size_t size)
{
    size_t offset;

    if (!flags || !data)
        return -EINVAL;

    offset = 0;
    while (offset < size) {
        uint8_t len = data[offset];

        /* Check if it is the end of the significant part */
        if (len == 0)
            break;

        if (len + offset > size)
            break;

        uint8_t type = data[offset + 1];

        if (type == static_cast<uint8_t>(read_flag::AD_TYPE)) {
            *flags = data[offset + 2];
            return 0;
        }

        offset += 1 + len;
    }

    return -ENOENT;
}

void ble_central::start_hci_scan(struct pollfd* fds, tph_datastore& datastore) {
	uint8_t scan_type = 0x01;
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t own_type = LE_PUBLIC_ADDRESS;
    uint8_t filter_policy = 0x00;
    uint8_t filter_dup = 0x01;
    uint8_t filter_type = 0;

    if (hci_le_set_scan_parameters(_current_hci_state.device_handle, scan_type, interval, window, own_type, filter_policy, 10000) < 0) {
        cerr << "[WARN] Failed to set scan parameters: " << strerror(errno) << endl;
    }

    if (hci_le_set_scan_enable(_current_hci_state.device_handle, 0x01, filter_dup, 10000) < 0) {
        cerr << "[WARN] Failed to enable scan: " << strerror(errno) << endl;;
    }

    _current_hci_state.state = hci_state::SCANNING;
	DEBUG_PUTS("Scanning...");

	try {
		scan_advertising_devices(fds, datastore, _current_hci_state.device_handle, filter_type);

	} catch(tt_ble_exception& ex) {
        cerr << "[ERROR] Could not receive advertising events: " << ex.what() << endl;
    }

    if (hci_le_set_scan_enable(_current_hci_state.device_handle, 0x00, filter_dup, 10000) < 0) {
        throw tt_ble_exception(string("Failed to disable scan: ") + strerror(errno));
    }

    hci_close_dev(_current_hci_state.device_handle);
}


// end of ble_central.cpp
