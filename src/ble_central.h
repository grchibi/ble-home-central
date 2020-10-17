#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

/**
 * ble_central.h
 *
 *    2020/10/09
 */

#include <stdexcept>
#include <string>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "tph.h"


enum class hci_state {
	NONE = 0,
	OPEN,
	SCANNING,
	FILTERING
};

enum class read_flag : uint8_t {
	AD_TYPE = 0x01,
	LIMITED_MODE_BIT = 0x01,
	GENERAL_MODE_BIT = 0x02
};

typedef struct hci_data {
    hci_data(void) { device_id = 0; device_handle = 0; original_filter = {0, {0, 0}, 0}; state = hci_state::NONE; }
    int device_id;
    int device_handle;
    struct hci_filter original_filter;
    hci_state state;
} hci_data_t;

typedef struct adv_data {
    adv_data(void) { *addr = '\0'; *name = '\0'; *manufacturer_data = '\0'; }
    char addr[19];
    char name[31];
    char manufacturer_data[32];
} adv_data_t;


/**
 * CLASS tt_ble_exception
 */

class tt_ble_exception : public std::runtime_error {
    public:
        tt_ble_exception(const std::string& msg) : std::runtime_error(msg) {}
};


/**
 * CLASS ble_central
 */

class ble_central {
	hci_data_t _current_hci_state;

	int check_report_filter(uint8_t procedure, le_advertising_info* adv);
	void scan_advertising_devices(tph_datastore& datastore, int dev_handle, uint8_t f_type);
	int read_flags(uint8_t *flags, const uint8_t *data, size_t size);

public:
	ble_central(void) {}
	~ble_central() {}

	static volatile int _s_signal_received;

	void open_device(void);
	void start_hci_scan(tph_datastore& datastore);

};


// end of ble_central.h

#endif
