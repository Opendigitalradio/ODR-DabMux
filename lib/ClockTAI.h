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

/* The EDI output needs TAI clock, according to ETSI TS 102 693 Annex F
 * "EDI Timestamps". This module can set the local CLOCK_TAI clock by
 * setting the TAI-UTC offset using adjtimex.
 *
 * This functionality requires Linux 3.10 (30 Jun 2013) or newer.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <vector>
#include "RemoteControl.h"

// EDI needs to know UTC-TAI, but doesn't need the CLOCK_TAI to be set.
// We can keep this code, maybe for future use
#define SUPPORT_SETTING_CLOCK_TAI 0

/* Loads, parses and represents TAI-UTC offset information from the IETF bulletin */
class ClockTAI : public RemoteControllable {
    public:
        ClockTAI(const std::vector<std::string>& bulletin_urls);

        // Fetch the bulletin from the IETF website and return the current
        // TAI-UTC offset.
        // Throws runtime_error on failure.
        int get_offset(void);

#if SUPPORT_SETTING_CLOCK_TAI
        // Update the local TAI clock according to the TAI-UTC offset
        // return 0 on success
        int update_local_tai_clock(int offset);
#endif

    private:
        class download_failed {};

        // Either retrieve the bulletin from the cache or if necessarly
        // download it, and calculate the TAI-UTC offset.
        // Returns the offset or throws download_failed or a range_error
        // if the offset is out of bounds.
        int get_valid_offset(void);

        // Download of new bulletin is done asynchronously
        std::future<int> m_offset_future;

        // Protect all data members, as RC functions are in another thread
        mutable std::mutex m_data_mutex;

        // The currently used TAI-UTC offset
        int m_offset = 0;
        int m_offset_valid = false;

        std::vector<std::string> m_bulletin_urls;

        std::string m_bulletin;
        std::chrono::system_clock::time_point m_bulletin_download_time;

        // Update the cache file with the current m_bulletin
        void update_cache(const char* cache_filename);


        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;
};

