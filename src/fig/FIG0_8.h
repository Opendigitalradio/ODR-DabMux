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
#include "fig/FIG.h"

namespace FIC {

// FIG type 0/8
// The Extension 8 of FIG type 0 (FIG 0/8) provides information to link
// together the service component description that is valid within the ensemble
// to a service component description that is valid in other ensembles
// Selects which service components an FIG0_8 instance carries.
//  - Both:      legacy behaviour, alternates programme then data passes from a
//               single instance, completing the full database every second loop.
//  - Programme: carries only programme service components, completing on each loop.
//  - Data:      carries only data service components, completing on each loop.
// The split modes let the priority carousel run programme components at rate A
// (group A MCI, 96ms) and data components at rate B, as permitted by EN 300 401
// clause 6.1, while keeping each instance single-pass so cycle timing is exact.
enum class FIG0_8_mode { Both, Programme, Data };

class FIG0_8 : public IFIG
{
    public:
        FIG0_8(FIGRuntimeInformation* rti, FIG0_8_mode mode = FIG0_8_mode::Both);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const {
            // Programme-only instances are group A MCI (96ms nominal).
            // Data-only and legacy Both instances use rate B, matching the
            // original declaration and EN 300 401 clause 6.1 for data
            // components.
            return (m_mode == FIG0_8_mode::Programme) ? FIG_rate::A : FIG_rate::B;
        }

        virtual int figtype() const { return 0; }
        virtual int figextension() const { return 8; }

    private:
        FIGRuntimeInformation *m_rti;
        FIG0_8_mode m_mode;
        bool m_initialised;
        bool m_transmit_programme;
        vec_sp_component::iterator componentFIG0_8;
};

}
