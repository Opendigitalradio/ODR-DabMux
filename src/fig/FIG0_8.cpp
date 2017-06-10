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

#include "fig/FIG0structs.h"
#include "fig/FIG0_8.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_8_short {
    uint8_t SCIdS:4;
    uint8_t rfa_1:3;
    uint8_t ext:1;
    uint8_t Id:6;
    uint8_t MscFic:1;
    uint8_t LS:1;
    uint8_t rfa_2;
} PACKED;

struct FIGtype0_8_long {
    uint8_t SCIdS:4;
    uint8_t rfa_1:3;
    uint8_t ext:1;
    uint8_t SCId_high:4;
    uint8_t rfa:3;
    uint8_t LS:1;
    uint8_t SCId_low;
    uint8_t rfa_2;
    void setSCId(uint16_t id) {
        SCId_high = id >> 8;
        SCId_low = id & 0xff;
    }
    uint16_t getSCid() {
        return (SCId_high << 8) | SCId_low;
    }
} PACKED;

FIG0_8::FIG0_8(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_transmit_programme(false)
{
}

FillStatus FIG0_8::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    ssize_t remaining = max_size;

    if (not m_initialised) {
        componentFIG0_8 = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; componentFIG0_8 != ensemble->components.end();
            ++componentFIG0_8) {

        auto service = getService(*componentFIG0_8,
                ensemble->services);
        auto subchannel = getSubchannel(ensemble->subchannels,
                (*componentFIG0_8)->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    (*componentFIG0_8)->subchId,
                    (*componentFIG0_8)->serviceId);
            throw MuxInitException();
        }

        if (m_transmit_programme and (*service)->isProgramme(ensemble)) {
            const int required_size =
                ((*subchannel)->type == subchannel_type_t::Packet ? 5 : 4);

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
                fig0->Extension = 8;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }

            if ((*subchannel)->type == subchannel_type_t::Packet) { // Data packet
                buf[0] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId) & 0xFF;
                fig0->Length += 2;
                buf += 2;
                remaining -= 2;

                FIGtype0_8_long* definition = (FIGtype0_8_long*)buf;
                memset(definition, 0, 3);
                definition->ext = 0;    // no rfa
                definition->SCIdS = (*componentFIG0_8)->SCIdS;
                definition->LS = 1;
                definition->setSCId((*componentFIG0_8)->packet.id);
                fig0->Length += 3;
                buf += 3;             // 8 minus rfa
                remaining -= 3;
            }
            else {    // Audio, data stream or FIDC
                buf[0] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId) & 0xFF;

                fig0->Length += 2;
                buf += 2;
                remaining -= 2;

                FIGtype0_8_short* definition = (FIGtype0_8_short*)buf;
                memset(definition, 0, 2);
                definition->ext = 0;    // no rfa
                definition->SCIdS = (*componentFIG0_8)->SCIdS;
                definition->LS = 0;
                definition->MscFic = 0;
                definition->Id = (*componentFIG0_8)->subchId;
                fig0->Length += 2;
                buf += 2;             // 4 minus rfa
                remaining -= 2;
            }
        }
        else if (!m_transmit_programme and !(*service)->isProgramme(ensemble)) {
            // Data
            const int required_size =
                ((*subchannel)->type == subchannel_type_t::Packet ? 7 : 6);

            if (fig0 == NULL) {
                if (remaining < 2 + required_size) {
                    break;
                }
                fig0 = (FIGtype0*)buf;
                fig0->FIGtypeNumber = 0;
                fig0->Length = 1;
                fig0->CN = 0;
                fig0->OE = 0;
                fig0->PD = 1;
                fig0->Extension = 8;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }

            if ((*subchannel)->type == subchannel_type_t::Packet) { // Data packet
                buf[0] = ((*componentFIG0_8)->serviceId >> 24) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId >> 16) & 0xFF;
                buf[2] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[3] = ((*componentFIG0_8)->serviceId) & 0xFF;
                fig0->Length += 4;
                buf += 4;
                remaining -= 4;

                FIGtype0_8_long* definition = (FIGtype0_8_long*)buf;
                memset(definition, 0, 3);
                definition->ext = 0;    // no rfa
                definition->SCIdS = (*componentFIG0_8)->SCIdS;
                definition->LS = 1;
                definition->setSCId((*componentFIG0_8)->packet.id);
                fig0->Length += 3;
                buf += 3;             // 8 minus rfa
                remaining -= 3;
            }
            else {    // Audio, data stream or FIDC
                buf[0] = ((*componentFIG0_8)->serviceId >> 24) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId >> 16) & 0xFF;
                buf[2] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[3] = ((*componentFIG0_8)->serviceId) & 0xFF;
                fig0->Length += 4;
                buf += 4;
                remaining -= 4;

                FIGtype0_8_short* definition = (FIGtype0_8_short*)buf;
                memset(definition, 0, 2);
                definition->ext = 0;    // no rfa
                definition->SCIdS = (*componentFIG0_8)->SCIdS;
                definition->LS = 0;
                definition->MscFic = 0;
                definition->Id = (*componentFIG0_8)->subchId;
                fig0->Length += 2;
                buf += 2;             // 4 minus rfa
                remaining -= 2;
            }
        }
    }

    if (componentFIG0_8 == ensemble->components.end()) {
        componentFIG0_8 = ensemble->components.begin();

        // The full database is sent every second full loop
        fs.complete_fig_transmitted = m_transmit_programme;

        m_transmit_programme = not m_transmit_programme;
        // Alternate between data and and programme FIG0/13,
        // do not mix fig0 with PD=0 with extension 13 stuff
        // that actually needs PD=1, and vice versa
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
