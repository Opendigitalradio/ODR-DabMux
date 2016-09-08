/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
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

/* This file downloads the TAI-UTC bulletin from the USNO servers, parses
 * it so that correct time can be communicated in EDI timestamps.
 *
 * This file contains self-test code that can be executed by running
 * g++ -g -Wall -DTEST -DHAVE_CURL -std=c++11 -lcurl -lboost_regex ClockTAI.cpp Log.cpp -o taitest && ./taitest
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
#include <boost/regex.hpp>

/* Last two lines from bulletin, example:
 2015 JUL  1 =JD 2457204.5  TAI-UTC=  36.0       S + (MJD - 41317.) X 0.0      S
 2017 JAN  1 =JD 2457754.5  TAI-UTC=  37.0       S + (MJD - 41317.) X 0.0
 */

ClockTAI::ClockTAI()
{
    m_offset = 0;
}

int ClockTAI::get_offset()
{
    auto time_now = std::chrono::system_clock::now();

    if (time_now - m_bulletin_download_time >
            std::chrono::seconds(3600 * 24 * 31)) {
        // Refresh if it's older than one month. Leap seconds are
        // announced several months in advance

        if (download_tai_utc_bulletin(tai_data_url1) == 0) {
            m_offset = parse_tai_offset();
        }
        else if (download_tai_utc_bulletin(tai_data_url2) == 0) {
            m_offset = parse_tai_offset();
        }
        else {
            // Try again in 1 hour
            m_bulletin_download_time += std::chrono::hours(1);

            throw std::runtime_error("Could not fetch TAI-UTC offset");
        }

        etiLog.level(info) << "Updated TAI-UTC offset to " << m_offset << "s.";
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

int ClockTAI::parse_tai_offset()
{
    boost::regex regex_bulletin("([0-9]{4}) ([A-Z]{3}) +([0-9]+) =JD +[0-9.]+ +TAI-UTC= *([0-9.]+)");
    /*      regex groups:       Year       Month       Day      Julian date            Offset */

    const std::array<std::string, 12> months{"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    /* I'm not certain about the format they would use if the day is a two-digit number. Will they keep
     * two spaces after the month? The regex should be resilient enough in that case.
     */

    time_t now = time(nullptr);
    struct tm *utc_time_now = gmtime(&now);

    const int year = utc_time_now->tm_year + 1900;
    const int month = utc_time_now->tm_mon; // January is 0
    if (month < 0 or month > 11) {
        throw std::runtime_error("Invalid value for month");
    }
    const int day = utc_time_now->tm_mday;


    int tai_utc_offset = 0;

    int linecount = 0;

    /* We cannot just take the last line, because it might
     * be in the future, announcing an upcoming leap second.
     *
     * So we need to look at the current date, and compare it
     * with the date of the leap second.
     */
    for (std::string line; std::getline(m_bulletin, line); ) {
        linecount++;

        boost::smatch bulletin_entry;

        bool is_match = boost::regex_search(line, bulletin_entry, regex_bulletin);
        if (is_match) {

            if (bulletin_entry.size() != 5) {
                throw std::runtime_error("Incorrect number of matched TAI bulletin entries");
            }
            const std::string bulletin_year(bulletin_entry[1]);
            const std::string bulletin_month_name(bulletin_entry[2]);
            const std::string bulletin_day(bulletin_entry[3]);
            const std::string bulletin_offset(bulletin_entry[4]);

            const auto match = std::find(months.begin(), months.end(), bulletin_month_name);
            if (match == months.end()) {
                std::stringstream s;
                s << "Month " << bulletin_month_name << " not in array!";
                std::runtime_error(s.str());
            }

            if (    (std::atoi(bulletin_year.c_str()) < year) or

                    (std::atoi(bulletin_year.c_str()) == year and
                     std::distance(months.begin(), match) < month) or

                    (std::atoi(bulletin_year.c_str()) == year and
                     std::distance(months.begin(), match) == month and
                     std::atoi(bulletin_day.c_str()) < day) ) {
                tai_utc_offset = std::atoi(bulletin_offset.c_str());
            }
#if TEST
            else {
                std::cerr << "Ignoring offset " << bulletin_offset << " at date "
                    << bulletin_year << " " << bulletin_month_name << " " <<
                    bulletin_day << " in the future" << std::endl;
            }
#endif
        }
#if TEST
        else {
            std::cerr << "No match for line " << line << std::endl;
        }
#endif
    }

    if (linecount == 0) {
        throw std::runtime_error("No data in TAI bulletin");
    }

    // With the current evolution of the offset, we're probably going
    // to reach 500 long after DAB gets replaced by another standard.
    if (tai_utc_offset < 0 or tai_utc_offset > 500) {
        std::stringstream ss;
        ss << "TAI offset " << tai_utc_offset << " out of range";
        throw std::range_error(ss.str());
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

int ClockTAI::download_tai_utc_bulletin(const char* url)
{
    int r = 0;

#ifdef HAVE_CURL
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* example.com is redirected, so we tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ClockTAI::fill_bulletin_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

            r = 1;
        }
        else {
            // Save the download time of the bulletin
            m_bulletin_download_time = std::chrono::system_clock::now();
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
#else
    throw std::runtime_error("Cannot download TAI Clock information without cURL");
#endif // HAVE_CURL
    return r;
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

    try {
        cerr << "Offset is " << tai.get_offset() << endl;
    }
    catch (std::exception &e) {
        cerr << "Exception " << e.what() << endl;
    }

    return 0;
}
#endif

