/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
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
#include "DabMultiplexer.h"

#define PACKED __attribute__ ((packed))

//=========== FIG 0/0 ===========

size_t FIG0_0::fill(uint8_t *buf, size_t max_size)
{
    if (max_size < 6) {
        return 0;
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

    return 6;
}


//=========== FIG 0/1 ===========

FIG0_1::FIG0_1(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
    subchannelFIG0_1 = m_rti->ensemble->subchannels.end();
}

size_t FIG0_1::fill(uint8_t *buf, size_t max_size)
{
    size_t remaining = max_size;

    if (max_size < 6) {
        return 0;
    }

    auto ensemble = m_rti->ensemble;

    FIGtype0_1 *figtype0_1;
    figtype0_1 = (FIGtype0_1*)buf;

    figtype0_1->FIGtypeNumber = 0;
    figtype0_1->Length = 1;
    figtype0_1->CN = 0;
    figtype0_1->OE = 0;
    figtype0_1->PD = 0;
    figtype0_1->Extension = 1;
    buf += 2;
    remaining -= 2;

    // Rotate through the subchannels until there is no more
    // space in the FIG0/1
    if (subchannelFIG0_1 == ensemble->subchannels.end()) {
        subchannelFIG0_1 = ensemble->subchannels.begin();
    }

    for (; subchannelFIG0_1 != ensemble->subchannels.end();
            ++subchannelFIG0_1) {
        dabProtection* protection = &(*subchannelFIG0_1)->protection;

        if ( (protection->form == UEP && remaining < 3) ||
             (protection->form == EEP && remaining < 4) ) {
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
                getSizeCu(*subchannelFIG0_1) / 256;
            fig0_1subchLong1->Sub_ChannelSize_low =
                getSizeCu(*subchannelFIG0_1) % 256;

            buf += 4;
            remaining -= 4;
            figtype0_1->Length += 4;
        }
    }

    return max_size - remaining;
}


//=========== FIG 0/2 ===========

FIG0_2::FIG0_2(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
    serviceFIG0_2 = m_rti->ensemble->services.end();
}

size_t FIG0_2::fill(uint8_t *buf, size_t max_size)
{
    FIGtype0_2 *fig0_2 = NULL;
    int cur = 0;
    ssize_t remaining = max_size;

    auto ensemble = m_rti->ensemble;

    // Rotate through the subchannels until there is no more
    // space
    if (serviceFIG0_2 == ensemble->services.end()) {
        serviceFIG0_2 = ensemble->services.begin();
    }

    for (; serviceFIG0_2 != ensemble->services.end();
            ++serviceFIG0_2) {

        // filter out services which have no components
        if ((*serviceFIG0_2)->nbComponent(ensemble->components) == 0) {
            continue;
        }

        // Exclude Fidc type services, TODO why ?
        auto type = (*serviceFIG0_2)->getType(ensemble);
        if (type == Fidc) {
            continue;
        }

        ++cur;

        if (fig0_2 == NULL) {
            fig0_2 = (FIGtype0_2 *)buf;

            fig0_2->FIGtypeNumber = 0;
            fig0_2->Length = 1;
            fig0_2->CN = 0;
            fig0_2->OE = 0;
            fig0_2->PD = (type == Audio) ? 0 : 1;
            fig0_2->Extension = 2;
            buf += 2;
            remaining -= 2;
        }

        if (type == Audio and
                remaining < 3 + 2 *
                (*serviceFIG0_2)->nbComponent(ensemble->components)) {
            break;
        }

        if (type != Audio and
                remaining < 5 + 2 *
                (*serviceFIG0_2)->nbComponent(ensemble->components)) {
            break;
        }

        if (type == Audio) {
            auto fig0_2serviceAudio = (FIGtype0_2_Service*)buf;

            fig0_2serviceAudio->SId = htons((*serviceFIG0_2)->id);
            fig0_2serviceAudio->Local_flag = 0;
            fig0_2serviceAudio->CAId = 0;
            fig0_2serviceAudio->NbServiceComp =
                (*serviceFIG0_2)->nbComponent(ensemble->components);
            buf += 3;
            fig0_2->Length += 3;
            remaining -= 3;
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

            if (subchannel == ensemble->subchannels.end()) {
                etiLog.log(error,
                        "Subchannel %i does not exist for component "
                        "of service %i\n",
                        (*component)->subchId, (*component)->serviceId);
                throw MuxInitException();
            }

            switch ((*subchannel)->type) {
                case Audio:
                    {
                        auto audio_description = (FIGtype0_2_audio_component*)buf;
                        audio_description->TMid    = 0;
                        audio_description->ASCTy   = (*component)->type;
                        audio_description->SubChId = (*subchannel)->id;
                        audio_description->PS      = ((curCpnt == 0) ? 1 : 0);
                        audio_description->CA_flag = 0;
                    }
                    break;
                case DataDmb:
                    {
                        auto data_description = (FIGtype0_2_data_component*)buf;
                        data_description->TMid    = 1;
                        data_description->DSCTy   = (*component)->type;
                        data_description->SubChId = (*subchannel)->id;
                        data_description->PS      = ((curCpnt == 0) ? 1 : 0);
                        data_description->CA_flag = 0;
                    }
                    break;
                case Packet:
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
        }
    }
    return max_size - remaining;
}


//=========== FIG 0/3 ===========

FIG0_3::FIG0_3(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
}

size_t FIG0_3::fill(uint8_t *buf, size_t max_size)
{
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    FIGtype0_3_header *fig0_3_header = NULL;
    FIGtype0_3_data *fig0_3_data = NULL;

    for (auto& component : ensemble->components) {
        auto subchannel = getSubchannel(ensemble->subchannels,
                component->subchId);

        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    component->subchId, component->serviceId);
            throw MuxInitException();
        }

        if ((*subchannel)->type != Packet)
            continue;

        if (fig0_3_header == NULL) {
            fig0_3_header = (FIGtype0_3_header*)buf;
            fig0_3_header->FIGtypeNumber = 0;
            fig0_3_header->Length = 1;
            fig0_3_header->CN = 0;
            fig0_3_header->OE = 0;
            fig0_3_header->PD = 0;
            fig0_3_header->Extension = 3;

            buf += 2;
            remaining -= 2;
        }

        /*  Warning: When bit SCCA_flag is unset(0), the multiplexer
         *  R&S does not send the SCCA field. But, in the Factum ETI
         *  analyzer, if this field is not there, it is an error.
         */
        fig0_3_data = (FIGtype0_3_data*)buf;
        fig0_3_data->setSCId(component->packet.id);
        fig0_3_data->rfa = 0;
        fig0_3_data->SCCA_flag = 0;
        // if 0, datagroups are used
        fig0_3_data->DG_flag = !component->packet.datagroup;
        fig0_3_data->rfu = 0;
        fig0_3_data->DSCTy = component->type;
        fig0_3_data->SubChId = (*subchannel)->id;
        fig0_3_data->setPacketAddress(component->packet.address);
        if (m_rti->factumAnalyzer) {
            fig0_3_data->SCCA = 0;
        }

        fig0_3_header->Length += 5;
        buf += 5;
        remaining -= 5;
        if (m_rti->factumAnalyzer) {
            fig0_3_header->Length += 2;
            buf += 2;
            remaining -= 2;
        }

        if (remaining < 0) {
            etiLog.level(error) <<
                    "can't add FIG 0/3 to FIB, "
                    "too many packet services";
            throw MuxInitException();
        }
    }

    return max_size - remaining;
}


//=========== FIG 0/17 ===========

FIG0_17::FIG0_17(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
    serviceFIG0_17 = m_rti->ensemble->services.end();
}

size_t FIG0_17::fill(uint8_t *buf, size_t max_size)
{
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    FIGtype0* fig0 = NULL;

    if (serviceFIG0_17 == ensemble->services.end()) {
        serviceFIG0_17 = ensemble->services.begin();
    }
    for (; serviceFIG0_17 != ensemble->services.end();
            ++serviceFIG0_17) {

        if (    (*serviceFIG0_17)->pty == 0 &&
                (*serviceFIG0_17)->language == 0) {
            continue;
        }

        if (fig0 == NULL) {
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

        if ((*serviceFIG0_17)->language == 0) {
            if (remaining < 4) {
                break;
            }
        }
        else {
            if (remaining < 5) {
                break;
            }
        }

        auto programme = (FIGtype0_17_programme*)buf;
        programme->SId = htons((*serviceFIG0_17)->id);
        programme->SD = 1;
        programme->PS = 0;
        programme->L = (*serviceFIG0_17)->language != 0;
        programme->CC = 0;
        programme->Rfa = 0;
        programme->NFC = 0;
        if ((*serviceFIG0_17)->language == 0) {
            buf[3] = (*serviceFIG0_17)->pty;
            fig0->Length += 4;
            buf += 4;
            remaining -= 4;
        }
        else {
            buf[3] = (*serviceFIG0_17)->language;
            buf[4] = (*serviceFIG0_17)->pty;
            fig0->Length += 5;
            buf += 5;
            remaining -= 5;
        }
    }

    return max_size - remaining;
}

