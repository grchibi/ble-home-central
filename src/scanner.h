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

	struct pollfd _fds_scanner[2];
	struct pollfd _fds_sender[1];

public:
	scanner();
	~scanner();

	int getfd_of_notify_signal() { return _pipe_notify_to_scanner[1]; }
	void run(void);

};


/**
 * CLASS scanner_worker
 */

class scanner_worker {

public:
	scanner_worker(void) {}
	~scanner_worker() {}

	void operator()(struct pollfd (&fds)[2], tph_datastore& datastore, std::exception_ptr& ep);

};


// end of scanner.h

#endif
