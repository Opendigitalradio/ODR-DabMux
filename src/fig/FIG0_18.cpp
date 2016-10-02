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

#include "fig/FIG0_18.h"
#include "utils.h"

namespace FIC {

FIG0_18::FIG0_18(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_18::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;

    if (not m_initialised) {
        service = m_rti->ensemble->services.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    FIGtype0* fig0 = NULL;

    for (; service != ensemble->services.end();
            ++service) {

        if ( (*service)->ASu == 0 ) {
            continue;
        }

        const ssize_t numclusters = (*service)->clusters.size();

        const int required_size = 5 + numclusters;


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
            fig0->Extension = 18;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        auto programme = (FIGtype0_18*)buf;
        programme->SId = htons((*service)->id);
        programme->ASu = htons((*service)->ASu);
        programme->Rfa = 0;
        programme->NumClusters = numclusters;
        buf += 5;

        for (uint8_t cluster : (*service)->clusters) {
            *(buf++) = cluster;
        }

        fig0->Length += 5 + numclusters;
        remaining -= 5 + numclusters;
    }

    if (service == ensemble->services.end()) {
        service = ensemble->services.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
