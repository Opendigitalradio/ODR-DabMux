/*
   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   EDI output.
    This defines a few TAG items as defined ETSI TS 102 821 and
    ETSI TS 102 693

   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#ifndef _TAGITEMS_H_
#define _TAGITEMS_H_

#include "config.h"
#include "Eti.h"
#include <vector>
#include <string>
#include <stdint.h>
#define PACKED __attribute__ ((packed))

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
        uint8_t fcth;
        uint8_t fct;

        // ATST (optional)
        bool atstf; // presence of atst data
        uint8_t utco;
        uint32_t seconds;
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
        TagESTn(uint8_t id) : id_(id) {} ;

        std::vector<uint8_t> Assemble();

        // SSTCn
        uint8_t scid;
        uint8_t sad;
        uint8_t tpl;
        uint8_t rfa;

        // Pointer to MSTn data
        uint8_t* mst_data;
        size_t mst_length; // STLn * 8 bytes

    private:
        uint8_t id_;
};
#endif

