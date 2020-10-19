/**
 *  scanner.cpp
 *
 *    2020/09/12
 */

#include <exception>
#include <iostream>
#include <map>
#include <thread>

#include <signal.h>
#include <unistd.h>

#include "scanner.h"
#include "ble_central.h"

#include "tt_lib.h"


using namespace std;


/**
 * scanner
 */

scanner::scanner() {
	if (pipe(_pipe_notify_to_scanner) == -1 || pipe(_pipe_notify_to_sender) == -1)
		throw runtime_error(string("Failed to initialize pipes. ") + strerror(errno));

	// for scanner
	_fds_scanner[0].fd = _pipe_notify_to_scanner[0];
	_fds_scanner[0].events = POLLIN;

	// for sender
	_fds_sender[0].fd = _pipe_notify_to_sender[0];
	_fds_sender[0].events = POLLIN;
}

scanner::~scanner() {
	close(_pipe_notify_to_scanner[0]);
	close(_pipe_notify_to_scanner[1]);

	close(_pipe_notify_to_sender[0]);
	close(_pipe_notify_to_sender[1]);
}

void scanner::run() {
	exception_ptr ep = nullptr;

	tph_datastore store;

	scanner_worker sc_worker;
	thread th_2(ref(sc_worker), ref(_fds_scanner), ref(store), ref(ep));

	th_2.join();

	cout << "finished." << endl;

	if (ep != nullptr)
		rethrow_exception(ep);
}


/**
 * scanner_worker
 */

void scanner_worker::operator()(struct pollfd (&fds)[2], tph_datastore& datastore, exception_ptr& ep) {
	ep = nullptr;

	try {
		ble_central central;

		central.open_device();
		central.start_hci_scan(fds, datastore);

	} catch (...) {
		ep = current_exception();
    }
}

/**
 * GLOBAL
 */

static int sg_fd4sigint = -1;

void sigint_handler(int sig) {
	if (sig == SIGINT) {
		DEBUG_PUTS("SIGINT RECEIVED.");
		write(sg_fd4sigint, "\x01", 1);
	}
}

int main(int argc, char** argv)
{
	struct sigaction sa = {};
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

	try {
		scanner svr;

		sg_fd4sigint = svr.getfd_of_notify_signal();

		svr.run();

	} catch (exception& ex) {
		cerr << "Error in main(): " << ex.what() << endl;
	}

	return 0;
}


// end of scanner.cpp
