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
        bool m_transmit_programme;
        std::vector<DabComponent*>::iterator componentFIG0_8;
};

}
