/**
 * central.cpp
 *
 *    2020/10/09
 */

#include <iostream>

#include <signal.h>
#include <unistd.h>

#include "central.h"
#include "tph.h"
#include "ttlib.h"


using namespace std;


volatile int Central::_s_signalReceived = 0;

int Central::checkReportFilter(uint8_t procedure, le_advertising_info* adv)
{
    uint8_t flags;

    // if no discovery procedure is set, all reports are treat as valid
    if (procedure == 0) return 1;

    // read flags AD type value from the advertising report if it exists
    if (readFlags(&flags, adv->data, adv->length)) return 0;

    switch (procedure) {
        case 'l':   // Limited Discovery Procedure
            if (flags & static_cast<uint8_t>(ReadFlag::LIMITED_MODE_BIT))
                return 1;
            break;
        case 'g':   // General Discovery Procedure
            if (flags & (static_cast<uint8_t>(ReadFlag::LIMITED_MODE_BIT) | static_cast<uint8_t>(ReadFlag::GENERAL_MODE_BIT)))
                return 1;
            break;
        default:
            cerr << "unknown discovery procedure." << endl;
    }

    return 0;
}

void Central::openDevice() {
	_currentHciState.device_id = hci_get_route(NULL);

    if ((_currentHciState.device_handle = hci_open_dev(_currentHciState.device_id)) < 0) {
		//string msg("Could not open device: ");
        //msg += strerror(errno);

        throw TTBleException(string("Could not open device: ") + strerror(errno));
    }

    // set fd non-blocking
    //int on = 1;
    //if (ioctl(result.device_handle, FIONBIO, (char*)&on) < 0) {
    //    result.has_error = true;
    //    result.error_message = "Could not set device to non-blocking: ";
    //    result.error_message += strerror(errno);
    //}

    _currentHciState.state = HciState::OPEN;
}

void Central::printAdvertisingDevices(int dev_handle, uint8_t f_type)
{
    struct hci_filter of, nf;

    socklen_t olen = sizeof(of);
    if (getsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
		throw TTBleException("Could not get socket options.");
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		throw TTBleException("Could not set socket options.");
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigintHandler;
    sigaction(SIGINT, &sa, NULL);

    int len;
    try {
        while (1) {
            evt_le_meta_event* meta;

            unsigned char buff[HCI_MAX_EVENT_SIZE] = {0};
            while ((len = read(dev_handle, buff, sizeof(buff))) < 0) {
                if (errno == EINTR && Central::_s_signalReceived == SIGINT) {
                    len = 0;
                    throw TTBleException("read error, received SIGINT.");
                } else if (errno == EAGAIN || errno == EINTR) {
                    continue;
                } else {
                    throw TTBleException("read error.");
                }
            }

            unsigned char* ptr = buff + (1 + HCI_EVENT_HDR_SIZE);
            len -= (1 + HCI_EVENT_HDR_SIZE);

            meta = (evt_le_meta_event*)ptr;

			if (meta->subevent != 0x02)
                throw TTBleException("evt_le_meta_event.subevent != 0x02");

            // ignoring multiple reports
            le_advertising_info* adv_info = (le_advertising_info*)(meta->data + 1);
            if (checkReportFilter(f_type, adv_info)) {
                TPHData tphData = TPHDataStore::store(*adv_info);
                if (tphData.isValid())		// has BME280 data.
                    cout << tphData.createJsonData() << endl;
            }
            sleep(1);
			DEBUG_PUTS("LOOP");
        }
    } catch (TTBleException& exp) {
		setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, sizeof(of));
		throw exp;
	}

    setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, sizeof(of));
}

int Central::readFlags(uint8_t *flags, const uint8_t *data, size_t size)
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

        if (type == static_cast<uint8_t>(ReadFlag::AD_TYPE)) {
            *flags = data[offset + 2];
            return 0;
        }

        offset += 1 + len;
    }

    return -ENOENT;
}

void Central::startHciScan() {
	uint8_t scan_type = 0x01;
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t own_type = LE_PUBLIC_ADDRESS;
    uint8_t filter_policy = 0x00;
    uint8_t filter_dup = 0x01;
    uint8_t filter_type = 0;

    if (hci_le_set_scan_parameters(_currentHciState.device_handle, scan_type, interval, window, own_type, filter_policy, 10000) < 0) {
        throw TTBleException(string("Failed to set scan parameters: ") + strerror(errno));
    }

    if (hci_le_set_scan_enable(_currentHciState.device_handle, 0x01, filter_dup, 10000) < 0) {
        throw TTBleException(string("Failed to enable scan: ") + strerror(errno));
    }

    _currentHciState.state = HciState::SCANNING;
	DEBUG_PUTS("Scanning...");

	try {
		printAdvertisingDevices(_currentHciState.device_handle, filter_type);

	} catch(TTBleException& ex) {
        cerr << "[ERROR] Could not receive advertising events: " << ex.what() << endl;
    }

    if (hci_le_set_scan_enable(_currentHciState.device_handle, 0x00, filter_dup, 10000) < 0) {
        throw TTBleException(string("Failed to disable scan: ") + strerror(errno));
    }

    hci_close_dev(_currentHciState.device_handle);
}


// end of central.cpp
