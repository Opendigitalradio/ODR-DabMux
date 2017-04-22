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

#pragma once

#include <cstdint>
#include <vector>

namespace FIC {

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
        std::vector<DabSubchannel*>           subchannels;
        std::vector<DabSubchannel*>::iterator subchannelFIG0_1;

        uint8_t m_watermarkData[128];
        size_t  m_watermarkSize;
        size_t  m_watermarkPos;
};

}
