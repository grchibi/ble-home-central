/**
 *  scanner.cpp
 *
 *    2020/09/12
 */

#include <ctime>
#include <exception>
#include <iostream>
#include <map>
#include <thread>

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "scanner.h"
#include "ble_central.h"
#include "data_sender.h"

#include "tt_lib.h"


using namespace std;


/**
 * scheduler
 */

int scheduler::get_sec_for_alarm_00() {
	auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
	tm* now_tm = localtime(&now);
	
	return ((60 - now_tm->tm_min - 1) * 60) + (60 - now_tm->tm_sec);
}

void scheduler::sigint() {
	{
		lock_guard<mutex> lock(_mtx);
		_rcv_sigint = true;
	}

	_cond.notify_all();
}

void scheduler::run() {
	try {
		unique_lock<mutex> lock(_mtx);

		while (1) {
			int sleep_sec = get_sec_for_alarm_00();

			DEBUG_PRINTF("SCHEDULER: falling into a sleep...%ds\n", sleep_sec);
			if (_cond.wait_for(lock, chrono::seconds(sleep_sec), [this]{ return _rcv_sigint; })) {
				cout << "SCHEDULER: stopped waiting." << endl;
				break;
			}

			DEBUG_PUTS("SCHEDULER: waked up!");
			start_scanning_peripherals();

			if (_cond.wait_for(lock, chrono::seconds(DURATION_SEC_OF_ACT), [this]{ return _rcv_sigint; })) {
				stop_scanning_peripherals();
				DEBUG_PUTS("SCHEDULER: stopped scanning.");

				break;
			}

			stop_scanning_peripherals();
			DEBUG_PUTS("SCHEDULER: next loop");
		}

	} catch (exception& ex) {
		stop_scanning_peripherals();
		cerr << "[ERROR] at scheduler::run() => " << ex.what() << endl;
	}

	cout << "SCHEDULER: normally finished." << endl;
}

void scheduler::start_scanning_peripherals() {
	DEBUG_PUTS("SCHEDULER: start scanning.");
}

void scheduler::stop_scanning_peripherals() {
	DEBUG_PUTS("SCHEDULER: stop scanning.");
}


/**
 * scanner
 */

scanner::scanner() {
	if (pipe(_pipe_notify_to_scanner) == -1 || pipe(_pipe_notify_to_sender) == -1 || pipe(_pipe_data_from_scanner) == -1) {
		char msgbuff[128];
		strerror_r(errno, msgbuff, 128);
		throw runtime_error(string("Failed to initialize pipes. ") + msgbuff);
	}
}

scanner::~scanner() {
	close(_pipe_notify_to_scanner[0]);
	close(_pipe_notify_to_scanner[1]);

	close(_pipe_notify_to_sender[0]);
	close(_pipe_notify_to_sender[1]);

	close(_pipe_data_from_scanner[0]);
	close(_pipe_data_from_scanner[1]);
}

void scanner::run() {
	exception_ptr ep1 = nullptr, ep2 = nullptr;

	tph_datastore store;

	sender_worker sd_worker(_pipe_notify_to_sender[0], _pipe_data_from_scanner[0]);
	thread th_1(ref(sd_worker), ref(ep1));

	scanner_worker sc_worker(_pipe_notify_to_scanner[0], _pipe_data_from_scanner[1]);
	thread th_2(ref(sc_worker), ref(store), ref(ep2));

	th_2.join();
	th_1.join();

	try {
		if (ep1 != nullptr) rethrow_exception(ep1);
	} catch (exception& ex) {
		cerr << "[ERROR] at run(): thread1: " << ex.what() << endl;
	}

	try {
		if (ep2 != nullptr) rethrow_exception(ep2);
	} catch (exception& ex) {
		cerr << "[ERROR] at run(): thread2: " << ex.what() << endl;
	}

	cout << "Normally finished." << endl;
}


/**
 * scanner_worker
 */

void scanner_worker::operator()(tph_datastore& datastore, exception_ptr& ep) {
	ep = nullptr;

	try {
		ble_central central(_fd_sig, _fd_w);

		central.open_device();
		central.start_hci_scan(datastore);

	} catch (...) {
		ep = current_exception();
    }
}


/**
 * sender_worker
 */

void sender_worker::operator()(exception_ptr& ep) {
	ep = nullptr;

	try {
		data_sender sender(_fd_sig, _fd_r);

		sender.start();

	} catch (...) {
		ep = current_exception();
    }
}


/**
 * GLOBAL
 */

static scheduler svr_scheduler;

static int sg_fd4sigint_scanner = -1;
static int sg_fd4sigint_sender = -1;

void sigint_handler(int sig) {
	if (sig == SIGINT) {
		DEBUG_PUTS("GLOBAL: SIGINT RECEIVED.");
		svr_scheduler.sigint();

		/*write(sg_fd4sigint_scanner, "\x01", 1);
		write(sg_fd4sigint_sender, "\x01", 1);*/
	}
}

int main(int argc, char** argv)
{
	struct sigaction sa = {};
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

	try {
		/*scanner svr;

		sg_fd4sigint_scanner = svr.getfd_of_notify_signal_4scanner();
		sg_fd4sigint_sender = svr.getfd_of_notify_signal_4sender();

		svr.run();*/

		svr_scheduler.run();

	} catch (exception& ex) {
		cerr << "Error in main(): " << ex.what() << endl;
	}

	return 0;
}


// end of scanner.cpp
