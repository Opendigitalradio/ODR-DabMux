/*
   EDI output.
    This defines a few TAG items as defined ETSI TS 102 821 and
    ETSI TS 102 693

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

#pragma once

#include <vector>
#include <array>
#include <chrono>
#include <string>
#include <cstdint>

namespace edi {

class TagItem
{
    public:
        virtual std::vector<uint8_t> Assemble() = 0;
};

// ETSI TS 102 693, 5.1.1 Protocol type and revision
class TagStarPTR : public TagItem
{
    public:
        TagStarPTR(const std::string& protocol);
        std::vector<uint8_t> Assemble();

    private:
        std::string m_protocol = "";
};

// ETSI TS 102 693, 5.1.3 DAB ETI(LI) Management (deti)
class TagDETI : public TagItem
{
    public:
        std::vector<uint8_t> Assemble();

        /***** DATA in intermediary format ****/
        // For the ETI Header: must be defined !
        uint8_t stat = 0;
        uint8_t mid = 0;
        uint8_t fp = 0;
        uint8_t rfa = 0;
        uint8_t rfu = 0; // MNSC is valid
        uint16_t mnsc = 0;
        uint16_t dlfc = 0; // modulo 5000 frame counter

        // ATST (optional)
        bool atstf = false; // presence of atst data

        /* UTCO: Offset (in seconds) between UTC and the Seconds value. The
         * value is expressed as an unsigned 8-bit quantity. As of February
         * 2009, the value shall be 2 and shall change as a result of each
         * modification of the number of leap seconds, as proscribed by
         * International Earth Rotation and Reference Systems Service (IERS).
         *
         * According to Annex F
         *  EDI = TAI - 32s (constant)
         *  EDI = UTC + UTCO
         * we derive
         *  UTCO = TAI-UTC - 32
         * where the TAI-UTC offset is given by the USNO bulletin using
         * the ClockTAI module.
         */
        uint8_t utco = 0;

        /* Update the EDI time. t is in UTC */
        void set_edi_time(const std::time_t t, int tai_utc_offset);

        /* The number of SI seconds since 2000-01-01 T 00:00:00 UTC as an
         * unsigned 32-bit quantity. Contrary to POSIX, this value also
         * counts leap seconds.
         */
        uint32_t seconds = 0;

        /* TSTA: Shall be the 24 least significant bits of the Time Stamp
         * (TIST) field from the STI-D(LI) Frame. The full definition for the
         * STI TIST can be found in annex B of EN 300 797 [4]. The most
         * significant 8 bits of the TIST field of the incoming STI-D(LI)
         * frame, if required, may be carried in the RFAD field.
         */
        uint32_t tsta = 0xFFFFFF;

        // the FIC (optional)
        bool ficf = false;
        const unsigned char* fic_data;
        size_t fic_length;

        // rfu
        bool rfudf = false;
        uint32_t rfud = 0;


};

// ETSI TS 102 693, 5.1.5 ETI Sub-Channel Stream <n>
class TagESTn : public TagItem
{
    public:
        std::vector<uint8_t> Assemble();

        // SSTCn
        uint8_t  scid;
        uint16_t sad;
        uint8_t  tpl;
        uint8_t  rfa;

        // Pointer to MSTn data
        uint8_t* mst_data;
        size_t mst_length; // STLn * 8 bytes

        // id is 1-indexed.
        uint8_t id = 1;
};

// ETSI TS 102 693, 5.1.2 DAB STI-D(LI) Management
class TagDSTI : public TagItem
{
    public:
        std::vector<uint8_t> Assemble();

        // dsti Header
        bool stihf = false;
        bool atstf = false; // presence of atst data
        bool rfadf = false;
        uint16_t dlfc = 0; // modulo 5000 frame counter

        // STI Header (optional)
        uint8_t stat = 0;
        uint16_t spid = 0;

        /* UTCO: Offset (in seconds) between UTC and the Seconds value. The
         * value is expressed as an unsigned 8-bit quantity. As of February
         * 2009, the value shall be 2 and shall change as a result of each
         * modification of the number of leap seconds, as proscribed by
         * International Earth Rotation and Reference Systems Service (IERS).
         *
         * According to Annex F
         *  EDI = TAI - 32s (constant)
         *  EDI = UTC + UTCO
         * we derive
         *  UTCO = TAI-UTC - 32
         * where the TAI-UTC offset is given by the USNO bulletin using
         * the ClockTAI module.
         */
        uint8_t utco = 0;

        /* Update the EDI time. t is in UTC */
        void set_edi_time(const std::time_t t, int tai_utc_offset);

        /* The number of SI seconds since 2000-01-01 T 00:00:00 UTC as an
         * unsigned 32-bit quantity. Contrary to POSIX, this value also
         * counts leap seconds.
         */
        uint32_t seconds = 0;

        /* TSTA: Shall be the 24 least significant bits of the Time Stamp
         * (TIST) field from the STI-D(LI) Frame. The full definition for the
         * STI TIST can be found in annex B of EN 300 797 [4]. The most
         * significant 8 bits of the TIST field of the incoming STI-D(LI)
         * frame, if required, may be carried in the RFAD field.
         */
        uint32_t tsta = 0xFFFFFF;

        std::array<uint8_t, 9> rfad;

    private:
        int tai_offset_cache = 0;
        std::time_t tai_offset_cache_updated_at = 0;
};

// ETSI TS 102 693, 5.1.4 STI-D Payload Stream <m>
class TagSSm : public TagItem
{
    public:
        std::vector<uint8_t> Assemble();

        // SSTCn
        uint8_t rfa = 0;
        uint8_t tid = 0; // See EN 300 797, 5.4.1.1. Value 0 means "MSC sub-channel"
        uint8_t tidext = 0; // EN 300 797, 5.4.1.3, Value 0 means "MSC audio stream"
        bool crcstf = false;
        uint16_t stid = 0;

        // Pointer to ISTDm data
        const uint8_t *istd_data;
        size_t istd_length; // bytes

        // id is 1-indexed.
        uint16_t id = 1;
};

// ETSI TS 102 821, 5.2.2.2 Dummy padding
class TagStarDMY : public TagItem
{
    public:
        /* length is the TAG value length in bytes */
        TagStarDMY(uint32_t length) : length_(length) {}
        std::vector<uint8_t> Assemble();

    private:
        uint32_t length_;
};

// Custom TAG that carries version information of the EDI source
class TagODRVersion : public TagItem
{
    public:
        TagODRVersion(const std::string& version, uint32_t uptime_s);
        std::vector<uint8_t> Assemble();

    private:
        std::string m_version;
        uint32_t m_uptime;
};

// Custom TAG that carries audio level metadata
class TagODRAudioLevels : public TagItem
{
    public:
        TagODRAudioLevels(int16_t audiolevel_left, int16_t audiolevel_right);
        std::vector<uint8_t> Assemble();

    private:
        int16_t m_audio_left;
        int16_t m_audio_right;
};

}

