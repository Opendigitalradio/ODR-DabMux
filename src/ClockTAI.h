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

/* The EDI output needs TAI clock, according to ETSI TS 102 693 Annex F
 * "EDI Timestamps". This module sets the local CLOCK_TAI clock by
 * setting the TAI-UTC offset using adjtimex.
 *
 * This functionality requires Linux 3.10 (30 Jun 2013) or newer */

#ifndef __CLOCK_TAI_H_
#define __CLOCK_TAI_H_

#include <stdint.h>
#include <stdlib.h>
#include <sstream>
#include <chrono>

// EDI needs to know UTC-TAI, but doesn't need the CLOCK_TAI to be set.
// We can keep this code, maybe for future use
#define SUPPORT_SETTING_CLOCK_TAI 0

/* Loads, parses and represents TAI-UTC offset information from the USNO bulletin */
class ClockTAI {
    public:
        ClockTAI();

        // Fetch the bulletin from the USNO website and return the current
        // TAI-UTC offset.
        // Throws runtime_error on failure.
        int get_offset(void);

#if SUPPORT_SETTING_CLOCK_TAI
        // Update the local TAI clock according to the TAI-UTC offset
        // return 0 on success
        int update_local_tai_clock(int offset);
#endif

    private:
        // leap seconds insertion bulletin is available at
        const char* tai_data_url1 = "http://maia.usno.navy.mil/ser7/tai-utc.dat";
        const char* tai_data_url2 = "http://toshi.nofs.navy.mil/ser7/tai-utc.dat";

        int m_offset;
        std::stringstream m_bulletin;
        std::chrono::system_clock::time_point m_bulletin_download_time;

        // Load bulletin into m_bulletin, return 0 on success
        int download_tai_utc_bulletin(const char* url);

        // read TAI offset from m_bulletin
        int parse_tai_offset(void);

        // callback that receives data from cURL
        size_t fill_bulletin(char *ptr, size_t size, size_t nmemb);

        // static callback wrapper for cURL
        static size_t fill_bulletin_cb(
                char *ptr, size_t size, size_t nmemb, void *ctx);
};

#endif // __CLOCK_TAI_H_

