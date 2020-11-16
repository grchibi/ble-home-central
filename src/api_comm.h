#ifndef API_COMM_H
#define API_COMM_H

/**
 * api_comm.h
 *
 *    2020/10/19
 */

#include <stdexcept>
#include <string>

#include <poll.h>


/**
 * CLASS api_comm
 */

class api_comm {
	struct pollfd _fds_poll[2];

public:
	api_comm(int fd_sighup, int fd_read);
	~api_comm() {}

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
