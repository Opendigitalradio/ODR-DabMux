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

#include "fig/FIG0_5.h"
#include "utils.h"

namespace FIC {

FIG0_5::FIG0_5(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_5::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    if (not m_initialised) {
        componentFIG0_5 = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; componentFIG0_5 != ensemble->components.end();
            ++componentFIG0_5) {

        auto service = getService(*componentFIG0_5,
                ensemble->services);
        auto subchannel = getSubchannel(ensemble->subchannels,
                (*componentFIG0_5)->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    (*componentFIG0_5)->subchId,
                    (*componentFIG0_5)->serviceId);
            throw MuxInitException();
        }

        if ( (*service)->language == 0) {
            continue;
        }

        const int required_size = 2;

        if (fig0 == NULL) {
            if (remaining < 2 + required_size) {
                break;
            }
            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN = 0;
            fig0->OE = 0;
            fig0->PD = 0;
            fig0->Extension = 5;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        FIGtype0_5_short *fig0_5 = (FIGtype0_5_short*)buf;

        fig0_5->LS = 0;
        fig0_5->rfu = 0;
        fig0_5->SubChId = (*subchannel)->id;
        fig0_5->language = (*service)->language;

        fig0->Length += 2;
        buf += 2;
        remaining -= 2;
    }

    if (componentFIG0_5 == ensemble->components.end()) {
        componentFIG0_5 = ensemble->components.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
