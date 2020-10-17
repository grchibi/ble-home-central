/**
 *  blescan.cpp
 *
 *    2020/09/12
 */

#include <exception>
#include <iostream>
#include <map>
#include <thread>

#include <signal.h>

#include "scanner.h"
#include "ble_central.h"

#include "tt_lib.h"


using namespace std;


void sigint_handler(int sig) {
	DEBUG_PUTS("SIGNAL RECEIVED.");
	ble_central::_s_signal_received = sig;
}

void scanner_worker::operator()(tph_datastore& datastore, exception_ptr& ep) {
	ep = nullptr;

	try {
		ble_central central;

		central.open_device();
		central.start_hci_scan(datastore);

	} catch (...) {
		ep = current_exception();
    }
}

int main(int argc, char** argv)
{
	struct sigaction sa = {};
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

	try {
		exception_ptr ep = nullptr;

		tph_datastore store;

		scanner_worker sc_worker;
		thread th_1(ref(sc_worker), ref(store), ref(ep));

		th_1.join();

		cout << "finished." << endl;

		if (ep != nullptr)
			rethrow_exception(ep);

	} catch (exception& ex) {
		cerr << "Error in main(): " << ex.what() << endl;
	}

	return 0;
}
