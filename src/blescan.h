/**
 *  blescan.h
 *
 *    2020/09/12
 */

#include <stdexcept>

class tt_ble_exception : public std::runtime_error {
	public:
		tt_ble_exception(const std::string& msg) : std::runtime_error(msg) {}
};


int read_flags(uint8_t* flags, const uint8_t* data, size_t size);
static void sigint_handler(int sig);


// end of blescan.h
