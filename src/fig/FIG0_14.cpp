/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2020
   Matthias P. Braendli, matthias.braendli@mpb.li
   
   Copyright (C) 2022
   Nick Piggott, nick@piggott.eu
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

#include "fig/FIG0structs.h"
#include "fig/FIG0_14.h"
#include "utils.h"

namespace FIC {

// See EN 300 401, Clause 6.2.2 for the FIG0_14 description

struct FIG0_14_AppInfo {
    uint8_t subchId:6;
    uint8_t fecScheme:2;
} PACKED;


FIG0_14::FIG0_14(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_14::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    ssize_t remaining = max_size;

    if (not m_initialised) {
        componentFIG0_14 = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; componentFIG0_14 != ensemble->components.end();
            ++componentFIG0_14) {

        auto subchannel = getSubchannel(ensemble->subchannels,
                (*componentFIG0_14)->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i",
                    (*componentFIG0_14)->subchId,
                    (*componentFIG0_14)->serviceId);
            continue;
        }

        if (   ((*subchannel)->type == subchannel_type_t::Packet and
                true)) { // Need to do a test here to see if a packet channel is normal or enhanced

            static_assert(sizeof(FIG0_14_AppInfo) == 1);

            int required_size = 1; // FIG0_14 data field is 6 bits subchanID and 2 bits fec scheme
            
            if (fig0 == NULL) {
                if (remaining < 2 + required_size) {
                    break;
                }
                fig0 = (FIGtype0*)buf;
                fig0->FIGtypeNumber = 0;
                fig0->Length = required_size;
                fig0->CN = 0;
                fig0->OE = 0;
                fig0->PD = 1; // Only ever applicable to data services
                fig0->Extension = 14;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }
            FIG0_14_AppInfo* app = (FIG0_14_AppInfo*)buf;
            app->subchId = (*componentFIG0_14)->subchId;
            app->fecScheme = 1; // only 1 fec_scheme is defined
            buf += 1 ;
            remaining -= 1;
            fig0->Length += 1;
        }
    }

    if (componentFIG0_14 == ensemble->components.end()) {
        componentFIG0_14 = ensemble->components.begin();

        fs.complete_fig_transmitted = true;

    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
