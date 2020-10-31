#ifndef BLESCAN_H
#define BLESCAN_H

/**
 *  scanner.h
 *
 *    2020/09/12
 */

#include <exception>

#include <poll.h>

#include "tph.h"


/**
 * CLASS scanner
 */

class scanner {
	int _pipe_notify_to_scanner[2];
	int _pipe_notify_to_sender[2];
	int _pipe_data_from_scanner[2];

public:
	scanner(void);
	~scanner();

	int getfd_of_notify_signal_4scanner() { return _pipe_notify_to_scanner[1]; }
	int getfd_of_notify_signal_4sender() { return _pipe_notify_to_sender[1]; }

	void run(void);

};


/**
 * CLASS scanner_worker
 */

class scanner_worker {
	int _fd_sig, _fd_w;

public:
	scanner_worker(int fd_sighup, int fd_write) : _fd_sig(fd_sighup), _fd_w(fd_write) {}
	~scanner_worker() {}

	//void operator()(struct pollfd (&fds)[2], tph_datastore& datastore, std::exception_ptr& ep);
	void operator()(tph_datastore& datastore, std::exception_ptr& ep);

};


/**
 * CLASS sender_worker
 */

class sender_worker {
	int _fd_sig, _fd_r;

public:
	sender_worker(int fd_sighup, int fd_read) : _fd_sig(fd_sighup), _fd_r(fd_read) {}
	~sender_worker() {}

	//void operator()(struct pollfd (&fds)[1], std::exception_ptr& ep);
	void operator()(std::exception_ptr& ep);

};


// end of scanner.h

#endif
