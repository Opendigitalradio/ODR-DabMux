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
    uint16_t xpad;
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
        if (    m_transmit_programme and
                (type == subchannel_type_t::DABPlusAudio or type == subchannel_type_t::DABAudio) and
                (*componentFIG0_13)->audio.uaTypes.size() != 0) {

            const size_t num_apps = (*componentFIG0_13)->audio.uaTypes.size();

            const size_t app_length = 2;
            static_assert(sizeof(FIG0_13_shortAppInfo) == 3);
            static_assert(sizeof(FIG0_13_app) == 4);
            const int required_size = sizeof(FIG0_13_shortAppInfo) + num_apps * (sizeof(FIG0_13_app) + app_length);

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
                fig0->Extension = 13;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }

            FIG0_13_shortAppInfo* info = (FIG0_13_shortAppInfo*)buf;
            info->SId = htonl((*componentFIG0_13)->serviceId) >> 16;
            info->SCIdS = (*componentFIG0_13)->SCIdS;
            info->No = num_apps;
            buf += sizeof(FIG0_13_shortAppInfo);
            remaining -= sizeof(FIG0_13_shortAppInfo);
            fig0->Length += sizeof(FIG0_13_shortAppInfo);

            for (const auto& ua : (*componentFIG0_13)->audio.uaTypes) {
                FIG0_13_app* app = (FIG0_13_app*)buf;
                app->setType(ua.uaType);
                app->length = app_length;

                const uint8_t dscty = 60; // TS 101 756 Table 2b (MOT)
                app->xpad = htons((ua.xpadAppType << 8) | dscty);
                /* xpad meaning
                   CA        = 0
                   CAOrg     = 0 (CAOrg field absent)
                   Rfu       = 0
                   AppTy(5)  = depending on config
                   DG        = 0 (MSC data groups used)
                   Rfu       = 0
                   DSCTy(6)  = 60 (MOT)
                   */

                buf += sizeof(FIG0_13_app);
                remaining -= sizeof(FIG0_13_app);
                fig0->Length += sizeof(FIG0_13_app);
            }
        }
        else if (not m_transmit_programme and
                (*subchannel)->type == subchannel_type_t::Packet and
                (*componentFIG0_13)->packet.uaTypes.size() != 0) {

            const size_t num_apps = (*componentFIG0_13)->audio.uaTypes.size();

            const size_t app_length = 2;
            const int required_size = sizeof(FIG0_13_longAppInfo) + num_apps * (sizeof(FIG0_13_app) + app_length);
            /* is conservative because app_length can be 0 */

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
                fig0->Extension = 13;
                buf += 2;
                remaining -= 2;
            }
            else if (remaining < required_size) {
                break;
            }

            FIG0_13_longAppInfo* info = (FIG0_13_longAppInfo*)buf;
            info->SId = htonl((*componentFIG0_13)->serviceId);
            info->SCIdS = (*componentFIG0_13)->SCIdS;
            info->No = num_apps;
            buf += sizeof(FIG0_13_longAppInfo);
            remaining -= sizeof(FIG0_13_longAppInfo);
            fig0->Length += sizeof(FIG0_13_longAppInfo);

            for (const auto& ua : (*componentFIG0_13)->audio.uaTypes) {
                FIG0_13_app* app = (FIG0_13_app*)buf;
                app->setType(ua.uaType);

                size_t effective_length = sizeof(FIG0_13_app);

                if (ua.uaType == FIG0_13_APPTYPE_SPI) {
                    // TODO This should probably be user configurable...
                    app->length = app_length;
                    app->xpad = htons(0x0100);
                    /* xpad is actually not the "X-PAD data" as in Figure 25, but is the actual user application data.
                     * We just recycle the same structure, even though it's a bit ugly.
                     * It holds two bytes of EPG profile information:
                     * 01 = basic profile
                     * 00 = list terminator */
                }
                else {
                    app->length = 0;
                    effective_length = 1; // FIG0_13_app without xpad
                }

                buf += effective_length;
                remaining -= effective_length;
                fig0->Length += effective_length;
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
