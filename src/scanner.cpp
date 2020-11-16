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

#include <curl/curl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "scanner.h"
#include "ble_central.h"
#include "api_comm.h"

#include "tt_lib.h"


#define CONTINUOUS_RUNNING 0


using namespace std;


/**
 * scheduler
 */

scheduler::scheduler() : _ep_central(nullptr), _ep_apicomm(nullptr), _rcv_sigint(false) {
	if (pipe(_pipe_notify_to_central) == -1 || pipe(_pipe_notify_to_apicomm) == -1 || pipe(_pipe_data_from_central) == -1) {
		char msgbuff[128];
		strerror_r(errno, msgbuff, 128);
		throw runtime_error(string("Failed to initialize pipes. ") + msgbuff);
	}

	_worker_ac = new apicomm_worker(_pipe_notify_to_apicomm[0], _pipe_data_from_central[0]);
	_worker_cl = new central_worker(_pipe_notify_to_central[0], _pipe_data_from_central[1]);
}

scheduler::~scheduler() {
	delete _worker_ac;
	delete _worker_cl;

	close(_pipe_notify_to_central[0]);
	close(_pipe_notify_to_central[1]);

	close(_pipe_notify_to_apicomm[0]);
	close(_pipe_notify_to_apicomm[1]);

	close(_pipe_data_from_central[0]);
	close(_pipe_data_from_central[1]);
}

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
		curl_global_init(CURL_GLOBAL_ALL);

		unique_lock<mutex> lock(_mtx);

		while (1) {
#if CONTINUOUS_RUNNING
			int sleep_sec = get_sec_for_alarm_00();

			DEBUG_PRINTF("SCHEDULER: falling into a sleep...%ds\n", sleep_sec);
			if (_cond.wait_for(lock, chrono::seconds(sleep_sec), [this]{ return _rcv_sigint; })) {
				cout << "SCHEDULER: stopped waiting." << endl;
				break;
			}

			DEBUG_PUTS("SCHEDULER: waked up!");
#endif
			start_scanning_peripherals();

			if (_cond.wait_for(lock, chrono::seconds(DURATION_SEC_OF_ACT), [this]{ return _rcv_sigint; })) {
				stop_scanning_peripherals();
				break;
			}

			DEBUG_PRINTF("SCHEDULER: timeout => %d sec.", DURATION_SEC_OF_ACT);
			stop_scanning_peripherals();

#if CONTINUOUS_RUNNING
			DEBUG_PUTS("SCHEDULER: next loop");
#else
			break;
#endif
		}

	} catch (exception& ex) {
		stop_scanning_peripherals();
		cerr << "[ERROR] at scheduler::run() => " << ex.what() << endl;
	}

	curl_global_cleanup();

	cout << "SCHEDULER: normally finished." << endl;
}

void scheduler::start_scanning_peripherals() {
	DEBUG_PUTS("SCHEDULER: start scanning.");

	_th_ac = thread(ref(*_worker_ac), ref(_ep_apicomm));
	_th_cl = thread(ref(*_worker_cl), ref(_ep_central));
}

void scheduler::stop_scanning_peripherals() {
	DEBUG_PUTS("SCHEDULER: stop scanning.");

	write(_pipe_notify_to_central[1], "\x01", 1);
	write(_pipe_notify_to_apicomm[1], "\x01", 1);

	_th_cl.join();
	_th_ac.join();

	try {
		if (_ep_apicomm != nullptr) rethrow_exception(_ep_apicomm);
	} catch (exception& ex) {
		cerr << "SCHEDULER[ERROR]: at api_comm => " << ex.what() << endl;
	}

	try {
		if (_ep_central != nullptr) rethrow_exception(_ep_central);
	} catch (exception& ex) {
		cerr << "SCHEDULER[ERROR]: at central => " << ex.what() << endl;
	}

	_ep_apicomm = _ep_central = nullptr;
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

	apicomm_worker ac_worker(_pipe_notify_to_sender[0], _pipe_data_from_scanner[0]);
	thread th_1(ref(ac_worker), ref(ep1));

	central_worker cl_worker(_pipe_notify_to_scanner[0], _pipe_data_from_scanner[1]);
	thread th_2(ref(cl_worker), ref(store), ref(ep2));

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
 * central_worker
 */

void central_worker::operator()(exception_ptr& ep) {
	ep = nullptr;
	tph_datastore store;

	try {
		ble_central central(_fd_sig, _fd_w);

		central.open_device();
		central.start_hci_scan(store);

	} catch (...) {
		ep = current_exception();
    }
}

void central_worker::operator()(tph_datastore& datastore, exception_ptr& ep) {
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
 * apicomm_worker
 */

void apicomm_worker::operator()(exception_ptr& ep) {
	ep = nullptr;

	try {
		api_comm communicator(_fd_sig, _fd_r);

		communicator.start();

	} catch (...) {
		ep = current_exception();
    }
}


/**
 * GLOBAL
 */

static scheduler svr_scheduler;

void sigint_handler(int sig) {
	if (sig == SIGINT) {
		DEBUG_PUTS("GLOBAL: SIGINT RECEIVED.");
		svr_scheduler.sigint();
	}
}

int main(int argc, char** argv)
{
	struct sigaction sa = {};
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

	try {
		svr_scheduler.run();

	} catch (exception& ex) {
		cerr << "Error in main(): " << ex.what() << endl;
	}

	return 0;
}


// end of scanner.cpp
