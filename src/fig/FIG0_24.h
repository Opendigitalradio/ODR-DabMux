/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
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
#include "FIG.h"

namespace FIC {
/* The OE Services feature is used to identify the services currently carried
 * in other DAB ensembles.
 */
class FIG0_24 : public IFIG
{
    public:
        FIG0_24(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::E; }

        virtual int figtype() const { return 0; }
        virtual int figextension() const { return 24; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;
        bool m_inserting_audio_not_data;
        std::vector<ServiceOtherEnsembleInfo>::iterator serviceFIG0_24;
};

}
