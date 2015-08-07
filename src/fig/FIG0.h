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

} // namespace FIC

#endif // __FIG0_H_

