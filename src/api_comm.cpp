/**
 * api_comm.cpp
 *
 *    2020/10/19
 */

#include <iostream>

#include <string.h>
#include <unistd.h>

#include "api_comm.h"

#include "tt_lib.h"


using namespace std;


api_comm::api_comm(int fd_sighup, int fd_read) {
	_curl_handle = curl_easy_init();
	if (!_curl_handle)
		throw tt_apicomm_exception("failed to initialize Curl handle.");

	_fds_poll[0].fd = fd_sighup;
	_fds_poll[0].events = POLLIN;
	_fds_poll[1].fd = fd_read;
	_fds_poll[1].events = POLLIN;
}

api_comm::~api_comm() {
	if (_curl_handle) curl_easy_cleanup(_curl_handle);
}
		
void api_comm::send_data(const char* json) {
	if (!_curl_handle) return;

	string url = PROTOCOL + "://" + HOST + ":" + PORT + "/api/v1/tph_register";
cout << url << endl;

	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_POST, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_POSTFIELDS, json);
	curl_easy_setopt(_curl_handle, CURLOPT_POSTFIELDSIZE, strlen(json));

	/*int cnt = 0;
	while (cnt < 5) {
		CURLcode resp = curl_easy_perform(_curl_handle);

		if (resp == CURLE_OK) {
			break;
		} else {
			cerr << "API_COMM[ERROR] at send_data(): " << curl_easy_strerror(resp) << endl;
		}

		sleep(10);
		cnt++;
	}*/
}

void api_comm::start() {
    try {
		while (1) {
			if (poll(_fds_poll, 2, -1) < 0 && errno != EINTR) {
				char msgbuff[128] = {0};
				strerror_r(errno, msgbuff, 128);
				cerr << "poll error occurred in api_comm. " << msgbuff << endl;
				continue;
			}

			if (_fds_poll[0].revents & POLLIN) {
				DEBUG_PUTS("API_COMM: SIGNAL NOTIFIED");
				throw tt_apicomm_exception("read signal notify from pipe.");
			}

			if (_fds_poll[1].revents & POLLIN) {
				char jd_buff[128] = {0};

				int len = 0;
				if ((len = read(_fds_poll[1].fd, jd_buff, sizeof(jd_buff))) < 0) {
                    if (errno == EAGAIN) {
                        DEBUG_PUTS("API_COMM: EAGAIN");
                        continue;
                    } else if (errno == EINTR) {
                        DEBUG_PUTS("API_COMM: read RECEIVED ANY SIGNAL");
                        continue;
                    } else {
                        throw tt_apicomm_exception("read error.");
                    }
                }

				DEBUG_PRINTF("API_COMM: Received => %s\n", jd_buff);
				send_data(jd_buff);
			}

			DEBUG_PUTS("API_COMM: RE-POLL(NO EVENTS)...");
		}	// while loop

    } catch (tt_apicomm_exception& exp) {
		throw exp;
	}
}


// end of api_comm.cpp
