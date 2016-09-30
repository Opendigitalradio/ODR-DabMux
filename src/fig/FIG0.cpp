/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of FIG0
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

#include "fig/FIG0.h"
#include "fig/FIG0structs.h"
#include "utils.h"

namespace FIC {

//=========== FIG 0/0 ===========

FillStatus FIG0_0::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    if (max_size < 6) {
        fs.num_bytes_written = 0;
        return fs;
    }

    FIGtype0_0 *fig0_0;
    fig0_0 = (FIGtype0_0 *)buf;

    fig0_0->FIGtypeNumber = 0;
    fig0_0->Length = 5;
    fig0_0->CN = 0;
    fig0_0->OE = 0;
    fig0_0->PD = 0;
    fig0_0->Extension = 0;

    fig0_0->EId = htons(m_rti->ensemble->id);
    fig0_0->Change = 0;
    fig0_0->Al = 0;
    fig0_0->CIFcnt_hight = (m_rti->currentFrame / 250) % 20;
    fig0_0->CIFcnt_low = (m_rti->currentFrame % 250);

    fs.complete_fig_transmitted = true;
    fs.num_bytes_written = 6;
    return fs;
}


//=========== FIG 0/1 ===========

FIG0_1::FIG0_1(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false),
    m_watermarkSize(0),
    m_watermarkPos(0)
{
    uint8_t buffer[sizeof(m_watermarkData) / 2];
    snprintf((char*)buffer, sizeof(buffer),
            "%s %s, %s %s",
            PACKAGE_NAME,
#if defined(GITVERSION)
            GITVERSION,
#else
            PACKAGE_VERSION,
#endif
            __DATE__, __TIME__);

    memset(m_watermarkData, 0, sizeof(m_watermarkData));
    m_watermarkData[0] = 0x55; // Sync
    m_watermarkData[1] = 0x55;
    m_watermarkSize = 16;
    for (unsigned i = 0; i < strlen((char*)buffer); ++i) {
        for (int j = 0; j < 8; ++j) {
            uint8_t bit = (buffer[m_watermarkPos >> 3] >> (7 - (m_watermarkPos & 0x07))) & 1;
            m_watermarkData[m_watermarkSize >> 3] |= bit << (7 - (m_watermarkSize & 0x07));
            ++m_watermarkSize;
            bit = 1;
            m_watermarkData[m_watermarkSize >> 3] |= bit << (7 - (m_watermarkSize & 0x07));
            ++m_watermarkSize;
            ++m_watermarkPos;
        }
    }
    m_watermarkPos = 0;
}

FillStatus FIG0_1::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_1_TRACE discard

    FillStatus fs;
    size_t remaining = max_size;

    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill initialised=" <<
        (m_initialised ? 1 : 0);

    const int watermark_bit = (m_watermarkData[m_watermarkPos >> 3] >>
            (7 - (m_watermarkPos & 0x07))) & 1;

    const bool iterate_forward = (watermark_bit == 1);

    if (not m_initialised) {
        m_initialised = true;

        subchannels = m_rti->ensemble->subchannels;

        if (not iterate_forward) {
            std::reverse(subchannels.begin(), subchannels.end());
        }
        subchannelFIG0_1 = subchannels.begin();
    }

    if (max_size < 6) {
        return fs;
    }

    FIGtype0_1 *figtype0_1 = NULL;

    // Rotate through the subchannels until there is no more
    // space in the FIG0/1
    for (; subchannelFIG0_1 != subchannels.end(); ++subchannelFIG0_1 ) {
        size_t subch_iter_ix = std::distance(subchannels.begin(), subchannelFIG0_1);

        etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill loop ix=" << subch_iter_ix;

        dabProtection* protection = &(*subchannelFIG0_1)->protection;

        if (figtype0_1 == NULL) {
            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill header " <<
                 (protection->form == UEP ? "UEP " : "EEP ") << remaining;

            if ( (protection->form == UEP && remaining < 2 + 3) ||
                 (protection->form == EEP && remaining < 2 + 4) ) {
                etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill no space for header";
                break;
            }

            figtype0_1 = (FIGtype0_1*)buf;

            figtype0_1->FIGtypeNumber = 0;
            figtype0_1->Length = 1;
            figtype0_1->CN = 0;
            figtype0_1->OE = 0;
            figtype0_1->PD = 0;
            figtype0_1->Extension = 1;
            buf += 2;
            remaining -= 2;
        }
        else if ( (protection->form == UEP && remaining < 3) ||
             (protection->form == EEP && remaining < 4) ) {
            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill no space for fig " <<
                 (protection->form == UEP ? "UEP " : "EEP ") << remaining;
            break;
        }

        if (protection->form == UEP) {
            FIG_01_SubChannel_ShortF *fig0_1subchShort =
                (FIG_01_SubChannel_ShortF*)buf;
            fig0_1subchShort->SubChId = (*subchannelFIG0_1)->id;

            fig0_1subchShort->StartAdress_high =
                (*subchannelFIG0_1)->startAddress / 256;
            fig0_1subchShort->StartAdress_low =
                (*subchannelFIG0_1)->startAddress % 256;

            fig0_1subchShort->Short_Long_form = 0;
            fig0_1subchShort->TableSwitch = 0;
            fig0_1subchShort->TableIndex =
                protection->uep.tableIndex;

            buf += 3;
            remaining -= 3;
            figtype0_1->Length += 3;

            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill insert UEP id=" <<
               (int)fig0_1subchShort->SubChId << " rem=" << remaining
               << " ix=" << subch_iter_ix;
        }
        else if (protection->form == EEP) {
            FIG_01_SubChannel_LongF *fig0_1subchLong1 =
                (FIG_01_SubChannel_LongF*)buf;
            fig0_1subchLong1->SubChId = (*subchannelFIG0_1)->id;

            fig0_1subchLong1->StartAdress_high =
                (*subchannelFIG0_1)->startAddress / 256;
            fig0_1subchLong1->StartAdress_low =
                (*subchannelFIG0_1)->startAddress % 256;

            fig0_1subchLong1->Short_Long_form = 1;
            fig0_1subchLong1->Option = protection->eep.GetOption();
            fig0_1subchLong1->ProtectionLevel =
                protection->level;

            fig0_1subchLong1->Sub_ChannelSize_high =
                (*subchannelFIG0_1)->getSizeCu() / 256;
            fig0_1subchLong1->Sub_ChannelSize_low =
                (*subchannelFIG0_1)->getSizeCu() % 256;

            buf += 4;
            remaining -= 4;
            figtype0_1->Length += 4;

            etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill insert EEP id=" <<
               (int)fig0_1subchLong1->SubChId << " rem=" << remaining
               << " ix=" << subch_iter_ix;
        }
    }

    size_t subch_iter_ix = std::distance(subchannels.begin(), subchannelFIG0_1);

    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill loop out, rem=" << remaining
        << " ix=" << subch_iter_ix;

    if (subchannelFIG0_1 == subchannels.end()) {
        etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill completed, rem=" << remaining;
        m_initialised = false;
        fs.complete_fig_transmitted = true;

        m_watermarkPos++;
        if (m_watermarkPos == m_watermarkSize) {
            m_watermarkPos = 0;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    etiLog.level(FIG0_1_TRACE) << "FIG0_1::fill wrote " << fs.num_bytes_written;
    return fs;
}


//=========== FIG 0/2 ===========

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
            throw invalid_argument("FIG0/2 does not support FIDC");
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
                        "of service %i\n",
                        (*component)->subchId, (*component)->serviceId);
                throw MuxInitException();
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
                    etiLog.log(error,
                            "Component type not supported\n");
                    throw MuxInitException();
            }
            buf += 2;
            fig0_2->Length += 2;
            remaining -= 2;
            if (remaining < 0) {
                etiLog.log(error,
                        "Sorry, no space left in FIG 0/2 to insert "
                        "component %i of program service %i.\n",
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


//=========== FIG 0/3 ===========

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
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    (*componentFIG0_3)->subchId, (*componentFIG0_3)->serviceId);
            throw MuxInitException();
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

//=========== FIG 0/5 ===========

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

//=========== FIG 0/8 ===========

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
                buf[0] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId) & 0xFF;
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
                buf[0] = ((*componentFIG0_8)->serviceId >> 8) & 0xFF;
                buf[1] = ((*componentFIG0_8)->serviceId) & 0xFF;
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

//=========== FIG 0/9 ===========
FIG0_9::FIG0_9(FIGRuntimeInformation *rti) :
    m_rti(rti) {}

FillStatus FIG0_9::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 5) {
        fs.num_bytes_written = 0;
        return fs;
    }

    auto fig0_9 = (FIGtype0_9*)buf;
    fig0_9->FIGtypeNumber = 0;
    fig0_9->Length = 4;
    fig0_9->CN = 0;
    fig0_9->OE = 0;
    fig0_9->PD = 0;
    fig0_9->Extension = 9;

    fig0_9->ext = 0;
    fig0_9->rfa1 = 0; // Had a different meaning in EN 300 401 V1.4.1

    if (ensemble->lto_auto) {
        time_t now = time(NULL);
        struct tm* ltime = localtime(&now);
        time_t now2 = timegm(ltime);
        ensemble->lto = (now2 - now) / 1800;
    }

    if (ensemble->lto >= 0) {
        fig0_9->ensembleLto = ensemble->lto;
    }
    else {
        /* Convert to 1-complement representation */
        fig0_9->ensembleLto = (-ensemble->lto) | (1<<5);
    }

    fig0_9->ensembleEcc = ensemble->ecc;
    fig0_9->tableId = ensemble->international_table;
    buf += 5;
    remaining -= 5;

    /* No extended field, no support for services with different ECC */

    fs.num_bytes_written = max_size - remaining;
    fs.complete_fig_transmitted = true;
    return fs;
}

//=========== FIG 0/10 ===========

FIG0_10::FIG0_10(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
}

FillStatus FIG0_10::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 8) {
        fs.num_bytes_written = 0;
        return fs;
    }

    //Time and country identifier
    auto fig0_10 = (FIGtype0_10_LongForm*)buf;

    fig0_10->FIGtypeNumber = 0;
    fig0_10->Length = 7;
    fig0_10->CN = 0;
    fig0_10->OE = 0;
    fig0_10->PD = 0;
    fig0_10->Extension = 10;
    buf += 2;
    remaining -= 2;

    tm* timeData;
    time_t dab_time_seconds = 0;
    uint32_t dab_time_millis = 0;
    get_dab_time(&dab_time_seconds, &dab_time_millis);
    timeData = gmtime(&dab_time_seconds);

    fig0_10->RFU = 0;
    fig0_10->setMJD(gregorian2mjd(timeData->tm_year + 1900,
                timeData->tm_mon + 1,
                timeData->tm_mday));
    fig0_10->LSI = 0;
    fig0_10->ConfInd = 1;
    fig0_10->UTC = 1;
    fig0_10->setHours(timeData->tm_hour);
    fig0_10->Minutes = timeData->tm_min;
    fig0_10->Seconds = timeData->tm_sec;
    fig0_10->setMilliseconds(dab_time_millis);
    buf += 6;
    remaining -= 6;

    fs.num_bytes_written = max_size - remaining;
    fs.complete_fig_transmitted = true;
    return fs;
}

//=========== FIG 0/13 ===========

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
                    "of service %i\n",
                    (*componentFIG0_13)->subchId,
                    (*componentFIG0_13)->serviceId);
            throw MuxInitException();
        }

        if (    m_transmit_programme &&
                (*subchannel)->type == subchannel_type_t::Audio &&
                (*componentFIG0_13)->audio.uaType != 0xffff) {

            const int required_size = 3+4+11;

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
            info->No = 1;
            buf += 3;
            remaining -= 3;
            fig0->Length += 3;

            FIG0_13_app* app = (FIG0_13_app*)buf;
            app->setType((*componentFIG0_13)->audio.uaType);
            app->length = 2;
            app->xpad = htons(0x0c3c);
            /* xpad meaning
               CA        = 0
               CAOrg     = 0
               Rfu       = 0
               AppTy(5)  = 12 (MOT, start of X-PAD data group)
               DG        = 0 (MSC data groups used)
               Rfu       = 0
               DSCTy(6)  = 60 (MOT)
               */

            buf += 2 + app->length;
            remaining -= 2 + app->length;
            fig0->Length += 2 + app->length;
        }
        else if (!m_transmit_programme &&
                (*subchannel)->type == subchannel_type_t::Packet &&
                (*componentFIG0_13)->packet.appType != 0xffff) {

            const int required_size = 5+2+2;

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
            info->No = 1;
            buf += 5;
            remaining -= 5;
            fig0->Length += 5;

            FIG0_13_app* app = (FIG0_13_app*)buf;
            app->setType((*componentFIG0_13)->packet.appType);
            if (app->typeLow == FIG0_13_APPTYPE_EPG) {
                app->length = 2;
                app->xpad = htons(0x0100);
                /* xpad used to hold two bytes of EPG profile information
                   01 = basic profile
                   00 = list terminator */
            }
            else {
                app->length = 0;
            }
            buf += 2 + app->length;
            remaining -= 2 + app->length;
            fig0->Length += 2 + app->length;
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

//=========== FIG 0/17 PTy ===========

FIG0_17::FIG0_17(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_17::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;

    if (not m_initialised) {
        serviceFIG0_17 = m_rti->ensemble->services.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    FIGtype0* fig0 = NULL;

    for (; serviceFIG0_17 != ensemble->services.end();
            ++serviceFIG0_17) {

        if ((*serviceFIG0_17)->pty == 0) {
            continue;
        }

        const int required_size = 4;


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
            fig0->Extension = 17;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        auto fig0_17 = (FIGtype0_17*)buf;
        fig0_17->SId = htons((*serviceFIG0_17)->id);
        fig0_17->SD = 1; // We only transmit dynamic PTy
        fig0_17->rfa1 = 0;
        fig0_17->rfu1 = 0;
        fig0_17->rfa2_low = 0;
        fig0_17->rfa2_high = 0;
        fig0_17->rfu2 = 0;
        fig0_17->IntCode = (*serviceFIG0_17)->pty;

        fig0->Length += 4;
        buf += 4;
        remaining -= 4;
    }

    if (serviceFIG0_17 == ensemble->services.end()) {
        serviceFIG0_17 = ensemble->services.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

//=========== FIG 0/18 ===========

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

//=========== FIG 0/19 ===========

FIG0_19::FIG0_19(FIGRuntimeInformation *rti) :
    m_rti(rti)
{ }

FillStatus FIG0_19::fill(uint8_t *buf, size_t max_size)
{
    using namespace std;

    auto ensemble = m_rti->ensemble;

    // We are called every 24ms, and must timeout after 2s
    const int timeout = 2000/24;

    m_transition.update_state(timeout, ensemble->clusters);

    FillStatus fs;
    ssize_t remaining = max_size;

    FIGtype0* fig0 = NULL;

    // Combine all clusters into one list
    set<AnnouncementCluster*> allclusters;
    for (const auto& cluster : m_transition.new_entries) {
        allclusters.insert(cluster.first.get());
    }
    for (const auto& cluster : m_transition.repeated_entries) {
        allclusters.insert(cluster.get());
    }
    for (const auto& cluster : m_transition.disabled_entries) {
        allclusters.insert(cluster.first.get());
    }

    const int length_0_19 = 4;
    fs.complete_fig_transmitted = true;
    for (auto& cluster : allclusters) {

        if (fig0 == NULL) {
            if (remaining < 2 + length_0_19) {
                fs.complete_fig_transmitted = false;
                break;
            }

            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN = 0;
            fig0->OE = 0;
            fig0->PD = 0;
            fig0->Extension = 19;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < length_0_19) {
            fs.complete_fig_transmitted = false;
            break;
        }


        auto fig0_19 = (FIGtype0_19*)buf;
        fig0_19->ClusterId = cluster->cluster_id;
        if (cluster->is_active()) {
            fig0_19->ASw = htons(cluster->flags);
        }
        else {
            fig0_19->ASw = 0;
        }
        fig0_19->NewFlag = 1;
        fig0_19->RegionFlag = 0;
        fig0_19->SubChId = 0;
        bool found = false;

        for (const auto& subchannel : ensemble->subchannels) {
            if (subchannel->uid == cluster->subchanneluid) {
                fig0_19->SubChId = subchannel->id;
                found = true;
                break;
            }
        }
        if (not found) {
            etiLog.level(warn) << "FIG0/19: could not find subchannel " <<
                cluster->subchanneluid << " for cluster " <<
                (int)cluster->cluster_id;
            continue;
        }

        fig0->Length += length_0_19;
        buf += length_0_19;
        remaining -= length_0_19;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

template<class T>
void TransitionHandler<T>::update_state(int timeout, std::vector<std::shared_ptr<T> > all_entries)
{
    for (const auto& entry : all_entries) {
        if (entry->is_active()) {
            if (repeated_entries.count(entry) > 0) {
                // We are currently announcing this entry
                continue;
            }

            if (new_entries.count(entry) > 0) {
                // We are currently announcing this entry at a
                // fast rate. Handle timeout:
                new_entries[entry] -= 1;
                if (new_entries[entry] <= 0) {
                    repeated_entries.insert(entry);
                    new_entries.erase(entry);
                }
                continue;
            }

            // unlikely
            if (disabled_entries.count(entry) > 0) {
                new_entries[entry] = timeout;
                disabled_entries.erase(entry);
                continue;
            }

            // It's a new entry!
            new_entries[entry] = timeout;
        }
        else { // Not active
            if (disabled_entries.count(entry) > 0) {
                disabled_entries[entry] -= 1;
                if (disabled_entries[entry] <= 0) {
                    disabled_entries.erase(entry);
                }
                continue;
            }

            if (repeated_entries.count(entry) > 0) {
                // We are currently announcing this entry
                disabled_entries[entry] = timeout;
                repeated_entries.erase(entry);
                continue;
            }

            // unlikely
            if (new_entries.count(entry) > 0) {
                // We are currently announcing this entry at a
                // fast rate. We must stop announcing it
                disabled_entries[entry] = timeout;
                new_entries.erase(entry);
                continue;
            }
        }
    }
}

FIG_rate FIG0_19::repetition_rate(void)
{
    if (    m_transition.new_entries.size() > 0 or
            m_transition.disabled_entries.size() ) {
        return FIG_rate::A_B;
    }
    else {
        return FIG_rate::B;
    }
}

} // namespace FIC

