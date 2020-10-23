/**
 * data_sender.cpp
 *
 *    2020/10/19
 */

#include <iostream>

#include <string.h>

#include "data_sender.h"

#include "tt_lib.h"


using namespace std;


void data_sender::start(struct pollfd* fds) {
    try {
		while (1) {
			if (poll(fds, 1, -1) < 0 && errno != EINTR) {
				char msgbuff[128];
				strerror_r(errno, msgbuff, 128);
				cerr << "poll error occurred in data sender worker. " << msgbuff << endl;
				continue;
			}

			if (fds[0].revents & POLLIN) {
				DEBUG_PUTS("SENDER: SIGNAL NOTIFIED");
				throw tt_sender_exception("read signal notify from pipe.");
			}

			DEBUG_PUTS("SENDER: RE-POLL(NO EVENTS)...");
		}	// while loop

    } catch (tt_sender_exception& exp) {
		throw exp;
	}
}


// end of data_sender.cpp
