/*
   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output.
    This defines a few TAG items as defined ETSI TS 102 821 and
    ETSI TS 102 693

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

#pragma once

#include "config.h"
#include "Eti.h"
#include <vector>
#include <chrono>
#include <string>
#include <stdint.h>

class TagItem
{
    public:
        virtual std::vector<uint8_t> Assemble() = 0;
};

// ETSI TS 102 693, 5.1.1 Protocol type and revision
class TagStarPTR : public TagItem
{
    public:
        std::vector<uint8_t> Assemble();
};

// ETSI TS 102 693, 5.1.3 DAB ETI(LI) Management (deti)
class TagDETI : public TagItem
{
    public:
        TagDETI()
        {
            // set optional fields to "not present"
            atstf = 0;
            rfudf = 0;
            ficf = 0;

            rfa = 0; //shall be zero
            rfu = 0; //mnsc is valid
        }
        std::vector<uint8_t> Assemble();

        /***** DATA in intermediary format ****/
        // For the ETI Header: must be defined !
        uint8_t stat;
        uint8_t mid;
        uint8_t fp;
        uint8_t rfa;
        uint8_t rfu;
        uint16_t mnsc;
        uint16_t dflc; // modulo 5000 frame counter

        // ATST (optional)
        bool atstf; // presence of atst data

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
        uint8_t utco;

        void set_utco(int tai_utc_offset) { utco = tai_utc_offset - 32; }

        /* The number of SI seconds since 2000-01-01 T 00:00:00 UTC as an
         * unsigned 32-bit quantity
         */
        uint32_t seconds;

        void set_seconds(std::chrono::system_clock::time_point t);

        /* TSTA: Shall be the 24 least significant bits of the Time Stamp
         * (TIST) field from the STI-D(LI) Frame. The full definition for the
         * STI TIST can be found in annex B of EN 300 797 [4]. The most
         * significant 8 bits of the TIST field of the incoming STI-D(LI)
         * frame, if required, may be carried in the RFAD field.
         */
        uint32_t tsta;

        // the FIC (optional)
        bool ficf;
        const unsigned char* fic_data;
        size_t fic_length;

        // rfu
        bool rfudf;
        uint32_t rfud;


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

        uint8_t id;
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

