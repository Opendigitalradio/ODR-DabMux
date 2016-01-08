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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "ClockTAI.h"

#include <stdio.h>
#include <errno.h>
#include <sys/timex.h>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <regex>

/* Last line from bulletin, example:
 2015 JUL  1 =JD 2457204.5  TAI-UTC=  36.0       S + (MJD - 41317.) X 0.0      S
 */

ClockTAI::ClockTAI()
{
    m_offset = 0;
    m_bulletin_download_time.tv_nsec = 0;
    m_bulletin_download_time.tv_sec = 0;
}

int ClockTAI::get_offset()
{
    struct timespec time_now;

    int err = clock_gettime(CLOCK_REALTIME, &time_now);
    if (err) {
        throw std::runtime_error("ClockTAI::get_offset() clock_gettime failed");
    }

    if (time_now.tv_sec - m_bulletin_download_time.tv_sec >
            3600 * 24 * 31) {
        // Refresh if it's older than one month. Leap seconds are
        // announced several months in advance

        if (download_tai_utc_bulletin(tai_data_url1) == 0) {
            m_offset = parse_tai_offset();
        }
        else if (download_tai_utc_bulletin(tai_data_url2) == 0) {
            m_offset = parse_tai_offset();
        }
    }

    return m_offset;

    throw std::runtime_error("Could not fetch TAI-UTC offset");
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
    std::regex regex_offset("TAI-UTC= *([0-9.]+)");

    int tai_utc_offset = 0;

    int linecount = 0;

    for (std::string line; std::getline(m_bulletin, line); ) {
        linecount++;
        auto words_begin =
            std::sregex_token_iterator(
                    line.begin(), line.end(), regex_offset, 1);
        auto words_end = std::sregex_token_iterator();

        for (auto w = words_begin; w != words_end; ++w) {
            std::string s(*w);
            tai_utc_offset = std::atoi(s.c_str());
        }

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
            int err = clock_gettime(CLOCK_REALTIME, &m_bulletin_download_time);
            if (err) {
                perror("REALTIME clock_gettime failed");
                r = 1;
            }
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
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

