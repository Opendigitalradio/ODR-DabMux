/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
   */
/*
   This file is part of the ODR-mmbTools.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file downloads the TAI-UTC bulletins from the from IETF and parses them
 * so that correct time can be communicated in EDI timestamps.
 *
 * This file contains self-test code that can be executed by running
 *  g++ -g -Wall -DTAI_TEST -DHAVE_CURL -std=c++11 -lcurl -pthread \
 *  ClockTAI.cpp Log.cpp RemoteControl.cpp -lboost_system -o taitest && ./taitest
 */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "ClockTAI.h"
#include "Log.h"

#include <ctime>
#include <cstdio>
#include <cerrno>
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
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#ifdef DOWNLOADED_IN_THE_PAST_TEST
static bool wait_longer = true;
#endif

constexpr int refresh_retry_interval_hours = 1;

// Offset between NTP time and POSIX time:
// timestamp_unix = timestamp_ntp - NTP_UNIX_OFFSET
constexpr int64_t NTP_UNIX_OFFSET = 2208988800L;

constexpr int64_t MONTH = 3600 * 24 * 30;

// leap seconds insertion bulletin is available from the IETF and in the TZ
// distribution
static array<const char*, 2> default_tai_urls = {
    "https://www.ietf.org/timezones/data/leap-seconds.list",
    "https://raw.githubusercontent.com/eggert/tz/master/leap-seconds.list",
};

// According to the Filesystem Hierarchy Standard, the data in
// /var/tmp "must not be deleted when the system is booted."
static const char *tai_cache_location = "/var/tmp/odr-leap-seconds.cache";

// read TAI offset from a valid bulletin in IETF format
static int parse_ietf_bulletin(const std::string& bulletin)
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

    stringstream ss(bulletin);

    /* We cannot just take the last line, because it might
     * be in the future, announcing an upcoming leap second.
     *
     * So we need to look at the current date, and compare it
     * with the date of the leap second.
     */
    for (string line; getline(ss, line); ) {

        std::smatch bulletin_entry;

        bool is_match = std::regex_search(line, bulletin_entry, regex_bulletin);
        if (is_match) {
            if (bulletin_entry.size() != 3) {
                throw runtime_error(
                        "Incorrect number of matched TAI IETF bulletin entries");
            }
            const string bulletin_ntp_timestamp(bulletin_entry[1]);
            const string bulletin_offset(bulletin_entry[2]);

            const int64_t timestamp_unix = std::stoll(bulletin_ntp_timestamp) - NTP_UNIX_OFFSET;

            const int offset = std::stoi(bulletin_offset.c_str());
            // Ignore entries announcing leap seconds in the future
            if (timestamp_unix < now) {
                tai_utc_offset = offset;
                tai_utc_offset_valid = true;
            }
#if TAI_TEST
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


struct bulletin_state {
    bool valid = false;
    int64_t expiry = 0;
    int offset = 0;

    bool usable() const { return valid and expiry > 0; }
    bool expires_soon() const { return usable() and expiry < 1 * MONTH; }
};

static bulletin_state parse_bulletin(const string& bulletin)
{
    // The bulletin contains one line that specifies an expiration date
    // in NTP time. If that point in time is in the future, we consider
    // the bulletin valid.
    //
    // The entry looks like this:
    //#@	3707596800

    bulletin_state ret;

    std::regex regex_expiration(R"(#@\s+([0-9]+))");

    time_t now = time(nullptr);

    stringstream ss(bulletin);

    for (string line; getline(ss, line); ) {
        std::smatch bulletin_entry;

        bool is_match = std::regex_search(line, bulletin_entry, regex_expiration);
        if (is_match) {
            if (bulletin_entry.size() != 2) {
                throw runtime_error(
                        "Incorrect number of matched TAI IETF bulletin expiration");
            }
            const string expiry_data_str(bulletin_entry[1]);
            try {
                const int64_t expiry_unix = std::stoll(expiry_data_str) - NTP_UNIX_OFFSET;

#ifdef TAI_TEST
                etiLog.level(info) << "Bulletin expires in " << expiry_unix - now;
#endif
                ret.expiry = expiry_unix - now;
                ret.offset = parse_ietf_bulletin(bulletin);
                ret.valid = true;
            }
            catch (const invalid_argument& e) {
                etiLog.level(warn) << "Could not parse bulletin: " << e.what();
            }
            catch (const out_of_range&) {
                etiLog.level(warn) << "Parse bulletin: conversion is out of range";
            }
            catch (const runtime_error& e) {
                etiLog.level(warn) << "Parse bulletin: " << e.what();
            }
            break;
        }
    }
    return ret;
}


// callback that receives data from cURL
static size_t fill_bulletin(char *ptr, size_t size, size_t nmemb, void *ctx)
{
    auto *bulletin = reinterpret_cast<stringstream*>(ctx);

    size_t len = size * nmemb;
    for (size_t i = 0; i < len; i++) {
        *bulletin << ptr[i];
    }
    return len;
}

static string download_tai_utc_bulletin(const char* url)
{
    stringstream bulletin;

#ifdef HAVE_CURL
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fill_bulletin);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bulletin);

        res = curl_easy_perform(curl);
        /* always cleanup ! */
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw runtime_error( "TAI-UTC bulletin download failed: " +
                    string(curl_easy_strerror(res)));
        }
    }
    return bulletin.str();
#else
    throw runtime_error("Cannot download TAI Clock information without cURL");
#endif // HAVE_CURL
}

static string load_bulletin_from_file(const char* cache_filename)
{
    int fd = open(cache_filename, O_RDWR); // lockf requires O_RDWR
    if (fd == -1) {
        etiLog.level(error) << "TAI-UTC bulletin open cache for reading: " <<
            strerror(errno);
        return "";
    }

    lseek(fd, 0, SEEK_SET);

    vector<char> buf(1024);
    vector<char> new_bulletin_data;

    ssize_t ret = lockf(fd, F_LOCK, 0);
    if (ret == 0) {
        // exclusive lock acquired

        do {
            ret = read(fd, buf.data(), buf.size());

            if (ret == -1) {
                close(fd);
                etiLog.level(error) << "TAI-UTC bulletin read cache: " <<
                        strerror(errno);
                return "";
            }

            copy(buf.data(), buf.data() + ret, back_inserter(new_bulletin_data));
        } while (ret > 0);

        close(fd);

        return string{new_bulletin_data.data(), new_bulletin_data.size()};
    }
    else {
        etiLog.level(error) <<
            "TAI-UTC bulletin acquire cache lock for reading: " <<
            strerror(errno);
        close(fd);
    }
    return "";
}

ClockTAI::ClockTAI(const std::vector<std::string>& bulletin_urls) :
    RemoteControllable("clocktai")
{
    RC_ADD_PARAMETER(expiry, "Number of seconds until TAI Bulletin expires");

    if (bulletin_urls.empty()) {
        etiLog.level(debug) << "Initialising default TAI Bulletin URLs";
        for (const auto url : default_tai_urls) {
            m_bulletin_urls.push_back(url);
        }
    }
    else {
        etiLog.level(debug) << "Initialising user-configured TAI Bulletin URLs";
        m_bulletin_urls = bulletin_urls;
    }

    for (const auto url : m_bulletin_urls) {
        etiLog.level(info) << "TAI Bulletin URL: '" << url << "'";
    }
}

int ClockTAI::get_valid_offset()
{
    int offset = 0;
    bool offset_valid = false;
    bool refresh_m_bulletin = false;

    std::unique_lock<std::mutex> lock(m_data_mutex);

    const auto state = parse_bulletin(m_bulletin);
    if (state.usable()) {
#if TAI_TEST
        etiLog.level(info) << "Bulletin already valid";
#endif
        offset = state.offset;
        offset_valid = true;

        refresh_m_bulletin = state.expires_soon();
    }
    else {
        refresh_m_bulletin = true;
    }

    if (refresh_m_bulletin) {
        const auto cache_bulletin = load_bulletin_from_file(tai_cache_location);
#if TAI_TEST
        etiLog.level(info) << "Loaded cache bulletin with " <<
            std::count_if(cache_bulletin.cbegin(), cache_bulletin.cend(),
                    [](const char c){ return c == '\n'; }) << " lines";
#endif
        const auto cache_state = parse_bulletin(cache_bulletin);

        if (cache_state.usable()) {
            m_bulletin = cache_bulletin;
            offset = cache_state.offset;
            offset_valid = true;
#if TAI_TEST
            etiLog.level(info) << "Bulletin from cache valid with offset=" << offset;
#endif
        }
        else {
            for (const auto url : m_bulletin_urls) {
                try {
#if TAI_TEST
                    etiLog.level(info) << "Load bulletin from " << url;
#endif
                    const auto new_bulletin = download_tai_utc_bulletin(url.c_str());
                    const auto new_state = parse_bulletin(new_bulletin);
                    if (new_state.usable()) {
                        m_bulletin = new_bulletin;
                        offset = new_state.offset;
                        offset_valid = true;

                        etiLog.level(debug) << "Loaded valid TAI Bulletin from " <<
                            url << " giving offset=" << offset;
                    }
                    else {
                        etiLog.level(debug) << "Skipping invalid TAI bulletin from "
                            << url;
                    }
                }
                catch (const runtime_error& e) {
                    etiLog.level(warn) <<
                        "TAI-UTC offset could not be retrieved from " <<
                        url << " : " << e.what();
                }

                if (offset_valid) {
                    update_cache(tai_cache_location);
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
        throw download_failed();
    }
}


int ClockTAI::get_offset()
{
    using namespace std::chrono;
    const auto time_now = system_clock::now();

    std::unique_lock<std::mutex> lock(m_data_mutex);

    if (not m_offset_valid) {
#ifdef DOWNLOADED_IN_THE_PAST_TEST
        // Assume we've downloaded it in the past:

        m_offset = 37; // Valid in early 2017
        m_offset_valid = true;

        // Simulate requiring a new download
        m_bulletin_refresh_time = time_now - hours(24 * 40);
#else
        // First time we run we must block until we know
        // the offset
        lock.unlock();
        try {
            m_offset = get_valid_offset();
        }
        catch (const download_failed&) {
            throw runtime_error("Unable to download TAI bulletin");
        }
        lock.lock();
        m_offset_valid = true;
        m_bulletin_refresh_time = time_now;
#endif
        etiLog.level(info) <<
            "Initialised TAI-UTC offset to " << m_offset << "s.";
    }

    if (m_bulletin_refresh_time + hours(1) < time_now) {
        // Once per hour, parse the bulletin again, and
        // if necessary trigger a download.
        // Leap seconds are announced several months in advance
        etiLog.level(debug) << "Trying to refresh TAI bulletin";

        if (m_offset_future.valid()) {
            auto state = m_offset_future.wait_for(seconds(0));
            switch (state) {
                case future_status::ready:
                    try {
                        m_offset = m_offset_future.get();
                        m_offset_valid = true;
                        m_bulletin_refresh_time = time_now;

                        etiLog.level(info) <<
                            "Updated TAI-UTC offset to " << m_offset << "s.";
                    }
                    catch (const download_failed&) {
                        etiLog.level(warn) <<
                            "TAI-UTC download failed, will retry in " <<
                            refresh_retry_interval_hours << " hour(s)";

                        m_bulletin_refresh_time += hours(refresh_retry_interval_hours);
                    }
#ifdef DOWNLOADED_IN_THE_PAST_TEST
                    wait_longer = false;
#endif
                    break;

                case future_status::deferred:
                case future_status::timeout:
                    // Not ready yet
#ifdef TAI_TEST
                    etiLog.level(debug) << "  async not ready yet";
#endif
                    break;
            }
        }
        else {
#ifdef TAI_TEST
            etiLog.level(debug) << " Launch async";
#endif
            m_offset_future = async(launch::async, &ClockTAI::get_valid_offset, this);
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

void ClockTAI::update_cache(const char* cache_filename)
{
    int fd = open(cache_filename, O_RDWR | O_CREAT, 00664);
    if (fd == -1) {
        etiLog.level(error) <<
            "TAI-UTC bulletin open cache for writing: " <<
            strerror(errno);
        return;
    }

    lseek(fd, 0, SEEK_SET);

    ssize_t ret = lockf(fd, F_LOCK, 0);
    if (ret == 0) {
        // exclusive lock acquired
        const char *data = m_bulletin.data();
        size_t remaining = m_bulletin.size();

        while (remaining > 0) {
            ret = write(fd, data, remaining);
            if (ret == -1) {
                close(fd);
                etiLog.level(error) <<
                    "TAI-UTC bulletin write cache: " <<
                    strerror(errno);
                return;
            }

            remaining -= ret;
            data += ret;
        }
        etiLog.level(debug) << "TAI-UTC bulletin cache updated";
        close(fd);
    }
    else {
        close(fd);
        etiLog.level(error) <<
            "TAI-UTC bulletin acquire cache lock for writing: " <<
            strerror(errno);
        return;
    }
}


void ClockTAI::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "expiry") {
        throw ParameterError("Parameter '" + parameter +
            "' is read-only in controllable " + get_rc_name());
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
        const int64_t expiry = parse_bulletin(m_bulletin).expiry;
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

