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
public:
	data_sender(void) {}
	~data_sender() {}

	void start(struct pollfd* fds);

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
