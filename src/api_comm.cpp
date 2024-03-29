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

	_headers = curl_slist_append(_headers, "Content-Type: application/json");

	_fds_poll[0].fd = fd_sighup;
	_fds_poll[0].events = POLLIN;
	_fds_poll[1].fd = fd_read;
	_fds_poll[1].events = POLLIN;
}

api_comm::~api_comm() {
	if (_curl_handle) {
		curl_slist_free_all(_headers);
		curl_easy_cleanup(_curl_handle);
	}
}

void api_comm::chg_settings(bool f_call_api, api_info_t& info) {
	_features_call_api = f_call_api;	// call api ?

	_protocol = info.protocol;
	_host = info.host;
	_port = info.port;
	_path = info.path;
	_ctype = info.ctype;
}

void api_comm::send_data(const char* json) {
	if (!_curl_handle) {
		tt_logger::instance().puts("API_COMM[WARN] at send_data(): curl handle is null.");
		return;
	} else if (!_features_call_api)	{
		tt_logger::instance().puts("API_COMM[WARN] at send_data(): the feature of calling api is unavailable.");
		return;
	}

	string url = _protocol + "://" + _host + ":" + _port + _path;

	curl_easy_setopt(_curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(_curl_handle, CURLOPT_HTTPHEADER, _headers);
	curl_easy_setopt(_curl_handle, CURLOPT_POST, 1);
	curl_easy_setopt(_curl_handle, CURLOPT_POSTFIELDS, json);
	curl_easy_setopt(_curl_handle, CURLOPT_POSTFIELDSIZE, strlen(json));

	int cnt = 0;
	while (cnt < RETRY_MAX_COUNT) {
		CURLcode ret = curl_easy_perform(_curl_handle);

		if (ret == CURLE_OK) {
			long resp_code = -1;
			ret = curl_easy_getinfo(_curl_handle, CURLINFO_RESPONSE_CODE, &resp_code);
			if (ret == CURLE_OK && resp_code == 200) {
				tt_logger::instance().puts("API_COMM[INFO] at send_data(): 200 OK.");
				break;
			} else {
				tt_logger::instance().printf("API_COMM[ERROR] at send_data(): http response code %ld was returned. retrying...", resp_code);
			}

		} else {
			tt_logger::instance().printf("API_COMM[ERROR] at send_data(): %s\n", curl_easy_strerror(ret));
		}

		sleep(RETRY_SLEEP_SECS);
		cnt++;
	}
}

void api_comm::start() {
    try {
		while (1) {
			if (poll(_fds_poll, 2, -1) < 0 && errno != EINTR) {
				char msgbuff[128] = {0};
				strerror_r(errno, msgbuff, 128);
				tt_logger::instance().printf("poll error occurred in api_comm. %s\n", msgbuff);
				continue;
			}

			if (_fds_poll[0].revents & POLLIN) {
				DEBUG_PUTS("API_COMM: SIGNAL NOTIFIED");
				throw tt_apicomm_exception("read signal notify from pipe.");
			}

			if (_fds_poll[1].revents & POLLIN) {
				char jd_buff[256] = {0};

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
