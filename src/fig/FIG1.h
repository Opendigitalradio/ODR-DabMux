/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#ifndef __FIG1_H_
#define __FIG1_H_

#include <cstdint>

#include "fig/FIG.h"

namespace FIC {

// FIG type 1/0, Multiplex Configuration Info (MCI),
// Ensemble information
class FIG1_0 : public IFIG
{
    public:
        FIG1_0(FIGRuntimeInformation* rti) :
            m_rti(rti) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 1; }
        virtual int figextension() const { return 0; }

    private:
        FIGRuntimeInformation *m_rti;
};

// FIG type 1/1, programme service label
class FIG1_1 : public IFIG
{
    public:
        FIG1_1(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 1; }
        virtual int figextension() const { return 1; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_service::iterator service;
};

// FIG type 1/4, service component label
class FIG1_4 : public IFIG
{
    public:
        FIG1_4(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 1; }
        virtual int figextension() const { return 4; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_component::iterator component;
};

// FIG type 1/5, data service label
class FIG1_5 : public IFIG
{
    public:
        FIG1_5(FIGRuntimeInformation* rti) :
            m_rti(rti), m_initialised(false) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::B; }

        virtual int figtype() const { return 1; }
        virtual int figextension() const { return 5; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        vec_sp_service::iterator service;
};

#ifdef _WIN32
#   pragma pack(push)
#endif

struct FIGtype1_0 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;

    uint16_t EId;
} PACKED;


struct FIGtype1_1 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;

    uint16_t Sld;
} PACKED;


struct FIGtype1_5 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint32_t SId;
} PACKED;


struct FIGtype1_4_programme {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint16_t SId;
} PACKED;


struct FIGtype1_4_data {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:3;
    uint8_t OE:1;
    uint8_t Charset:4;
    uint8_t SCIdS:4;
    uint8_t rfa:3;
    uint8_t PD:1;
    uint32_t SId;
} PACKED;


#ifdef _WIN32
#   pragma pack(pop)
#endif

} // namespace FIC

#endif // __FIG1_H_

