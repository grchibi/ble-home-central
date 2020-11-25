#ifndef API_COMM_H
#define API_COMM_H

/**
 * api_comm.h
 *
 *    2020/10/19
 */

#include <stdexcept>
#include <string>

#include <curl/curl.h>
#include <poll.h>


/**
 * CLASS api_comm
 */

class api_comm {
	std::string PROTOCOL = "http";
	std::string HOST = "52.198.149.43";
	std::string PORT = "80";
	std::string CTYPE = "application/json";

	CURL* _curl_handle;
	struct pollfd _fds_poll[2];


//  RETRY_MAX_COUNT = 3  # TIMES
//  RETRY_WAIT_TIME = 5  # SECONDS
//  REQUEST_TIMEOUT = 10 # SECONDS
//    @uri = "#{PROTOCOL}://#{HOST}:#{PORT}/api/v1/tph_register"

	void send_data(const char* json);

public:
	api_comm(int fd_sighup, int fd_read);
	~api_comm();

	void start(void);

};


/**
 * CLASS tt_apicomm_exception
 */

class tt_apicomm_exception : public std::runtime_error {
    public:
        tt_apicomm_exception(const std::string& msg) : std::runtime_error(msg) {}
};


// end of api_comm.h

#endif
