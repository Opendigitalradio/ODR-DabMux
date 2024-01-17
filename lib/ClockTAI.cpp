/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2024
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

#include <iterator>
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "ClockTAI.h"
#include "Log.h"

#include <ctime>
#include <cstdio>
#include <cstring>
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

constexpr int refresh_retry_interval_hours = 1;

// Offset between NTP time and POSIX time:
// timestamp_unix = timestamp_ntp - NTP_UNIX_OFFSET
constexpr int64_t NTP_UNIX_OFFSET = 2208988800L;

// leap seconds insertion bulletin was previously available from the IETF and in the TZ
// distribution, but in late 2023 IETF stopped serving the file.
static array<const char*, 1> default_tai_urls = {
    "https://raw.githubusercontent.com/eggert/tz/master/leap-seconds.list",
};

// According to the Filesystem Hierarchy Standard, the data in
// /var/tmp "must not be deleted when the system is booted."
static const char *tai_cache_location = "/var/tmp/odr-leap-seconds.cache";

static string join_string_with_pipe(const vector<string>& vec)
{
    stringstream ss;
    for (auto it = vec.cbegin(); it != vec.cend(); ++it) {
        ss << *it;
        if (it + 1 != vec.cend()) {
            ss << "|";
        }
    }
    return ss.str();
}

static vector<string> split_pipe_separated_string(const string& s)
{
    stringstream ss;
    ss << s;

    string elem;
    vector<string> components;
    while (getline(ss, elem, '|')) {
        components.push_back(elem);
    }
    return components;
}


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

    // With the current evolution of the offset, we're probably going
    // to reach 500 long after DAB gets replaced by another standard.
    // Or maybe leap seconds get abolished first...
    if (tai_utc_offset < 0 or tai_utc_offset > 500) {
        throw runtime_error("Unreasonable TAI-UTC offset calculated");
    }

    return tai_utc_offset;
}

int64_t BulletinState::expires_in() const {
    time_t now = time(nullptr);
    return expires_at - now;
}

bool BulletinState::usable() const {
    return valid and expires_in() > 0;
}

bool BulletinState::expires_soon() const {
    constexpr int64_t MONTH = 3600 * 24 * 30;
    return usable() and expires_in() < 1 * MONTH;
}

BulletinState Bulletin::state() const
{
    // The bulletin contains one line that specifies an expiration date
    // in NTP time. If that point in time is in the future, we consider
    // the bulletin valid.
    //
    // The entry looks like this:
    //#@	3707596800

    BulletinState ret;

    if (std::holds_alternative<Bulletin::OverrideData>(bulletin_or_override)) {
        const auto& od = std::get<Bulletin::OverrideData>(bulletin_or_override);
        ret.offset = od.offset;
        ret.expires_at = od.expires_at;
        ret.valid = true;
#ifdef TAI_TEST
        etiLog.level(debug) << "state() from Override!";
#endif
        return ret;
    }

    const auto& bulletin = std::get<string>(bulletin_or_override);

    std::regex regex_expiration(R"(#@\s+([0-9]+))");

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
                {
                    // Do not use `now` for anything else but debugging, otherwise it
                    // breaks the cache.
                    time_t now = time(nullptr);
                    etiLog.level(debug) << "Bulletin " <<
                        get_source() << " expires in " << expiry_unix - now;
                }
#endif
                ret.expires_at = expiry_unix;
                ret.offset = parse_ietf_bulletin(bulletin);
                ret.valid = true;
            }
            catch (const invalid_argument& e) {
                etiLog.level(warn) << "Could not parse bulletin from " <<
                    get_source() << ": " << e.what();
            }
            catch (const out_of_range&) {
                etiLog.level(warn) << "Parse bulletin from " <<
                    get_source() << ": conversion is out of range";
            }
            catch (const runtime_error& e) {
                etiLog.level(warn) << "Parse bulletin from " <<
                    get_source() << ": " << e.what();
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

Bulletin Bulletin::download_from_url(const char* url)
{
    stringstream bulletin_data;

#ifdef HAVE_CURL
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fill_bulletin);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bulletin_data);

        res = curl_easy_perform(curl);
        /* always cleanup ! */
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw runtime_error( "TAI-UTC bulletin download failed: " +
                    string(curl_easy_strerror(res)));
        }
    }
    Bulletin bulletin;
    bulletin.source = url;
    bulletin.bulletin_or_override = bulletin_data.str();
    return bulletin;
#else
    throw runtime_error("Cannot download TAI Clock information without cURL");
#endif // HAVE_CURL
}

Bulletin Bulletin::create_with_fixed_offset(int offset)
{
    Bulletin bulletin;
    bulletin.source = "manual override";

    OverrideData od;
    od.offset = offset;
    time_t now = time(nullptr);
    // 10 years is probably equivalent to infinity in this case...
    od.expires_at = now + 10L * 365 * 24 * 3600;
    bulletin.bulletin_or_override = od;
    return bulletin;
}

Bulletin Bulletin::load_from_file(const char* cache_filename)
{
    Bulletin bulletin;
    bulletin.source = cache_filename;

    int fd = open(cache_filename, O_RDWR); // lockf requires O_RDWR
    if (fd == -1) {
        etiLog.level(error) << "TAI-UTC bulletin open cache for reading: " <<
            strerror(errno);
        return bulletin;
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
                return bulletin;
            }

            copy(buf.data(), buf.data() + ret, back_inserter(new_bulletin_data));
        } while (ret > 0);

        close(fd);

        bulletin.bulletin_or_override = string{new_bulletin_data.data(), new_bulletin_data.size()};
    }
    else {
        etiLog.level(error) <<
            "TAI-UTC bulletin acquire cache lock for reading: " <<
            strerror(errno);
        close(fd);
    }
    return bulletin;
}

void Bulletin::clear_expiry_if_overridden()
{
    if (std::holds_alternative<Bulletin::OverrideData>(bulletin_or_override)) {
        auto& od = std::get<Bulletin::OverrideData>(bulletin_or_override);
        time_t now = time(nullptr);
        od.expires_at = now;
    }
}

ClockTAI::ClockTAI(const std::vector<std::string>& bulletin_urls) :
    RemoteControllable("clocktai")
{
    RC_ADD_PARAMETER(tai_utc_offset, "TAI-UTC offset");
    RC_ADD_PARAMETER(expiry, "Number of seconds until TAI Bulletin expires");
    RC_ADD_PARAMETER(expires_at, "UNIX timestamp when TAI Bulletin expires");
    RC_ADD_PARAMETER(url, "URLs used to fetch the bulletin, separated by pipes");

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

    etiLog.level(debug) << "ClockTAI uses bulletin URL: '" << join_string_with_pipe(m_bulletin_urls) << "'";
}

BulletinState ClockTAI::get_valid_offset()
{
    std::unique_lock<std::mutex> lock(m_data_mutex);

    const auto state = m_bulletin.state();
#if TAI_TEST
    etiLog.level(info) << "TAI get_valid_offset STEP 1 ";
    etiLog.level(info) << " " << m_bulletin.get_source() << " " <<
        state.valid << " " << state.usable() << " " << state.expires_in();
#endif
    if (state.usable()) {
        return state;
    }

#if TAI_TEST
    etiLog.level(info) << "TAI get_valid_offset STEP 2";
#endif
    const auto cache_bulletin = Bulletin::load_from_file(tai_cache_location);
    const auto cache_state = cache_bulletin.state();
    if (cache_state.usable()) {
        m_bulletin = cache_bulletin;
#if TAI_TEST
        etiLog.level(info) << "TAI get_valid_offset STEP 2 take cache";
#endif
        return cache_state;
    }

#if TAI_TEST
    etiLog.level(info) << "TAI get_valid_offset STEP 3";
#endif

    vector<Bulletin> bulletins({m_bulletin, cache_bulletin});

    for (const auto& url : m_bulletin_urls) {
        try {
#if TAI_TEST
            etiLog.level(info) << "Load bulletin from " << url;
#endif
            const auto new_bulletin = Bulletin::download_from_url(url.c_str());
            bulletins.push_back(new_bulletin);

            const auto new_state = new_bulletin.state();
            if (new_state.usable()) {
                m_bulletin = new_bulletin;
                new_bulletin.store_to_cache(tai_cache_location);

                etiLog.level(debug) << "Loaded valid TAI Bulletin from " <<
                    url << " giving offset=" << new_state.offset;
                return new_state;
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
    }

#if TAI_TEST
    etiLog.level(info) << "TAI get_valid_offset STEP 4";
#endif

    // Maybe we have a valid but expired bulletin available.
    // Place bulletins with largest expiry first
    std::sort(bulletins.begin(), bulletins.end(),
            [](const Bulletin& a, const Bulletin& b) {
            return a.state().expires_at > b.state().expires_at; });

    for (const auto& bulletin : bulletins) {
        const auto& state = bulletin.state();
        if (state.valid) {
            etiLog.level(warn) << "Taking TAI-UTC offset from expired bulletin from " <<
                bulletin.get_source() << " : " << state.offset << "s expired " <<
                state.expires_in() << "s ago";
            m_bulletin = bulletin;
            return state;
        }
    }

    throw download_failed();
}


int ClockTAI::get_offset()
{
    using namespace std::chrono;
    const auto time_now = steady_clock::now();

    std::unique_lock<std::mutex> lock(m_data_mutex);

    if (not m_state.has_value()) {
        // First time we run we must block until we know
        // the offset
        lock.unlock();
        try {
            m_state = get_valid_offset();
        }
        catch (const download_failed&) {
            throw runtime_error("Unable to download TAI bulletin");
        }
        lock.lock();
        m_state_last_updated = time_now;
        etiLog.level(info) << "Initialised TAI-UTC offset to " << m_state->offset << "s.";
    }

    if (m_state_last_updated + hours(1) < time_now) {
        // Once per hour, parse the bulletin again, and
        // if necessary trigger a download.
        // Leap seconds are announced several months in advance
        etiLog.level(debug) << "Trying to refresh TAI bulletin";

        if (m_offset_future.valid()) {
            auto state = m_offset_future.wait_for(seconds(0));
            switch (state) {
                case future_status::ready:
                    try {
                        m_state = m_offset_future.get();
                        m_state_last_updated = time_now;

                        etiLog.level(info) <<
                            "Updated TAI-UTC offset to " << m_state->offset << "s.";
                    }
                    catch (const download_failed&) {
                        etiLog.level(warn) <<
                            "TAI-UTC download failed, will retry in " <<
                            refresh_retry_interval_hours << " hour(s)";

#if TAI_TEST
                        m_state_last_updated += seconds(11);
#else
                        m_state_last_updated += hours(refresh_retry_interval_hours);
#endif
                    }
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

    if (m_state) {
        return m_state->offset;
    }
    throw std::logic_error("ClockTAI: No valid m_state at end of get_offset()");
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

void Bulletin::store_to_cache(const char* cache_filename) const
{
    if (not std::holds_alternative<string>(bulletin_or_override)) {
        etiLog.level(error) << "ClockTAI: Cannot store an artificial bulletin to cache!";
    }
    const auto& bulletin = std::get<string>(bulletin_or_override);

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
        const char *data = bulletin.data();
        size_t remaining = bulletin.size();

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
    if (parameter == "expiry" or parameter == "expires_at") {
        throw ParameterError("Parameter '" + parameter +
            "' is read-only in controllable " + get_rc_name());
    }
    else if (parameter == "tai_utc_offset") {
        const auto offset = std::stoi(value);
        auto b = Bulletin::create_with_fixed_offset(offset);

        etiLog.level(warn) << "ClockTAI: manually overriding UTC-TAI offset to " << offset;

        std::unique_lock<std::mutex> lock(m_data_mutex);
        m_bulletin = b;
        m_state = b.state();
        m_state_last_updated = chrono::steady_clock::now();
    }
    else if (parameter == "url") {
        {
            std::unique_lock<std::mutex> lock(m_data_mutex);
            m_bulletin_urls = split_pipe_separated_string(value);
            m_state_last_updated = chrono::steady_clock::time_point::min();

            // Setting URL expires the bulletin, if it was manually overridden,
            // so that the selection logic doesn't prefer it
            m_bulletin.clear_expiry_if_overridden();
        }
        etiLog.level(info) << "ClockTAI: triggering a reload from URLs...";
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
        return to_string(m_bulletin.state().expires_in());
    }
    else if (parameter == "expires_at") {
        std::unique_lock<std::mutex> lock(m_data_mutex);
        return to_string(m_bulletin.state().expires_at);
    }
    else if (parameter == "tai_utc_offset") {
        std::unique_lock<std::mutex> lock(m_data_mutex);
        if (m_state) {
            return to_string(m_state->offset);
        }
        throw ParameterError("Parameter '" + parameter +
                "' has no current value" + get_rc_name());
    }
    else if (parameter == "url") {
        return join_string_with_pipe(m_bulletin_urls);;
    }
    else {
        throw ParameterError("Parameter '" + parameter +
            "' is not exported by controllable " + get_rc_name());
    }
}

const json::map_t ClockTAI::get_all_values() const
{
    json::map_t stat;
    std::unique_lock<std::mutex> lock(m_data_mutex);

    const auto& state = m_bulletin.state();

#if CLOCK_TAI
    etiLog.level(debug) << "CALC FROM m_bulletin: " << state.valid << " " <<
        state.offset << " " << state.expires_at << " -> " << state.expires_in();
    etiLog.level(debug) << "CACHED IN m_state:    " << m_state->valid << " " <<
        m_state->offset << " " << m_state->expires_at << " -> " << m_state->expires_in();
#endif

    stat["tai_utc_offset"].v = state.offset;

    stat["expiry"].v = state.expires_in(); // Might be negative when expired or 0 when invalid
    if (state.valid) {
        stat["expires_at"].v = state.expires_at;
    }
    else {
        stat["expires_at"].v = nullopt;
    }

    stat["url"].v = join_string_with_pipe(m_bulletin_urls);

    return stat;
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

