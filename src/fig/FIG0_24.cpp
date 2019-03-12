/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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
#include "fig/FIG0_24.h"
#include "utils.h"

/* FIG0/24 allows us to announce if a service is available in another ensemble.
 * Things we do not support:
 *
 * - The CEI using length=0, because this information is not available in the
 *   remote control, and there is no runtime changes.
 */

namespace FIC {

struct FIGtype0_24_audioservice {
    uint16_t SId;
    uint8_t Length:4;
    uint8_t CAId:3;
    uint8_t rfa:1;
} PACKED;

struct FIGtype0_24_dataservice {
    uint32_t SId;
    uint8_t Length:4;
    uint8_t CAId:3;
    uint8_t rfa:1;
} PACKED;


FIG0_24::FIG0_24(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_inserting_audio_not_data(false)
{
}

FillStatus FIG0_24::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_24_TRACE discard
    using namespace std;

    FillStatus fs;
    FIGtype0* fig0 = nullptr;
    ssize_t remaining = max_size;

    const auto ensemble = m_rti->ensemble;

    etiLog.level(FIG0_24_TRACE) << "FIG0_24::fill init " << (m_initialised ? 1 : 0) <<
        " ********************************";

    if (not m_initialised) {
        serviceFIG0_24 = ensemble->service_other_ensemble.begin();
        m_initialised = true;
    }

    const auto last_service = ensemble->service_other_ensemble.end();

    bool last_oe = false;

    // Rotate through the subchannels until there is no more
    // space
    for (; serviceFIG0_24 != last_service; ++serviceFIG0_24) {

        shared_ptr<DabService> service;
        for (const auto& local_service : ensemble->services) {
            if (local_service->id == serviceFIG0_24->service_id) {
                service = local_service;
                break;
            }
        }

        subchannel_type_t type = subchannel_type_t::Audio;
        bool isProgramme = true;
        bool oe = true;

        if (service) {
            oe = false;
            type = service->getType(ensemble);
            isProgramme = service->isProgramme(ensemble);
        }

        etiLog.log(FIG0_24_TRACE, "FIG0_24::fill  loop OE=%d SId=%04x %s/%s",
                oe,
                serviceFIG0_24->service_id,
                m_inserting_audio_not_data ? "AUDIO" : "DATA",
                type == subchannel_type_t::Audio ? "Audio" :
                type == subchannel_type_t::Packet ? "Packet" :
                type == subchannel_type_t::DataDmb ? "DataDmb" :
                type == subchannel_type_t::Fidc ? "Fidc" : "?");

        if (last_oe != oe) {
            fig0 = nullptr;
        }

        const ssize_t required_size =
            (isProgramme ? 2 : 4) + 1 +
            serviceFIG0_24->other_ensembles.size() * 2;


        if (fig0 == nullptr) {
            etiLog.level(FIG0_24_TRACE) << "FIG0_24::fill  header";
            if (remaining < 2 + required_size) {
                etiLog.level(FIG0_24_TRACE) << "FIG0_24::fill  header no place" <<
                    " rem=" << remaining << " req=" << 2 + required_size;
                break;
            }
            fig0 = (FIGtype0*)buf;

            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            // CN according to ETSI TS 103 176, Clause 5.3.4.1
            bool isFirst = serviceFIG0_24 == ensemble->service_other_ensemble.begin();
            fig0->CN = (isFirst ? 0 : 1);
            fig0->OE = oe;
            fig0->PD = isProgramme ? 0 : 1;
            fig0->Extension = 24;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            etiLog.level(FIG0_24_TRACE) << "FIG0_24::fill  no place" <<
                " rem=" << remaining << " req=" << required_size;
            break;
        }

        if (type == subchannel_type_t::Audio) {
            auto fig0_2serviceAudio = (FIGtype0_24_audioservice*)buf;

            fig0_2serviceAudio->SId = htons(serviceFIG0_24->service_id);
            fig0_2serviceAudio->rfa = 0;
            fig0_2serviceAudio->CAId = 0;
            fig0_2serviceAudio->Length = serviceFIG0_24->other_ensembles.size();
            buf += 3;
            fig0->Length += 3;
            remaining -= 3;

            etiLog.log(FIG0_24_TRACE, "FIG0_24::fill  audio SId=%04x",
               serviceFIG0_24->service_id);
        }
        else {
            auto fig0_2serviceData = (FIGtype0_24_dataservice*)buf;

            fig0_2serviceData->SId = htonl(serviceFIG0_24->service_id);
            fig0_2serviceData->rfa = 0;
            fig0_2serviceData->CAId = 0;
            fig0_2serviceData->Length = serviceFIG0_24->other_ensembles.size();
            buf += 4;
            fig0->Length += 4;
            remaining -= 4;

            etiLog.log(FIG0_24_TRACE, "FIG0_24::fill  data SId=%04x",
               serviceFIG0_24->service_id);
        }

        for (const uint16_t oe : serviceFIG0_24->other_ensembles) {
            buf[0] = oe >> 8;
            buf[1] = oe & 0xFF;

            buf += 2;
            fig0->Length += 2;
            remaining -= 2;
        }
    }

    if (serviceFIG0_24 == last_service) {
        etiLog.log(FIG0_24_TRACE, "FIG0_24::loop reached last");
        fs.complete_fig_transmitted = true;
        m_initialised = false;
    }

    etiLog.log(FIG0_24_TRACE, "FIG0_24::loop end complete=%d %d",
            fs.complete_fig_transmitted ? 1 : 0,
            max_size - remaining);

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
