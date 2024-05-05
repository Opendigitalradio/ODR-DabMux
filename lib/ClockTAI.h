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

/* The EDI output needs TAI clock, according to ETSI TS 102 693 Annex F
 * "EDI Timestamps". This module can set the local CLOCK_TAI clock by
 * setting the TAI-UTC offset using adjtimex.
 *
 * This functionality requires Linux 3.10 (30 Jun 2013) or newer.
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <vector>
#include <optional>
#include <variant>

#include "RemoteControl.h"

// EDI needs to know UTC-TAI, but doesn't need the CLOCK_TAI to be set.
// We can keep this code, maybe for future use
#define SUPPORT_SETTING_CLOCK_TAI 0

struct BulletinState {
    bool valid = false;
    int64_t expires_at = 0;
    int offset = 0;

    int64_t expires_in() const;
    bool usable() const;
    bool expires_soon() const;
};

class Bulletin {
    public:
        static Bulletin download_from_url(const char *url);
        static Bulletin create_with_fixed_offset(int offset);
        static Bulletin load_from_file(const char *cache_filename);

        void clear_expiry_if_overridden();

        void store_to_cache(const char* cache_filename) const;

        std::string get_source() const { return source; }
        BulletinState state() const;
    private:
        // URL or file path from which the bulletin has been/will be loaded
        std::string source;

        struct OverrideData {
            int offset = 0;
            int expires_at = 0;
        };
        // string: A cache of the bulletin, or empty string if not loaded
        // int: A manually overridden offset
        std::variant<std::string, OverrideData> bulletin_or_override;
};

/* Loads, parses and represents TAI-UTC offset information from the IETF bulletin */
class ClockTAI
#if ENABLE_REMOTECONTROL
: public RemoteControllable
#endif // ENABLE_REMOTECONTROL
{
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
        BulletinState get_valid_offset(void);

        // Download of new bulletin is done asynchronously
        std::future<BulletinState> m_offset_future;

        // Protect all data members, as RC functions are in another thread
        mutable std::mutex m_data_mutex;

        std::vector<std::string> m_bulletin_urls;

        Bulletin m_bulletin;

        // The currently used TAI-UTC offset, extracted the bulletin and cached
        // here to avoid having to parse the bulletin all the time
        std::optional<BulletinState> m_state;
        std::chrono::steady_clock::time_point m_state_last_updated;

#if ENABLE_REMOTECONTROL
    public:
        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

        virtual const json::map_t get_all_values() const;
#endif // ENABLE_REMOTECONTROL
};

