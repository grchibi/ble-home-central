/**
 * central.h
 *
 *    2020/10/09
 */

#include <stdexcept>
#include <string>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>


enum class HciState {
	NONE = 0,
	OPEN,
	SCANNING,
	FILTERING
};

enum class ReadFlag : uint8_t {
	AD_TYPE = 0x01,
	LIMITED_MODE_BIT = 0x01,
	GENERAL_MODE_BIT = 0x02
};

typedef struct hci_state {
    hci_state(void) { device_id = 0; device_handle = 0; original_filter = {0, {0, 0}, 0}; state = HciState::NONE; }
    int device_id;
    int device_handle;
    struct hci_filter original_filter;
    HciState state;
} hci_state_t;

typedef struct adv_data {
    adv_data(void) { *addr = '\0'; *name = '\0'; *manufacturer_data = '\0'; }
    char addr[19];
    char name[31];
    char manufacturer_data[32];
} adv_data_t;


/**
 * CLASS TTBleException
 */

class TTBleException : public std::runtime_error {
    public:
        TTBleException(const std::string& msg) : std::runtime_error(msg) {}
};


/**
 * CLASS Central
 */

class Central {
	static volatile int _s_signalReceived;

	static void sigintHandler(int sig) { Central::_s_signalReceived = sig; }

	hci_state_t _currentHciState;

	int checkReportFilter(uint8_t procedure, le_advertising_info* adv);
	void printAdvertisingDevices(int dev_handle, uint8_t f_type);
	int readFlags(uint8_t *flags, const uint8_t *data, size_t size);

public:
	Central(void) {}
	~Central() {}

	void openDevice(void);
	void startHciScan(void);

};


// end of central.h
