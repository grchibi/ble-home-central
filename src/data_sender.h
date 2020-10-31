#ifndef DATA_SENDER_H
#define DATA_SENDER_H

/**
 * data_sender.h
 *
 *    2020/10/19
 */

#include <stdexcept>
#include <string>

#include <poll.h>


/**
 * CLASS data_sender
 */

class data_sender {
	struct pollfd _fds_poll[2];

public:
	data_sender(int fd_sighup, int fd_read);
	~data_sender() {}

	void start(void);

};


/**
 * CLASS tt_sender_exception
 */

class tt_sender_exception : public std::runtime_error {
    public:
        tt_sender_exception(const std::string& msg) : std::runtime_error(msg) {}
};


// end of data_sender.h

#endif
