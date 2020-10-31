/**
 * data_sender.cpp
 *
 *    2020/10/19
 */

#include <iostream>

#include <string.h>
#include <unistd.h>

#include "data_sender.h"

#include "tt_lib.h"


using namespace std;


data_sender::data_sender(int fd_sighup, int fd_read) {
	_fds_poll[0].fd = fd_sighup;
	_fds_poll[0].events = POLLIN;
	_fds_poll[1].fd = fd_read;
	_fds_poll[1].events = POLLIN;
}

void data_sender::start() {
    try {
		while (1) {
			if (poll(_fds_poll, 2, -1) < 0 && errno != EINTR) {
				char msgbuff[128];
				strerror_r(errno, msgbuff, 128);
				cerr << "poll error occurred in data sender worker. " << msgbuff << endl;
				continue;
			}

			if (_fds_poll[0].revents & POLLIN) {
				DEBUG_PUTS("SENDER: SIGNAL NOTIFIED");
				throw tt_sender_exception("read signal notify from pipe.");
			}

			if (_fds_poll[1].revents & POLLIN) {
				char jd_buff[128];

				int len = 0;
				if ((len = read(_fds_poll[1].fd, jd_buff, sizeof(jd_buff))) < 0) {
                    if (errno == EAGAIN) {
                        DEBUG_PUTS("SENDER: EAGAIN");
                        continue;
                    } else if (errno == EINTR) {
                        DEBUG_PUTS("SENDER: read RECEIVED ANY SIGNAL");
                        continue;
                    } else {
                        throw tt_sender_exception("read error.");
                    }
                }

				cout << "SENDER: Received => " << jd_buff << endl;
			}

			DEBUG_PUTS("SENDER: RE-POLL(NO EVENTS)...");
		}	// while loop

    } catch (tt_sender_exception& exp) {
		throw exp;
	}
}


// end of data_sender.cpp
