/**
 * tph.h
 *
 *    2020/10/03
 */

#include <map>
#include <stdexcept>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#define EIR_NAME_SHORT      0x08
#define EIR_NAME_COMPLETE   0x09
#define EIR_MANUFACTURER_DATA   0xff


/**
 * CLASS TPHInitializationError
 */

class TPHInitializationError : public std::runtime_error {
    public:
        TPHInitializationError(const std::string& msg) : std::runtime_error(msg) {}
};


/**
 * CLASS TPHData
 */

class TPHData {
	char _addr[19];
	char _name[31];

	std::string _dt;
	float _t, _p, _h;

	void _decodeAdvertisementData(const char* src);
	void _initialize(const le_advertising_info& advinfo);

public:
	TPHData(void) : _addr{0}, _name{0}, _t(0), _p(0), _h(0) {}
	TPHData(const le_advertising_info& advinfo);
	~TPHData() {}

	std::string createJsonData(void);
	bool isValid(void) { return (_t != 0.0 && _p != 0.0 && _h != 0.0); }
	void update(const le_advertising_info& advinfo);

};


/**
 * CLASS TPHDataStore
 */

class TPHDataStore {
	static std::map<std::string, TPHData> s_store;

public:
	static void store(const le_advertising_info& advinfo);

};


// end of tph.h
