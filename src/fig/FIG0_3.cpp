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
#include "fig/FIG0_3.h"
#include "utils.h"

namespace FIC {

/* Warning: When bit SCCA_flag is unset(0), the multiplexer R&S does not send
 * the SCCA field. But, in the Factum ETI analyzer, if this field is not there,
 * it is an error.
 */
struct FIGtype0_3 {
    uint8_t SCId_high;

    uint8_t SCCA_flag:1;
    uint8_t rfa:3;
    uint8_t SCId_low:4;

    uint8_t DSCTy:6;
    uint8_t rfu:1;
    uint8_t DG_flag:1;

    uint8_t Packet_address_high:2;
    uint8_t SubChId:6;

    uint8_t Packet_address_low;
    uint16_t SCCA;
    void setSCId(uint16_t SCId) {
        SCId_high = SCId >> 4;
        SCId_low = SCId & 0xf;
    }
    void setPacketAddress(uint16_t address) {
        Packet_address_high = address >> 8;
        Packet_address_low = address & 0xff;
    }
} PACKED;

FIG0_3::FIG0_3(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_3::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    if (not m_initialised) {
        componentFIG0_3 = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    FIGtype0 *fig0 = NULL;

    for (; componentFIG0_3 != ensemble->components.end();
            ++componentFIG0_3) {
        auto subchannel = getSubchannel(ensemble->subchannels,
                (*componentFIG0_3)->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "FIG0/3: Subchannel %i does not exist "
                    "for component of service %i",
                    (*componentFIG0_3)->subchId,
                    (*componentFIG0_3)->serviceId);
            continue;
        }

        if ((*subchannel)->type != subchannel_type_t::Packet)
            continue;

        const int required_size = 5 + (m_rti->factumAnalyzer ? 2 : 0);

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
            fig0->Extension = 3;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        /*  Warning: When bit SCCA_flag is unset(0), the multiplexer
         *  R&S does not send the SCCA field. But, in the Factum ETI
         *  analyzer, if this field is not there, it is an error.
         */
        FIGtype0_3 *fig0_3 = (FIGtype0_3*)buf;
        fig0_3->setSCId((*componentFIG0_3)->packet.id);
        fig0_3->rfa = 0;
        fig0_3->SCCA_flag = 0;
        // if 0, datagroups are used
        fig0_3->DG_flag = !(*componentFIG0_3)->packet.datagroup;
        fig0_3->rfu = 0;
        fig0_3->DSCTy = (*componentFIG0_3)->type;
        fig0_3->SubChId = (*subchannel)->id;
        fig0_3->setPacketAddress((*componentFIG0_3)->packet.address);
        if (m_rti->factumAnalyzer) {
            fig0_3->SCCA = 0;
        }

        fig0->Length += 5;
        buf += 5;
        remaining -= 5;
        if (m_rti->factumAnalyzer) {
            fig0->Length += 2;
            buf += 2;
            remaining -= 2;
        }
    }

    if (componentFIG0_3 == ensemble->components.end()) {
        componentFIG0_3 = ensemble->components.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
