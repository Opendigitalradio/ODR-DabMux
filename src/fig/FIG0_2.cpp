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
#include "fig/FIG0_2.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_2 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;


struct FIGtype0_2_Service {
    uint16_t SId;
    uint8_t NbServiceComp:4;
    uint8_t CAId:3;
    uint8_t Local_flag:1;
} PACKED;


struct FIGtype0_2_Service_data {
    uint32_t SId;
    uint8_t NbServiceComp:4;
    uint8_t CAId:3;
    uint8_t Local_flag:1;
} PACKED;


struct FIGtype0_2_audio_component {
    uint8_t ASCTy:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SubChId:6;
} PACKED;


struct FIGtype0_2_data_component {
    uint8_t DSCTy:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SubChId:6;
} PACKED;


struct FIGtype0_2_packet_component {
    uint8_t SCId_high:6;
    uint8_t TMid:2;
    uint8_t CA_flag:1;
    uint8_t PS:1;
    uint8_t SCId_low:6;
    void setSCId(uint16_t SCId) {
        SCId_high = SCId >> 6;
        SCId_low = SCId & 0x3f;
    }
} PACKED;

FIG0_2::FIG0_2(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_inserting_audio_not_data(false)
{
}

FillStatus FIG0_2::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_2_TRACE discard
    using namespace std;

    FillStatus fs;
    FIGtype0_2 *fig0_2 = NULL;
    int cur = 0;
    ssize_t remaining = max_size;

    const auto ensemble = m_rti->ensemble;

    etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill init " << (m_initialised ? 1 : 0) <<
        " ********************************";

    for (const auto& s : ensemble->services) {
        // Exclude Fidc type services, TODO unsupported
        auto type = s->getType(ensemble);
        if (type == subchannel_type_t::Fidc) {
            etiLog.log(warn, "FIG0/2 does not support FIDC");
            continue;
        }
    }

    if (not m_initialised) {
        m_audio_services.clear();
        copy_if(ensemble->services.begin(),
                ensemble->services.end(),
                back_inserter(m_audio_services),
                [&](shared_ptr<DabService>& s) {
                    return s->isProgramme(ensemble);
                } );

        m_data_services.clear();
        copy_if(ensemble->services.begin(),
                ensemble->services.end(),
                back_inserter(m_data_services),
                [&](shared_ptr<DabService>& s) {
                    return not s->isProgramme(ensemble);
                } );

        m_initialised = true;
        m_inserting_audio_not_data = !m_inserting_audio_not_data;

        if (m_inserting_audio_not_data) {
            serviceFIG0_2 = m_audio_services.begin();
        }
        else {
            serviceFIG0_2 = m_data_services.begin();
        }

        etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill we have " <<
            m_audio_services.size() << " audio and " <<
            m_data_services.size() << " data services. Inserting " <<
            (m_inserting_audio_not_data ? "AUDIO" : "DATA");
    }

    const auto last_service = m_inserting_audio_not_data ?
            m_audio_services.end() : m_data_services.end();

    // Rotate through the subchannels until there is no more
    // space
    for (; serviceFIG0_2 != last_service; ++serviceFIG0_2) {
        const auto type = (*serviceFIG0_2)->getType(ensemble);
        const auto isProgramme = (*serviceFIG0_2)->isProgramme(ensemble);

        etiLog.log(FIG0_2_TRACE, "FIG0_2::fill  loop SId=%04x %s/%s",
                (*serviceFIG0_2)->id,
                m_inserting_audio_not_data ? "AUDIO" : "DATA",
                type == subchannel_type_t::Audio ? "Audio" :
                type == subchannel_type_t::Packet ? "Packet" :
                type == subchannel_type_t::DataDmb ? "DataDmb" :
                type == subchannel_type_t::Fidc ? "Fidc" : "?");

        // filter out services which have no components
        if ((*serviceFIG0_2)->nbComponent(ensemble->components) == 0) {
            etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill  no components ";
            continue;
        }

        ++cur;

        const int required_size = isProgramme ?
            3 + 2 * (*serviceFIG0_2)->nbComponent(ensemble->components) :
            5 + 2 * (*serviceFIG0_2)->nbComponent(ensemble->components);

        if (fig0_2 == NULL) {
            etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill  header";
            if (remaining < 2 + required_size) {
                etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill  header no place" <<
                    " rem=" << remaining << " req=" << 2 + required_size;
                break;
            }
            fig0_2 = (FIGtype0_2 *)buf;

            fig0_2->FIGtypeNumber = 0;
            fig0_2->Length = 1;
            fig0_2->CN = 0;
            fig0_2->OE = 0;
            fig0_2->PD = isProgramme ? 0 : 1;
            fig0_2->Extension = 2;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            etiLog.level(FIG0_2_TRACE) << "FIG0_2::fill  no place" <<
                " rem=" << remaining << " req=" << required_size;
            break;
        }

        if (type == subchannel_type_t::Audio) {
            auto fig0_2serviceAudio = (FIGtype0_2_Service*)buf;

            fig0_2serviceAudio->SId = htons((*serviceFIG0_2)->id);
            fig0_2serviceAudio->Local_flag = 0;
            fig0_2serviceAudio->CAId = 0;
            fig0_2serviceAudio->NbServiceComp =
                (*serviceFIG0_2)->nbComponent(ensemble->components);
            buf += 3;
            fig0_2->Length += 3;
            remaining -= 3;

            etiLog.log(FIG0_2_TRACE, "FIG0_2::fill  audio SId=%04x",
               (*serviceFIG0_2)->id);
        }
        else {
            auto fig0_2serviceData = (FIGtype0_2_Service_data*)buf;

            fig0_2serviceData->SId = htonl((*serviceFIG0_2)->id);
            fig0_2serviceData->Local_flag = 0;
            fig0_2serviceData->CAId = 0;
            fig0_2serviceData->NbServiceComp =
                (*serviceFIG0_2)->nbComponent(ensemble->components);
            buf += 5;
            fig0_2->Length += 5;
            remaining -= 5;

            etiLog.log(FIG0_2_TRACE, "FIG0_2::fill  data SId=%04x",
               (*serviceFIG0_2)->id);
        }

        int curCpnt = 0;
        for (auto component = getComponent(
                    ensemble->components, (*serviceFIG0_2)->id );
                component != ensemble->components.end();
                component = getComponent(
                    ensemble->components,
                    (*serviceFIG0_2)->id,
                    component )
            ) {
            auto subchannel = getSubchannel(
                    ensemble->subchannels, (*component)->subchId);

            etiLog.log(FIG0_2_TRACE, "FIG0_2::fill     comp sub=%04x srv=%04x",
                (*component)->subchId, (*component)->serviceId);

            if (subchannel == ensemble->subchannels.end()) {
                etiLog.log(error,
                        "Subchannel %i does not exist for component "
                        "of service %i",
                        (*component)->subchId, (*component)->serviceId);
                continue;
            }

            switch ((*subchannel)->type) {
                case subchannel_type_t::Audio:
                    {
                        auto audio_description = (FIGtype0_2_audio_component*)buf;
                        audio_description->TMid    = 0;
                        audio_description->ASCTy   = (*component)->type;
                        audio_description->SubChId = (*subchannel)->id;
                        audio_description->PS      = ((curCpnt == 0) ? 1 : 0);
                        audio_description->CA_flag = 0;
                    }
                    break;
                case subchannel_type_t::DataDmb:
                    {
                        auto data_description = (FIGtype0_2_data_component*)buf;
                        data_description->TMid    = 1;
                        data_description->DSCTy   = (*component)->type;
                        data_description->SubChId = (*subchannel)->id;
                        data_description->PS      = ((curCpnt == 0) ? 1 : 0);
                        data_description->CA_flag = 0;
                    }
                    break;
                case subchannel_type_t::Packet:
                    {
                        auto packet_description = (FIGtype0_2_packet_component*)buf;
                        packet_description->TMid    = 3;
                        packet_description->setSCId((*component)->packet.id);
                        packet_description->PS      = ((curCpnt == 0) ? 1 : 0);
                        packet_description->CA_flag = 0;
                    }
                    break;
                default:
                    throw logic_error("Component type not supported");
            }
            buf += 2;
            fig0_2->Length += 2;
            remaining -= 2;
            if (remaining < 0) {
                etiLog.log(error,
                        "Sorry, no space left in FIG 0/2 to insert "
                        "component %i of program service %i.",
                        curCpnt, cur);
                throw MuxInitException();
            }
            ++curCpnt;

            etiLog.log(FIG0_2_TRACE, "FIG0_2::fill     comp length=%d",
                    fig0_2->Length);
        }
    }

    if (serviceFIG0_2 == last_service) {
        etiLog.log(FIG0_2_TRACE, "FIG0_2::loop reached last");
        m_initialised = false;
        fs.complete_fig_transmitted = !m_inserting_audio_not_data;
    }

    etiLog.log(FIG0_2_TRACE, "FIG0_2::loop end complete=%d",
            fs.complete_fig_transmitted ? 1 : 0);

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
