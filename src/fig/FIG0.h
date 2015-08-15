/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
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

#ifndef __FIG0_H_
#define __FIG0_H_

#include <cstdint>

#include "fig/FIG.h"

namespace FIC {

// FIG type 0/0, Multiplex Configuration Info (MCI),
// Ensemble information
class FIG0_0 : public IFIG
{
    public:
        FIG0_0(FIGRuntimeInformation* rti) :
            m_rti(rti) {}
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::FIG0_0; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 0; }

    private:
        FIGRuntimeInformation *m_rti;
};

// FIG type 0/1, MIC, Sub-Channel Organization,
// one instance of the part for each subchannel
class FIG0_1 : public IFIG
{
    public:
        FIG0_1(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 1; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<dabSubchannel*>::iterator subchannelFIG0_1;
};

// FIG type 0/2, MCI, Service Organization, one instance of
// FIGtype0_2_Service for each subchannel
class FIG0_2 : public IFIG
{
    public:
        FIG0_2(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 2; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator serviceFIG0_2;
};

// FIG type 0/3
// The Extension 3 of FIG type 0 (FIG 0/3) gives additional information about
// the service component description in packet mode.
class FIG0_3 : public IFIG
{
    public:
        FIG0_3(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 3; }

    private:
        FIGRuntimeInformation *m_rti;
};

// FIG type 0/8
// The Extension 8 of FIG type 0 (FIG 0/8) provides information to link
// together the service component description that is valid within the ensemble
// to a service component description that is valid in other ensembles
class FIG0_8 : public IFIG
{
    public:
        FIG0_8(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 8; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<DabComponent*>::iterator componentFIG0_8;
};

// FIG type 0/9
// The Country, LTO and International table feature defines the local time
// offset, the International Table and the Extended Country Code (ECC)
class FIG0_9 : public IFIG
{
    public:
        FIG0_9(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 9; }

    private:
        FIGRuntimeInformation *m_rti;
};


// FIG type 0/10
// The date and time feature is used to signal a location-independent timing
// reference in UTC format.
class FIG0_10 : public IFIG
{
    public:
        FIG0_10(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 10; }

    private:
        FIGRuntimeInformation *m_rti;

        uint8_t m_watermarkData[128];
        size_t  m_watermarkSize;
        size_t  m_watermarkPos;
};

// FIG type 0/13
// User Application Information
class FIG0_13 : public IFIG
{
    public:
        FIG0_13(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 13; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        bool m_transmit_programme;
        std::vector<DabComponent*>::iterator componentFIG0_13;
};

// FIG type 0/17
class FIG0_17 : public IFIG
{
    public:
        FIG0_17(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A_B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 17; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator serviceFIG0_17;
};

// FIG type 0/18
class FIG0_18 : public IFIG
{
    public:
        FIG0_18(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::B; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 18; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator service;
};

// FIG type 0/19
class FIG0_19 : public IFIG
{
    public:
        FIG0_19(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate(void) { return FIG_rate::A; }

        virtual const int figtype(void) const { return 0; }
        virtual const int figextension(void) const { return 19; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        std::vector<std::shared_ptr<DabService> >::iterator service;
};

} // namespace FIC

#endif // __FIG0_H_

