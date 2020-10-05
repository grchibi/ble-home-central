/**
 *  blescan.cpp
 *
 *    2020/09/12
 */

#include <exception>
#include <iostream>
#include <signal.h>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "blescan.h"
#include "tph.h"

#define FLAGS_AD_TYPE 0x01
#define FLAGS_LIMITED_MODE_BIT 0x01
#define FLAGS_GENERAL_MODE_BIT 0x02

#define HCI_STATE_NONE			0
#define HCI_STATE_OPEN			2
#define HCI_STATE_SCANNING		3
#define HCI_STATE_FILTERING	4


using namespace std;


typedef struct hci_state {
	hci_state(void) { device_id = 0; device_handle = 0; original_filter = {0, {0, 0}, 0}; state = HCI_STATE_NONE; has_error = true; error_message = ""; }
	int device_id;
	int device_handle;
	struct hci_filter original_filter;
	int state;
	bool has_error;
	string error_message;
} hci_state_t;

typedef struct adv_data {
	adv_data(void) { *addr = '\0'; *name = '\0'; *manufacturer_data = '\0'; }
	char addr[19];
	char name[31];
	char manufacturer_data[32];
} adv_data_t;

static volatile int s_signal_received = 0;


int check_report_filter(uint8_t procedure, le_advertising_info* adv)
{
	uint8_t flags;

	// if no discovery procedure is set, all reports are treat as valid
	if (procedure == 0) return 1;

	// read flags AD type value from the advertising report if it exists
	if (read_flags(&flags, adv->data, adv->length)) return 0;

	switch (procedure) {
		case 'l':	// Limited Discovery Procedure
			if (flags & FLAGS_LIMITED_MODE_BIT)
				return 1;
			break;
		case 'g':	// General Discovery Procedure
			if (flags & (FLAGS_LIMITED_MODE_BIT | FLAGS_GENERAL_MODE_BIT))
				return 1;
			break;
		default:
			cerr << "unknown discovery procedure." << endl;
	}

	return 0;
}

string decode_advertisement_data(const char* src)
{
	string result;

	char dc_buff[19];

	// yyyymmdd
	int32_t ymd32 = ((int32_t)*src) + ((int32_t)*(src + 1) << 8) + ((int32_t)*(src + 2) << 16) + ((int32_t)*(src + 3) << 24);

	// hhmm
	int32_t hm32 = (int32_t)0 + ((int32_t)*(src + 4)) + ((int32_t)*(src + 5) << 8);
	sprintf(dc_buff, "+%d%04d", ymd32, hm32);
	result = dc_buff;

	// temperature
	
	
	return result;
}

/*void parse_eir(uint8_t* eir, size_t eir_len, adv_data_t& adv)
{
	size_t offset = 0;

	try {
		while (offset < eir_len) {
			uint8_t field_len = eir[0];

			if (field_len == 0) break;	// end of EIR

			if (eir_len < offset + field_len)
				throw tt_ble_exception("EIR parse error.");

			size_t name_len = 0, mdata_len = 0;
			switch (eir[1]) {
				case EIR_NAME_SHORT:
				case EIR_NAME_COMPLETE:
					name_len = ((field_len + 1) <= (int)sizeof(adv.name)) ? field_len : sizeof(adv.name) - 1;
					memcpy(adv.name, &eir[2], name_len);
					*(adv.name + name_len) = '\0';
					break;
				case EIR_MANUFACTURER_DATA:
					sprintf(adv.manufacturer_data, "%d", field_len);
					mdata_len = ((field_len + 1) <= (int)sizeof(adv.manufacturer_data)) ? field_len : sizeof(adv.manufacturer_data) - 1;
					memcpy(adv.manufacturer_data, &eir[2], mdata_len);
					*(adv.manufacturer_data + mdata_len) = '\0';
					break;
			}
/
			offset += field_len + 1;
			eir += field_len + 1;
		}
	} catch (tt_ble_exception& exp) {
		cout << "Error occurred in parse_eir(): " << exp.what() << endl;
	}
}*/

void error_check_and_exit(hci_state_t& state)
{
	if (state.has_error) {
		cout << "ERROR: " << state.error_message << endl;
		exit(1);
	}
}

void open_default_hci_device(hci_state_t& result)
{
	result.device_id = hci_get_route(NULL);

	if ((result.device_handle = hci_open_dev(result.device_id)) < 0) {
		result.has_error = true;
		result.error_message = "Could not open device: ";
		result.error_message += strerror(errno);
	}

	// set fd non-blocking
	/*int on = 1;
	if (ioctl(result.device_handle, FIONBIO, (char*)&on) < 0) {
		result.has_error = true;
		result.error_message = "Could not set device to non-blocking: ";
		result.error_message += strerror(errno);
	}*/

	result.state = HCI_STATE_OPEN;
	result.has_error = false;
}

int print_advertising_devices(int dev_handle, uint8_t f_type)
{
	struct hci_filter of, nf;

	socklen_t olen = sizeof(of);
	if (getsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
		cout << "Could not get socket options." << endl;
		return -1;
	}

	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);

	if (setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		cout << "Could not set socket options." << endl;
		return -1;
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	int len;
	try {
		while (1) {
			evt_le_meta_event* meta;
			
			unsigned char buff[HCI_MAX_EVENT_SIZE] = {0};
			while ((len = read(dev_handle, buff, sizeof(buff))) < 0) {
				if (errno == EINTR && s_signal_received == SIGINT) {
					len = 0;
					throw tt_ble_exception("read error, received SIGINT.");
				} else if (errno == EAGAIN || errno == EINTR) {
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
				TPHDataStore::store(*adv_info);
			}
			sleep(1);
			cout << "LOOP" << endl;
		}
	} catch (tt_ble_exception& exp) {}

	setsockopt(dev_handle, SOL_HCI, HCI_FILTER, &of, sizeof(of));

	if (len < 0) return -1;

	return 0;
}

int read_flags(uint8_t *flags, const uint8_t *data, size_t size)
{
	size_t offset;

	if (!flags || !data)
		return -EINVAL;

	offset = 0;
	while (offset < size) {
		uint8_t len = data[offset];
		uint8_t type;

		/* Check if it is the end of the significant part */
		if (len == 0)
			break;

		if (len + offset > size)
			break;

		type = data[offset + 1];

		if (type == FLAGS_AD_TYPE) {
			*flags = data[offset + 2];
			return 0;
		}

		offset += 1 + len;
	}

	return -ENOENT;
}

static void sigint_handler(int sig)
{
	s_signal_received = sig;
}

void start_hci_scan(hci_state_t& result)
{
	uint8_t scan_type = 0x01;
	uint16_t interval = htobs(0x0010);
	uint16_t window = htobs(0x0010);
	uint8_t own_type = LE_PUBLIC_ADDRESS;
	uint8_t filter_policy = 0x00;
	uint8_t filter_dup = 0x01;
	uint8_t filter_type = 0;

	if (hci_le_set_scan_parameters(result.device_handle, scan_type, interval, window, own_type, filter_policy, 10000) < 0) {
		result.has_error = true;
		result.error_message = "Failed to set scan parameters: ";
		result.error_message += strerror(errno);
		return;
	}

	if (hci_le_set_scan_enable(result.device_handle, 0x01, filter_dup, 10000) < 0) {
		result.has_error = true;
		result.error_message = "Failed to enable scan: ";
		result.error_message += strerror(errno);
		return;
	}

	result.state = HCI_STATE_SCANNING;
	cout << "Scanning..." << endl;

	if (print_advertising_devices(result.device_handle, filter_type) < 0) {
		result.has_error = true;
		result.error_message = "Could not receive advertising events: ";
		result.error_message += strerror(errno);
		//snprintf(state.error_message, sizeof(state.error_message), "Could not receive advertising events: %s", strerror(errno));
	}

	if (hci_le_set_scan_enable(result.device_handle, 0x00, filter_dup, 10000) < 0) {
		result.has_error = true;
		result.error_message = "Failed to disable scan: ";
		result.error_message += strerror(errno);
		//snprintf(state.error_message, sizeof(state.error_message), "Failed to disable scan: %s", strerror(errno));
	}

	hci_close_dev(result.device_handle);
}

int main(int argc, char** argv)
{
	hci_state_t current_hci_state;

	open_default_hci_device(current_hci_state);
	error_check_and_exit(current_hci_state);

	start_hci_scan(current_hci_state);
	error_check_and_exit(current_hci_state);


	/*bool done = false, error = false;
	while (!done && !error) {
		int len = 0;
		unsigned char buff[HCI_MAX_EVENT_SIZE];
		while ((len = read(current_hci_state.device_handle, buff, sizeof(buff))) < 0) {
			if (errno == EINTR || errno == EAGAIN) {
cout << "tsugi" << endl;
				usleep(100);
				continue;
			}

cout << "saigo" << endl;
			error = true;
		}

	}

	char addr[19] = { 0 };
	char name[248] = { 0 };

	int dev_id = hci_get_route(NULL);
	int sock = hci_open_dev(dev_id);
	if (dev_id < 0 || sock < 0) {
		perror("opening socket");
		exit(1);
	}

	int len = 8;
	int max_rsp = 255;
	int flags = IREQ_CACHE_FLUSH;
	inquiry_info* ii = new inquiry_info[max_rsp * sizeof(inquiry_info)];

	int num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	if (num_rsp < 0)
		perror("hci_inquiry");
cout << num_rsp << endl;
	for (int it = 0; it < num_rsp; it++) {
		ba2str(&(ii + it)->bdaddr, addr);
		memset(name, 0, sizeof(name));

		if (hci_read_remote_name(sock, &(ii + it)->bdaddr, sizeof(name), name, 0) < 0) {
			strcpy(name, "[unknown]");
		}

		cout << addr << "\t" << name << "\n";
	}

	delete[] ii;
	close(sock);*/

	return 0;
}
