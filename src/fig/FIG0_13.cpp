/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2020
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
#include "fig/FIG0_13.h"
#include "utils.h"

namespace FIC {

// See EN 300 401, Clause 8.1.20 for the FIG0_13 description
struct FIG0_13_shortAppInfo {
    uint16_t SId;
    uint8_t No:4;
    uint8_t SCIdS:4;
} PACKED;

struct FIG0_13_longAppInfo {
    uint32_t SId;
    uint8_t No:4;
    uint8_t SCIdS:4;
} PACKED;

struct FIG0_13_app {
    uint8_t typeHigh;
    uint8_t length:5;
    uint8_t typeLow:3;
    void setType(uint16_t type) {
        typeHigh = type >> 3;
        typeLow = type & 0x1f;
    }
} PACKED;


FIG0_13::FIG0_13(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_transmit_programme(false)
{
}

FillStatus FIG0_13::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    ssize_t remaining = max_size;

    if (not m_initialised) {
        componentFIG0_13 = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; componentFIG0_13 != ensemble->components.end();
            ++componentFIG0_13) {

        auto subchannel = getSubchannel(ensemble->subchannels,
                (*componentFIG0_13)->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i",
                    (*componentFIG0_13)->subchId,
                    (*componentFIG0_13)->serviceId);
            continue;
        }

        const auto type = (*subchannel)->type;
        if (   (m_transmit_programme and
                (type == subchannel_type_t::DABPlusAudio or type == subchannel_type_t::DABAudio) and
                (*componentFIG0_13)->audio.uaTypes.size() != 0)
            or (not m_transmit_programme and
                (*subchannel)->type == subchannel_type_t::Packet and
                (*componentFIG0_13)->packet.uaTypes.size() != 0)) {

            const std::vector<userApplication>& uaTypes = m_transmit_programme ?
                (*componentFIG0_13)->audio.uaTypes : (*componentFIG0_13)->packet.uaTypes;

            const size_t num_apps = uaTypes.size();

            const size_t xpadapp_length = 2;
            static_assert(sizeof(FIG0_13_shortAppInfo) == 3);
            static_assert(sizeof(FIG0_13_longAppInfo) == 5);
            static_assert(sizeof(FIG0_13_app) == 2);

            int required_size = 0;
            if (m_transmit_programme) {
                required_size += sizeof(FIG0_13_shortAppInfo);
            }
            else {
                required_size += sizeof(FIG0_13_longAppInfo);
            }

            for (const auto& ua : uaTypes) {
                if (m_transmit_programme) {
                    required_size += sizeof(FIG0_13_app) + xpadapp_length;
                }
                else {
                    required_size += sizeof(FIG0_13_app);
                }

                if (ua.uaType == FIG0_13_APPTYPE_SPI) {
                    required_size += 2; // For the "basic profile" user application data
                }
            }

            if (fig0 == NULL) {
                if (remaining < 2 + required_size) {
                    break;
                }
                fig0 = (FIGtype0*)buf;
                fig0->FIGtypeNumber = 0;
                fig0->Length = 1;
                fig0->CN = 0;
                fig0->OE = 0;
                fig0->PD = m_transmit_programme ? 0 : 1;
                fig0->Extension = 13;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }

            if (m_transmit_programme) {
                FIG0_13_shortAppInfo* info = (FIG0_13_shortAppInfo*)buf;
                info->SId = htonl((*componentFIG0_13)->serviceId) >> 16;
                info->SCIdS = (*componentFIG0_13)->SCIdS;
                info->No = num_apps;
                buf += sizeof(FIG0_13_shortAppInfo);
                remaining -= sizeof(FIG0_13_shortAppInfo);
                fig0->Length += sizeof(FIG0_13_shortAppInfo);
            }
            else {
                FIG0_13_longAppInfo* info = (FIG0_13_longAppInfo*)buf;
                info->SId = htonl((*componentFIG0_13)->serviceId);
                info->SCIdS = (*componentFIG0_13)->SCIdS;
                info->No = num_apps;
                buf += sizeof(FIG0_13_longAppInfo);
                remaining -= sizeof(FIG0_13_longAppInfo);
                fig0->Length += sizeof(FIG0_13_longAppInfo);
            }

            for (const auto& ua : uaTypes) {
                FIG0_13_app* app = (FIG0_13_app*)buf;
                app->setType(ua.uaType);
                app->length = xpadapp_length;
                if (ua.uaType == FIG0_13_APPTYPE_SPI) {
                    app->length += 2;
                }

                buf += sizeof(FIG0_13_app);
                remaining -= sizeof(FIG0_13_app);
                fig0->Length += sizeof(FIG0_13_app);

                if (m_transmit_programme) {
                    const uint8_t dscty = 60; // TS 101 756 Table 2b (MOT)
                    const uint16_t xpadapp = htons((ua.xpadAppType << 8) | dscty);
                    /* xpad meaning
                       CA        = 0
                       CAOrg     = 0 (CAOrg field absent)
                       Rfu       = 0
                       AppTy(5)  = depending on config
                       DG        = 0 (MSC data groups used)
                       Rfu       = 0
                       DSCTy(6)  = 60 (MOT)
                       */

                    memcpy(buf, &xpadapp, 2);
                    buf += 2;
                    remaining -= 2;
                    fig0->Length += 2;
                }

                if (ua.uaType == FIG0_13_APPTYPE_SPI) {
                    buf[0] = 0x01; // = basic profile
                    buf[1] = 0x00; // = list terminator
                    buf += 2;
                    remaining -= 2;
                    fig0->Length += 2;
                }
            }
        }
    }

    if (componentFIG0_13 == ensemble->components.end()) {
        componentFIG0_13 = ensemble->components.begin();

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
