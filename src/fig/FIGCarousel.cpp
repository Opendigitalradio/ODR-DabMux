/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of the FIG carousel to schedule the FIGs into the
   FIBs.
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

#include "fig/FIGCarousel.h"
#include "fig/FIG0.h"

FIGCarousel::FIGCarousel(boost::shared_ptr<dabEnsemble> ensemble)
{
    m_rti.ensemble = ensemble;
    m_rti.currentFrame = 0;
    m_figs.emplace_back<FIG0_0>(&m_rti);
    m_figs.emplace_back<FIG0_1>(&m_rti);
}

void FIGCarousel::set_currentFrame(unsigned long currentFrame)
{
    m_rti.currentFrame = currentFrame;
}

#if 0
void fib0 {
    switch (insertFIG) {

        case 0:
        case 4:
        case 8:
        case 12:
            // FIG type 0/0, Multiplex Configuration Info (MCI),
            //  Ensemble information
            fig0_0 = (FIGtype0_0 *) & etiFrame[index];

            fig0_0->FIGtypeNumber = 0;
            fig0_0->Length = 5;
            fig0_0->CN = 0;
            fig0_0->OE = 0;
            fig0_0->PD = 0;
            fig0_0->Extension = 0;

            fig0_0->EId = htons(ensemble->id);
            fig0_0->Change = 0;
            fig0_0->Al = 0;
            fig0_0->CIFcnt_hight = (currentFrame / 250) % 20;
            fig0_0->CIFcnt_low = (currentFrame % 250);
            index = index + 6;
            figSize += 6;

            break;

        case 1:
        case 6:
        case 10:
        case 13:
            // FIG type 0/1, MIC, Sub-Channel Organization,
            // one instance of the part for each subchannel
            figtype0_1 = (FIGtype0_1 *) & etiFrame[index];

            figtype0_1->FIGtypeNumber = 0;
            figtype0_1->Length = 1;
            figtype0_1->CN = 0;
            figtype0_1->OE = 0;
            figtype0_1->PD = 0;
            figtype0_1->Extension = 1;
            index = index + 2;
            figSize += 2;

            // Rotate through the subchannels until there is no more
            // space in the FIG0/1
            if (subchannelFIG0_1 == ensemble->subchannels.end()) {
                subchannelFIG0_1 = ensemble->subchannels.begin();
            }

            for (; subchannelFIG0_1 != ensemble->subchannels.end();
                    ++subchannelFIG0_1) {
                dabProtection* protection = &(*subchannelFIG0_1)->protection;

                if ( (protection->form == UEP && figSize > 27) ||
                        (protection->form == EEP && figSize > 26) ) {
                    break;
                }

                if (protection->form == UEP) {
                    fig0_1subchShort =
                        (FIG_01_SubChannel_ShortF*) &etiFrame[index];
                    fig0_1subchShort->SubChId = (*subchannelFIG0_1)->id;

                    fig0_1subchShort->StartAdress_high =
                        (*subchannelFIG0_1)->startAddress / 256;
                    fig0_1subchShort->StartAdress_low =
                        (*subchannelFIG0_1)->startAddress % 256;

                    fig0_1subchShort->Short_Long_form = 0;
                    fig0_1subchShort->TableSwitch = 0;
                    fig0_1subchShort->TableIndex =
                        protection->uep.tableIndex;

                    index = index + 3;
                    figSize += 3;
                    figtype0_1->Length += 3;
                }
                else if (protection->form == EEP) {
                    fig0_1subchLong1 =
                        (FIG_01_SubChannel_LongF*) &etiFrame[index];
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

                    index = index + 4;
                    figSize += 4;
                    figtype0_1->Length += 4;
                }
            }
            break;

        case 2:
        case 9:
        case 11:
        case 14:
            // FIG type 0/2, MCI, Service Organization, one instance of
            // FIGtype0_2_Service for each subchannel
            fig0_2 = NULL;
            cur = 0;

            // Rotate through the subchannels until there is no more
            // space in the FIG0/1
            if (serviceProgFIG0_2 == ensemble->services.end()) {
                serviceProgFIG0_2 = ensemble->services.begin();
            }

            for (; serviceProgFIG0_2 != ensemble->services.end();
                    ++serviceProgFIG0_2) {
                if (!(*serviceProgFIG0_2)->nbComponent(ensemble->components)) {
                    continue;
                }

                if ((*serviceProgFIG0_2)->getType(ensemble) != 0) {
                    continue;
                }

                ++cur;

                if (fig0_2 == NULL) {
                    fig0_2 = (FIGtype0_2 *) & etiFrame[index];

                    fig0_2->FIGtypeNumber = 0;
                    fig0_2->Length = 1;
                    fig0_2->CN = 0;
                    fig0_2->OE = 0;
                    fig0_2->PD = 0;
                    fig0_2->Extension = 2;
                    index = index + 2;
                    figSize += 2;
                }

                if (figSize + 3
                        + (*serviceProgFIG0_2)->nbComponent(ensemble->components)
                        * 2 > 30) {
                    break;
                }

                fig0_2serviceAudio = (FIGtype0_2_Service*) &etiFrame[index];

                fig0_2serviceAudio->SId = htons((*serviceProgFIG0_2)->id);
                fig0_2serviceAudio->Local_flag = 0;
                fig0_2serviceAudio->CAId = 0;
                fig0_2serviceAudio->NbServiceComp =
                    (*serviceProgFIG0_2)->nbComponent(ensemble->components);
                index += 3;
                fig0_2->Length += 3;
                figSize += 3;

                int curCpnt = 0;
                for (component = getComponent(ensemble->components,
                            (*serviceProgFIG0_2)->id);
                        component != ensemble->components.end();
                        component = getComponent(ensemble->components,
                            (*serviceProgFIG0_2)->id, component)) {
                    subchannel = getSubchannel(ensemble->subchannels,
                            (*component)->subchId);

                    if (subchannel == ensemble->subchannels.end()) {
                        etiLog.log(error,
                                "Subchannel %i does not exist for component "
                                "of service %i\n",
                                (*component)->subchId, (*component)->serviceId);
                        throw MuxInitException();
                    }

                    switch ((*subchannel)->type) {
                        case Audio:
                            audio_description =
                                (FIGtype0_2_audio_component*)&etiFrame[index];
                            audio_description->TMid    = 0;
                            audio_description->ASCTy   = (*component)->type;
                            audio_description->SubChId = (*subchannel)->id;
                            audio_description->PS      = ((curCpnt == 0) ? 1 : 0);
                            audio_description->CA_flag = 0;
                            break;
                        case DataDmb:
                            data_description =
                                (FIGtype0_2_data_component*)&etiFrame[index];
                            data_description->TMid    = 1;
                            data_description->DSCTy   = (*component)->type;
                            data_description->SubChId = (*subchannel)->id;
                            data_description->PS      = ((curCpnt == 0) ? 1 : 0);
                            data_description->CA_flag = 0;
                            break;
                        case Packet:
                            packet_description =
                                (FIGtype0_2_packet_component*)&etiFrame[index];
                            packet_description->TMid    = 3;
                            packet_description->setSCId((*component)->packet.id);
                            packet_description->PS      = ((curCpnt == 0) ? 1 : 0);
                            packet_description->CA_flag = 0;
                            break;
                        default:
                            etiLog.log(error,
                                    "Component type not supported\n");
                            throw MuxInitException();
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.log(error,
                                "Sorry, no space left in FIG 0/2 to insert "
                                "component %i of program service %i.\n",
                                curCpnt, cur);
                        throw MuxInitException();
                    }
                    ++curCpnt;
                }
            }
            break;

        case 3:
            fig0_2 = NULL;
            cur = 0;

            if (serviceDataFIG0_2 == ensemble->services.end()) {
                serviceDataFIG0_2 = ensemble->services.begin();
            }
            for (; serviceDataFIG0_2 != ensemble->services.end();
                    ++serviceDataFIG0_2) {
                if (!(*serviceDataFIG0_2)->nbComponent(ensemble->components)) {
                    continue;
                }

                unsigned char type = (*serviceDataFIG0_2)->getType(ensemble);
                if ((type == 0) || (type == 2)) {
                    continue;
                }

                ++cur;

                if (fig0_2 == NULL) {
                    fig0_2 = (FIGtype0_2 *) & etiFrame[index];

                    fig0_2->FIGtypeNumber = 0;
                    fig0_2->Length = 1;
                    fig0_2->CN = 0;
                    fig0_2->OE = 0;
                    fig0_2->PD = 1;
                    fig0_2->Extension = 2;
                    index = index + 2;
                    figSize += 2;
                }

                if (figSize + 5
                        + (*serviceDataFIG0_2)->nbComponent(ensemble->components)
                        * 2 > 30) {
                    break;
                }

                fig0_2serviceData =
                    (FIGtype0_2_Service_data*) &etiFrame[index];

                fig0_2serviceData->SId = htonl((*serviceDataFIG0_2)->id);
                fig0_2serviceData->Local_flag = 0;
                fig0_2serviceData->CAId = 0;
                fig0_2serviceData->NbServiceComp =
                    (*serviceDataFIG0_2)->nbComponent(ensemble->components);
                fig0_2->Length += 5;
                index += 5;
                figSize += 5;

                int curCpnt = 0;
                for (component = getComponent(ensemble->components,
                            (*serviceDataFIG0_2)->id);
                        component != ensemble->components.end();
                        component = getComponent(ensemble->components,
                            (*serviceDataFIG0_2)->id, component)) {
                    subchannel = getSubchannel(ensemble->subchannels,
                            (*component)->subchId);
                    if (subchannel == ensemble->subchannels.end()) {
                        etiLog.log(error,
                                "Subchannel %i does not exist for component "
                                "of service %i\n",
                                (*component)->subchId, (*component)->serviceId);
                        throw MuxInitException();
                    }

                    switch ((*subchannel)->type) {
                        case Audio:
                            audio_description =
                                (FIGtype0_2_audio_component*)&etiFrame[index];
                            audio_description->TMid = 0;
                            audio_description->ASCTy = (*component)->type;
                            audio_description->SubChId = (*subchannel)->id;
                            audio_description->PS = ((curCpnt == 0) ? 1 : 0);
                            audio_description->CA_flag = 0;
                            break;
                        case DataDmb:
                            data_description =
                                (FIGtype0_2_data_component*)&etiFrame[index];
                            data_description->TMid = 1;
                            data_description->DSCTy = (*component)->type;
                            data_description->SubChId = (*subchannel)->id;
                            data_description->PS = ((curCpnt == 0) ? 1 : 0);
                            data_description->CA_flag = 0;
                            break;
                        case Packet:
                            packet_description =
                                (FIGtype0_2_packet_component*)&etiFrame[index];
                            packet_description->TMid = 3;
                            packet_description->setSCId((*component)->packet.id);
                            packet_description->PS = ((curCpnt == 0) ? 1 : 0);
                            packet_description->CA_flag = 0;
                            break;
                        default:
                            etiLog.log(error,
                                    "Component type not supported\n");
                            throw MuxInitException();
                    }
                    index += 2;
                    fig0_2->Length += 2;
                    figSize += 2;
                    if (figSize > 30) {
                        etiLog.log(error,
                                "Sorry, no place left in FIG 0/2 to insert "
                                "component %i of data service %i.\n",
                                curCpnt, cur);
                        throw MuxInitException();
                    }
                    ++curCpnt;
                }
            }
            break;

        case 5:
            fig0_3_header = NULL;

            for (component = ensemble->components.begin();
                    component != ensemble->components.end();
                    ++component) {
                subchannel = getSubchannel(ensemble->subchannels,
                        (*component)->subchId);

                if (subchannel == ensemble->subchannels.end()) {
                    etiLog.log(error,
                            "Subchannel %i does not exist for component "
                            "of service %i\n",
                            (*component)->subchId, (*component)->serviceId);
                    throw MuxInitException();
                }

                if ((*subchannel)->type != Packet)
                    continue;

                if (fig0_3_header == NULL) {
                    fig0_3_header = (FIGtype0_3_header*)&etiFrame[index];
                    fig0_3_header->FIGtypeNumber = 0;
                    fig0_3_header->Length = 1;
                    fig0_3_header->CN = 0;
                    fig0_3_header->OE = 0;
                    fig0_3_header->PD = 0;
                    fig0_3_header->Extension = 3;

                    index += 2;
                    figSize += 2;
                }

                bool factumAnalyzer = m_pt.get("general.writescca", false);

                /*  Warning: When bit SCCA_flag is unset(0), the multiplexer
                 *  R&S does not send the SCCA field. But, in the Factum ETI
                 *  analyzer, if this field is not there, it is an error.
                 */
                fig0_3_data = (FIGtype0_3_data*)&etiFrame[index];
                fig0_3_data->setSCId((*component)->packet.id);
                fig0_3_data->rfa = 0;
                fig0_3_data->SCCA_flag = 0;
                // if 0, datagroups are used
                fig0_3_data->DG_flag = !(*component)->packet.datagroup;
                fig0_3_data->rfu = 0;
                fig0_3_data->DSCTy = (*component)->type;
                fig0_3_data->SubChId = (*subchannel)->id;
                fig0_3_data->setPacketAddress((*component)->packet.address);
                if (factumAnalyzer) {
                    fig0_3_data->SCCA = 0;
                }

                fig0_3_header->Length += 5;
                index += 5;
                figSize += 5;
                if (factumAnalyzer) {
                    fig0_3_header->Length += 2;
                    index += 2;
                    figSize += 2;
                }

                if (figSize > 30) {
                    etiLog.log(error,
                            "can't add to Fic Fig 0/3, "
                            "too much packet service\n");
                    throw MuxInitException();
                }
            }
            break;

        case 7:
            fig0 = NULL;
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
                    fig0 = (FIGtype0*)&etiFrame[index];
                    fig0->FIGtypeNumber = 0;
                    fig0->Length = 1;
                    fig0->CN = 0;
                    fig0->OE = 0;
                    fig0->PD = 0;
                    fig0->Extension = 17;
                    index += 2;
                    figSize += 2;
                }

                if ((*serviceFIG0_17)->language == 0) {
                    if (figSize + 4 > 30) {
                        break;
                    }
                }
                else {
                    if (figSize + 5 > 30) {
                        break;
                    }
                }

                programme =
                    (FIGtype0_17_programme*)&etiFrame[index];
                programme->SId = htons((*serviceFIG0_17)->id);
                programme->SD = 1;
                programme->PS = 0;
                programme->L = (*serviceFIG0_17)->language != 0;
                programme->CC = 0;
                programme->Rfa = 0;
                programme->NFC = 0;
                if ((*serviceFIG0_17)->language == 0) {
                    etiFrame[index + 3] = (*serviceFIG0_17)->pty;
                    fig0->Length += 4;
                    index += 4;
                    figSize += 4;
                }
                else {
                    etiFrame[index + 3] = (*serviceFIG0_17)->language;
                    etiFrame[index + 4] = (*serviceFIG0_17)->pty;
                    fig0->Length += 5;
                    index += 5;
                    figSize += 5;
                }
            }
            break;
    }
}
#endif
