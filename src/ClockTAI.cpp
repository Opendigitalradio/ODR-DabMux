/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
   */
/*
   This file is part of ODR-DabMux.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This file downloads the TAI-UTC bulletins from the from IETF and parses them
 * so that correct time can be communicated in EDI timestamps.
 *
 * This file contains self-test code that can be executed by running
 * g++ -g -Wall -DTEST -DHAVE_CURL -std=c++11 -lcurl -pthread \
 *   ClockTAI.cpp Log.cpp -o taitest && ./taitest
 */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "ClockTAI.h"
#include "Log.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#if SUPPORT_SETTING_CLOCK_TAI
#  include <sys/timex.h>
#endif
#ifdef HAVE_CURL
#  include <curl/curl.h>
#endif
#include <array>
#include <string>
#include <iostream>
#include <algorithm>
#include <regex>

using namespace std;

#ifdef TEST
static bool wait_longer = true;
#endif

constexpr int download_retry_interval_hours = 1;

// Offset between NTP time and POSIX time:
// timestamp_unix = timestamp_ntp - ntp_unix_offset
const int64_t ntp_unix_offset = 2208988800L;

// leap seconds insertion bulletin is available at
static const char* tai_ietf_url =
    "http://www.ietf.org/timezones/data/leap-seconds.list";
// and in the tz distribution
static const char* tai_tz_url =
    "https://raw.githubusercontent.com/eggert/tz/master/leap-seconds.list";

static const char* tai_ietf_cache_file = "/tmp/odr-dabmux-leap-seconds.cache";

ClockTAI::ClockTAI() :
    RemoteControllable("clocktai")
{
    RC_ADD_PARAMETER(expiry, "Number of seconds until TAI Bulletin expires");
}

int ClockTAI::get_valid_offset()
{
    int offset = 0;
    bool offset_valid = false;

    if (bulletin_is_valid()) {
#if TEST
        etiLog.level(info) << "Bulletin already valid";
#endif
        offset = parse_ietf_bulletin();
        offset_valid = true;
    }
    else {
        load_bulletin_from_file(tai_ietf_cache_file);

        if (bulletin_is_valid()) {
#if TEST
            etiLog.level(info) << "Bulletin from file valid";
#endif
            offset = parse_ietf_bulletin();
            offset_valid = true;
        }
        else {
            for (const auto url : {tai_ietf_url, tai_tz_url}) {
                try {
                    download_tai_utc_bulletin(url);
#if TEST
                    etiLog.level(info) << "Load bulletin from " << url;
#endif
                    if (bulletin_is_valid()) {
#if TEST
                        etiLog.level(info) << "Bulletin from " << url << " valid";
#endif
                        offset = parse_ietf_bulletin();
                        offset_valid = true;
                    }
                }
                catch (const runtime_error& e) {
                    etiLog.level(warn) <<
                        "TAI-UTC offset could not be retrieved from " <<
                        url << " : " << e.what();
                }

                if (offset_valid) {
                    update_cache(tai_ietf_cache_file);
                    break;
                }
            }
        }
    }

    if (offset_valid) {
        // With the current evolution of the offset, we're probably going
        // to reach 500 long after DAB gets replaced by another standard.
        if (offset < 0 or offset > 500) {
            stringstream ss;
            ss << "TAI offset " << offset << " out of range";
            throw range_error(ss.str());
        }

        return offset;
    }
    else {
        // Try again later
        m_bulletin_download_time += chrono::hours(download_retry_interval_hours);

        throw download_failed();
    }
}


int ClockTAI::get_offset()
{
    using namespace std::chrono;
    auto time_now = system_clock::now();

    std::unique_lock<std::mutex> lock(m_data_mutex);

    if (not m_offset_valid) {
#ifdef TEST
        m_offset = 37; // Valid in early 2017
        m_offset_valid = true;

        // Simulate requiring a new download
        m_bulletin_download_time = system_clock::now() - hours(24 * 40);
#else
        // First time we run we must block until we know
        // the offset
        m_offset = get_valid_offset();
        m_offset_valid = true;
        m_bulletin_download_time = system_clock::now();
#endif
        etiLog.level(info) <<
            "Initialised TAI-UTC offset to " << m_offset << "s.";
    }

    if (time_now - m_bulletin_download_time > hours(24 * 31)) {
        // Refresh if it's older than one month. Leap seconds are
        // announced several months in advance

        if (m_offset_future.valid()) {
            auto state = m_offset_future.wait_for(seconds(0));
            switch (state) {
                case future_status::ready:
                    try {
                        m_offset = m_offset_future.get();
                        m_offset_valid = true;
                        m_bulletin_download_time = system_clock::now();

                        etiLog.level(info) <<
                            "Updated TAI-UTC offset to " << m_offset << "s.";
                    }
                    catch (const download_failed&) {
                        etiLog.level(warn) <<
                            "TAI-UTC download failed, will retry in " +
                            to_string(download_retry_interval_hours) + " hour(s)";
                    }
#ifdef TEST
                    wait_longer = false;
#endif
                    break;

                case future_status::deferred:
                case future_status::timeout:
                    // Not ready yet
#ifdef TEST
                    etiLog.level(debug) << "  async not ready yet";
#endif
                    break;
            }
        }
        else {
#ifdef TEST
            etiLog.level(debug) << " Launch async";
#endif
            m_offset_future = async(launch::async,
                    &ClockTAI::get_valid_offset, this);
        }
    }

    return m_offset;
}

#if SUPPORT_SETTING_CLOCK_TAI
int ClockTAI::update_local_tai_clock(int offset)
{
    struct timex timex_request;
    timex_request.modes = ADJ_TAI;
    timex_request.constant = offset;

    int err = adjtimex(&timex_request);
    if (err == -1) {
        perror("adjtimex");
    }

    printf("adjtimex: %d, tai %d\n", err, timex_request.tai);

    return err;
}
#endif

int ClockTAI::parse_ietf_bulletin()
{
    // Example Line:
    // 3692217600	37	# 1 Jan 2017
    //
    // NTP timestamp<TAB>leap seconds<TAB># some comment
    // The NTP timestamp starts at epoch 1.1.1900.
    // The difference between NTP timestamps and unix epoch is 70
    // years i.e. 2208988800 seconds

    std::regex regex_bulletin(R"(([0-9]+)\s+([0-9]+)\s+#.*)");

    time_t now = time(nullptr);

    int tai_utc_offset = 0;

    int tai_utc_offset_valid = false;

    m_bulletin.clear();
    m_bulletin.seekg(0);

    /* We cannot just take the last line, because it might
     * be in the future, announcing an upcoming leap second.
     *
     * So we need to look at the current date, and compare it
     * with the date of the leap second.
     */
    for (string line; getline(m_bulletin, line); ) {

        std::smatch bulletin_entry;

        bool is_match = std::regex_search(line, bulletin_entry, regex_bulletin);
        if (is_match) {
            if (bulletin_entry.size() != 3) {
                throw runtime_error(
                        "Incorrect number of matched TAI IETF bulletin entries");
            }
            const string bulletin_ntp_timestamp(bulletin_entry[1]);
            const string bulletin_offset(bulletin_entry[2]);

            const int64_t timestamp_unix =
                std::atol(bulletin_ntp_timestamp.c_str()) - ntp_unix_offset;

            const int offset = std::atoi(bulletin_offset.c_str());
            // Ignore entries announcing leap seconds in the future
            if (timestamp_unix < now) {
                tai_utc_offset = offset;
                tai_utc_offset_valid = true;
            }
#if TEST
            else {
                cerr << "IETF Ignoring offset " << bulletin_offset <<
                    " at TS " << bulletin_ntp_timestamp <<
                    " in the future" << endl;
            }
#endif
        }
    }

    if (not tai_utc_offset_valid) {
        throw runtime_error("No data in TAI bulletin");
    }

    return tai_utc_offset;
}

size_t ClockTAI::fill_bulletin(char *ptr, size_t size, size_t nmemb)
{
    size_t len = size * nmemb;
    for (size_t i = 0; i < len; i++) {
        m_bulletin << ptr[i];
    }
    return len;
}

size_t ClockTAI::fill_bulletin_cb(
        char *ptr, size_t size, size_t nmemb, void *ctx)
{
    return ((ClockTAI*)ctx)->fill_bulletin(ptr, size, nmemb);
}

void ClockTAI::load_bulletin_from_file(const char* cache_filename)
{
    // Clear the bulletin
    m_bulletin.str("");
    m_bulletin.clear();

    ifstream f(cache_filename);
    if (not f.good()) {
        return;
    }

    m_bulletin << f.rdbuf();
    f.close();
}

void ClockTAI::update_cache(const char* cache_filename)
{
    ofstream f(cache_filename);
    if (not f.good()) {
        throw runtime_error("TAI-UTC bulletin open cache for writing");
    }

    m_bulletin.clear();
    m_bulletin.seekg(0);
#if TEST
    etiLog.level(info) << "Update cache, state:" <<
        (m_bulletin.eof() ? " eof" : "") <<
        (m_bulletin.fail() ? " fail" : "") <<
        (m_bulletin.bad() ? " bad" : "");
#endif

    f << m_bulletin.rdbuf();
    f.close();
}

bool ClockTAI::bulletin_is_valid()
{
    return bulletin_expiry_delay() > 0;
}

int64_t ClockTAI::bulletin_expiry_delay() const
{
    // The bulletin contains one line that specifies an expiration date
    // in NTP time. If that point in time is in the future, we consider
    // the bulletin valid.
    //
    // The entry looks like this:
    //#@	3707596800

    std::regex regex_expiration(R"(#@\s+([0-9]+))");

    time_t now = time(nullptr);

    m_bulletin.clear();
    m_bulletin.seekg(0);

    for (string line; getline(m_bulletin, line); ) {
        std::smatch bulletin_entry;

        bool is_match = std::regex_search(line, bulletin_entry, regex_expiration);
        if (is_match) {
            if (bulletin_entry.size() != 2) {
                throw runtime_error(
                        "Incorrect number of matched TAI IETF bulletin expiration");
            }
            const string expiry_data_str(bulletin_entry[1]);
            const int64_t expiry_unix =
                std::atol(expiry_data_str.c_str()) - ntp_unix_offset;

            if (expiry_unix > now) {
                return expiry_unix - now;
            }
        }
    }
    return -1;
}

void ClockTAI::download_tai_utc_bulletin(const char* url)
{
    // Clear the bulletin
    m_bulletin.str("");
    m_bulletin.clear();

#ifdef HAVE_CURL
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ClockTAI::fill_bulletin_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

        res = curl_easy_perform(curl);
        /* always cleanup ! */
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw runtime_error( "TAI-UTC bulletin download failed: " +
                    string(curl_easy_strerror(res)));
        }
    }
#else
    throw runtime_error("Cannot download TAI Clock information without cURL");
#endif // HAVE_CURL
}

void ClockTAI::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "expiry") {
        throw ParameterError("Parameter '" + parameter +
            "' is not read-only in controllable " + get_rc_name());
    }
    else {
        throw ParameterError("Parameter '" + parameter +
            "' is not exported by controllable " + get_rc_name());
    }
}

const string ClockTAI::get_parameter(const string& parameter) const
{
    if (parameter == "expiry") {
        std::unique_lock<std::mutex> lock(m_data_mutex);
        int64_t expiry = bulletin_expiry_delay();
        if (expiry > 0) {
            return to_string(expiry);
        }
        else {
            return "Bulletin expired or invalid!";
        }
    }
    else {
        throw ParameterError("Parameter '" + parameter +
            "' is not exported by controllable " + get_rc_name());
    }
}

#if 0
// Example testing code
void debug_tai_clk()
{
    struct timespec rt_clk;

    int err = clock_gettime(CLOCK_REALTIME, &rt_clk);
    if (err) {
        perror("REALTIME clock_gettime failed");
    }

    struct timespec tai_clk;

    err = clock_gettime(CLOCK_TAI, &tai_clk);
    if (err) {
        perror("TAI clock_gettime failed");
    }

    printf("RT - TAI = %ld\n", rt_clk.tv_sec - tai_clk.tv_sec);


    struct timex timex_request;
    timex_request.modes = 0; // Do not set anything

    err = adjtimex(&timex_request);
    if (err == -1) {
        perror("adjtimex");
    }

    printf("adjtimex: %d, tai %d\n", err, timex_request.tai);
}
#endif

#if TEST
int main(int argc, char **argv)
{
    using namespace std;

    ClockTAI tai;

    while (wait_longer) {
        try {
            etiLog.level(info) <<
                "Offset is " << tai.get_offset();
        }
        catch (const exception &e) {
            etiLog.level(error) <<
                "Exception " << e.what();
        }

        this_thread::sleep_for(chrono::seconds(2));
    }

    return 0;
}
#endif

