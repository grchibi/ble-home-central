/**
 * tph.cpp
 *
 *    2020/10/03
 */

#include <iostream>
#include <string>

#include "tph.h"
#include "tt_lib.h"

using namespace std;


/**
 * CLASS tph_data
 */

tph_data::tph_data(const le_advertising_info& advinfo) : _t(0), _p(0), _h(0)
{
	*_addr = '\0';
	*_name = '\0';

	_initialize(advinfo);
}

void tph_data::_decode_advertisement_data(const char* src)
{
    // yyyymmdd
    int32_t ymd32 = ((int32_t)*src) + ((int32_t)*(src + 1) << 8) + ((int32_t)*(src + 2) << 16) + ((int32_t)*(src + 3) << 24);

    // hhmm
    int32_t hm32 = (int32_t)0 + ((int32_t)*(src + 4)) + ((int32_t)*(src + 5) << 8);
    //sprintf(dc_buff, "+%d%04d", ymd32, hm32);
    _dt = format("+%d%04d", ymd32, hm32);

    // temperature
	int32_t t32 = (int32_t)0 + ((int32_t)*(src + 6)) + ((int32_t)*(src + 7) << 8);
	_t = (float)((float)t32 / 100.00);

	// atmosphere pressure
	int32_t p32 = (int32_t)0 + ((int32_t)*(src + 8)) + ((int32_t)*(src + 9) << 8) + ((int32_t)*(src + 10) << 16);
	_p = (float)((float)p32 / 100.00);

	// humidity
	int32_t h32 = (int32_t)0 + ((int32_t)*(src + 11)) + ((int32_t)*(src + 12) << 8);
	_h = (float)h32 / 100.00;
}

void tph_data::_initialize(const le_advertising_info& advinfo)
{
	// address
	memset(_addr, 0, sizeof(_addr));
	ba2str(&advinfo.bdaddr, _addr);

	// name, others...
	strcpy(_name, "(unknown)");

	// update from manufacturer specific data
	update(advinfo);
}

string tph_data::create_json_data()
{
	return format("{\"dsrc\":\"%s\", \"dt\": \"%s\", \"t\": %2.2f, \"p\": %4.2f, \"h\": %3.2f}", _name, _dt.c_str(), _t, _p, _h);
}

void tph_data::update(const le_advertising_info& advinfo)
{
	uint8_t* eir = (uint8_t*)advinfo.data;
	size_t eir_len = advinfo.length;
    size_t offset = 0;
	size_t name_len = 0, mdata_len = 0;
	char manufacturer_data[32] = {0};
    try {
        while (offset < eir_len) {
            uint8_t field_len = eir[0];

            if (field_len == 0) break;  // end of EIR

            if (eir_len < offset + field_len)
                throw tph_initialization_error("EIR parse error.");

            switch (eir[1]) {
                case EIR_NAME_SHORT:
                case EIR_NAME_COMPLETE:
                    name_len = ((field_len + 1) <= (int)sizeof(_name)) ? field_len : sizeof(_name) - 1;
                    memcpy(_name, &eir[2], name_len);
					if (*(_name + name_len - 1) == '\n') name_len -= 1;
                    *(_name + name_len) = '\0';
                    break;
                case EIR_MANUFACTURER_DATA:
                    mdata_len = ((field_len + 1) <= (int)sizeof(manufacturer_data)) ? field_len : sizeof(manufacturer_data) - 1;
                    memcpy(manufacturer_data, &eir[2], mdata_len);
                    *(manufacturer_data + mdata_len) = '\0';
                    break;
            }

            offset += field_len + 1;
            eir += field_len + 1;
        }

		if (strstr(_name, "BME280_BEACON") != NULL && 15 <= mdata_len)
			_decode_advertisement_data(manufacturer_data);
		
    } catch (tph_initialization_error& exp) {
        throw tph_initialization_error(std::string("Error occurred in _initialize(): ") + exp.what());
    }
}


/**
 * CLASS tph_datastore
 */

const tph_data tph_datastore::store(const le_advertising_info& advinfo)
{
	lock_guard<mutex> lock(_mtx);

	// address
	char addr[19] = {0};
    memset(addr, 0, sizeof(addr));
    ba2str(&advinfo.bdaddr, addr);

	if (auto ite = _store.find(addr); ite != end(_store)) {	// FOUND
		ite->second.update(advinfo);

	} else {	// NOT FOUND
		tph_data newData(advinfo);
		_store[addr] = newData;
	}

	return _store[addr];
}


// end of tph.cpp
