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

#include <set>
#include <memory>
#include "DabMultiplexer.h"
#include "ConfigParser.h"
#include "fig/FIG.h"

using namespace std;

// Protection levels and bitrates for UEP.
const unsigned char ProtectionLevelTable[64] = {
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 2, 1, 0,
    4, 3, 1,
    4, 2, 0
};

const unsigned short BitRateTable[64] = {
    32, 32, 32, 32, 32,
    48, 48, 48, 48, 48,
    56, 56, 56, 56,
    64, 64, 64, 64, 64,
    80, 80, 80, 80, 80,
    96, 96, 96, 96, 96,
    112, 112, 112, 112,
    128, 128, 128, 128, 128,
    160, 160, 160, 160, 160,
    192, 192, 192, 192, 192,
    224, 224, 224, 224, 224,
    256, 256, 256, 256, 256,
    320, 320, 320,
    384, 384, 384
};

DabMultiplexer::DabMultiplexer(
        boost::property_tree::ptree pt) :
    RemoteControllable("mux"),
    m_pt(pt),
    timestamp(0),
    MNSC_increment_time(false),
    sync(0x49C5F8),
    currentFrame(0),
    ensemble(std::make_shared<dabEnsemble>()),
    fig_carousel(ensemble)
{
    RC_ADD_PARAMETER(frames,
            "Show number of frames generated [read-only]");
}

void DabMultiplexer::set_edi_config(const edi_configuration_t& new_edi_conf)
{
    edi_conf = new_edi_conf;

#if HAVE_OUTPUT_EDI
    if (edi_conf.verbose) {
        etiLog.log(info, "Setup EDI");
    }

    if (edi_conf.dump) {
        edi_debug_file.open("./edi.debug");
    }

    if (edi_conf.enabled()) {
        for (auto& edi_destination : edi_conf.destinations) {
            auto edi_output = std::make_shared<UdpSocket>(edi_destination.source_port);

            if (not edi_destination.source_addr.empty()) {
                int err = edi_output->setMulticastSource(edi_destination.source_addr.c_str());
                if (err) {
                    etiLog.level(error) << "EDI socket set source failed!";
                    throw MuxInitException();
                }
                err = edi_output->setMulticastTTL(edi_destination.ttl);
                if (err) {
                    etiLog.level(error) << "EDI socket set TTL failed!";
                    throw MuxInitException();
                }
            }

            edi_destination.socket = edi_output;
        }
    }

    if (edi_conf.verbose) {
        etiLog.log(info, "EDI set up");
    }

    // The AF Packet will be protected with reed-solomon and split in fragments
    PFT pft(edi_conf);
    edi_pft = pft;
#endif
}


// Run a set of checks on the configuration
void DabMultiplexer::prepare()
{
    parse_ptree(m_pt, ensemble);

    rcs.enrol(this);
    rcs.enrol(ensemble.get());

    prepare_subchannels();
    prepare_services_components();
    prepare_data_inputs();

    if (ensemble->subchannels.size() == 0) {
        etiLog.log(error, "can't multiplex no subchannel!\n");
        throw MuxInitException();
    }

    vector<DabSubchannel*>::iterator subchannel =
        ensemble->subchannels.end() - 1;

    if ((*subchannel)->startAddress + (*subchannel)->getSizeCu() > 864) {
        etiLog.log(error, "Total size in CU exceeds 864\n");
        printSubchannels(ensemble->subchannels);
        throw MuxInitException();
    }

    /* TODO:
     * In a SFN, when reconfiguring the ensemble, the multiplexer
     * has to be restarted (odr-dabmux doesn't support reconfiguration).
     * Ideally, we must be able to restart transmission s.t. the receiver
     * synchronisation is preserved.
     */
    gettimeofday(&mnsc_time, nullptr);

#if HAVE_OUTPUT_EDI
    edi_time = chrono::system_clock::now();

    // Try to load offset once

    bool tist_enabled = m_pt.get("general.tist", false);

    if (tist_enabled and edi_conf.enabled()) {
        try {
            m_clock_tai.get_offset();
        }
        catch (std::runtime_error& e) {
            const char* err_msg =
                "Could not initialise TAI clock properly required by "
                "EDI with timestamp. Do you have a working internet "
                "connection?";

            etiLog.level(error) << err_msg;
            throw e;
        }
    }
#endif

    // Shift ms by 14 to Timestamp level 2, see below in Section TIST
    timestamp = (mnsc_time.tv_usec / 1000) << 14;
}


// Check and adjust subchannels
void DabMultiplexer::prepare_subchannels()
{
    set<unsigned char> ids;

    for (auto subchannel : ensemble->subchannels) {
        if (ids.find(subchannel->id) != ids.end()) {
            etiLog.log(error,
                    "Subchannel %u is set more than once!\n",
                    subchannel->id);
            throw MuxInitException();
        }
        ids.insert(subchannel->id);
    }
}

// Check and adjust services and components
void DabMultiplexer::prepare_services_components()
{
    set<uint32_t> ids;
    dabProtection* protection = nullptr;

    vector<DabComponent*>::iterator component;
    vector<DabSubchannel*>::iterator subchannel;

    for (auto service : ensemble->services) {
        if (ids.find(service->id) != ids.end()) {
            etiLog.log(error,
                    "Service id 0x%x (%u) is set more than once!\n",
                    service->id, service->id);
            throw MuxInitException();
        }

        // Get first component of this service
        component = getComponent(ensemble->components, service->id);
        if (component == ensemble->components.end()) {
            etiLog.log(error,
                    "Service id 0x%x (%u) includes no component!\n",
                    service->id, service->id);
            throw MuxInitException();
        }

        rcs.enrol(service.get());

        // Adjust components type for DAB+
        while (component != ensemble->components.end()) {
            subchannel =
                getSubchannel(ensemble->subchannels, (*component)->subchId);
            if (subchannel == ensemble->subchannels.end()) {
                etiLog.log(error, "Error, service %u component "
                        "links to the invalid subchannel %u\n",
                        (*component)->serviceId, (*component)->subchId);
                throw MuxInitException();
            }

            protection = &(*subchannel)->protection;
            switch ((*subchannel)->type) {
                case subchannel_type_t::Audio:
                    {
                        if (protection->form == EEP) {
                            /* According to ETSI TS 102 563 Clause 7.1 FIC signalling:
                             *
                             * "AAC audio services are signalled in the same way
                             * as Layer II audio services with the exception
                             * that the ASCTy carried in FIG 0/2 (see EN 300
                             * 401, clause 6.3.1) is set to the value
                             * 1 1 1 1 1 1."
                             */
                            (*component)->type = 0x3f;
                        }
                    }
                    break;
                case subchannel_type_t::DataDmb:
                case subchannel_type_t::Fidc:
                case subchannel_type_t::Packet:
                    break;
                default:
                    etiLog.log(error,
                            "Error, unknown subchannel type\n");
                    throw MuxInitException();
            }
            component = getComponent(ensemble->components,
                    service->id, component);
        }
    }

    // Init packet components SCId
    int cur_packetid = 0;
    for (auto component : ensemble->components) {
        subchannel = getSubchannel(ensemble->subchannels,
                component->subchId);
        if (subchannel == ensemble->subchannels.end()) {
            etiLog.log(error,
                    "Subchannel %i does not exist for component "
                    "of service %i\n",
                    component->subchId, component->serviceId);
            throw MuxInitException();
        }
        if ((*subchannel)->type != subchannel_type_t::Packet) continue;

        component->packet.id = cur_packetid++;

        rcs.enrol(component);
    }

}

void DabMultiplexer::prepare_data_inputs()
{
    dabProtection* protection = nullptr;
    vector<DabSubchannel*>::iterator subchannel;

    // Prepare and check the data inputs
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        protection = &(*subchannel)->protection;
        if (subchannel == ensemble->subchannels.begin()) {
            (*subchannel)->startAddress = 0;
        } else {
            (*subchannel)->startAddress = (*(subchannel - 1))->startAddress +
                (*(subchannel - 1))->getSizeCu();
        }
        if ((*subchannel)->input->open((*subchannel)->inputUri) == -1) {
            perror((*subchannel)->inputUri.c_str());
            throw MuxInitException();
        }

        // TODO Check errors
        int subch_bitrate = (*subchannel)->input->setBitrate( (*subchannel)->bitrate);
        if (subch_bitrate <= 0) {
            etiLog.level(error) << "can't set bitrate for source " <<
                (*subchannel)->inputUri;
            throw MuxInitException();
        }
        (*subchannel)->bitrate = subch_bitrate;

        /* Use EEP unless we find a UEP configuration
         * UEP is only used for MPEG audio, but some bitrates don't
         * have a UEP profile (EN 300 401 Clause 6.2.1).
         * For these bitrates, we must switch to EEP.
         *
         * AAC audio and data is already EEP
         */
        if (protection->form == UEP) {
            protection->form = EEP;
            for (int i = 0; i < 64; i++) {
                if ( (*subchannel)->bitrate == BitRateTable[i] &&
                        protection->level == ProtectionLevelTable[i] ) {
                    protection->form = UEP;
                    protection->uep.tableIndex = i;
                }
            }
        }

        /* EEP B can only be used for subchannels with bitrates
         * multiple of 32kbit/s
         */
        if ( protection->form == EEP &&
             protection->eep.profile == EEP_B &&
             subch_bitrate % 32 != 0 ) {
            etiLog.level(error) <<
                "Cannot use EEP_B protection for subchannel " <<
                (*subchannel)->inputUri <<
                ": bitrate not multiple of 32kbit/s";
            throw MuxInitException();
        }
    }
}

/*  Each call creates one ETI frame */
void DabMultiplexer::mux_frame(std::vector<std::shared_ptr<DabOutput> >& outputs)
{
    unsigned char etiFrame[6144];
    unsigned short index = 0;

    vector<std::shared_ptr<DabService> >::iterator service;
    vector<DabComponent*>::iterator component;
    vector<DabSubchannel*>::iterator subchannel;

    // FIC Length, DAB Mode I, II, IV -> FICL = 24, DAB Mode III -> FICL = 32
    unsigned FICL  = (ensemble->mode == 3 ? 32 : 24);

    // For EDI, save ETI(LI) Management data into a TAG Item DETI
    TagDETI edi_tagDETI;
    TagStarPTR edi_tagStarPtr;
    map<DabSubchannel*, TagESTn> edi_subchannelToTag;

    // The above Tag Items will be assembled into a TAG Packet
    TagPacket edi_tagpacket(edi_conf.tagpacket_alignment);

    update_dab_time();

    // Initialise the ETI frame
    memset(etiFrame, 0, 6144);

    /**********************************************************************
     **********   Section SYNC of ETI(NI, G703)   *************************
     **********************************************************************/

    // See ETS 300 799 Clause 6
    eti_SYNC *etiSync = (eti_SYNC *) etiFrame;

    etiSync->ERR = edi_tagDETI.stat = 0xFF; // ETS 300 799, 5.2, no error

    //****** Field FSYNC *****//
    // See ETS 300 799, 6.2.1.2
    sync ^= 0xffffff;
    etiSync->FSYNC = sync;

    /**********************************************************************
     ***********   Section LIDATA of ETI(NI, G703)   **********************
     **********************************************************************/

    // See ETS 300 799 Figure 5 for a better overview of these fields.

    //****** Section FC ***************************************************/
    // 4 octets, starts at offset 4
    eti_FC *fc = (eti_FC *) &etiFrame[4];

    //****** FCT ******//
    // Incremente for each frame, overflows at 249
    fc->FCT = currentFrame % 250;
    edi_tagDETI.dflc = currentFrame % 5000;

    //****** FICF ******//
    // Fast Information Channel Flag, 1 bit, =1 if FIC present
    fc->FICF = edi_tagDETI.ficf = 1;

    //****** NST ******//
    /* Number of audio of data sub-channels, 7 bits, 0-64.
     * In the 15-frame period immediately preceding a multiplex
     * re-configuration, NST can take the value 0 (see annex E).
     */
    fc->NST = ensemble->subchannels.size();

    //****** FP ******//
    /* Frame Phase, 3 bit counter, tells the COFDM generator
     * when to insert the TII. Is also used by the MNSC.
     */
    fc->FP = edi_tagDETI.fp = currentFrame & 0x7;

    //****** MID ******//
    //Mode Identity, 2 bits, 01 ModeI, 10 modeII, 11 ModeIII, 00 ModeIV
    fc->MID = edi_tagDETI.mid = ensemble->mode;      //mode 2 needs 3 FIB, 3*32octets = 96octets

    //****** FL ******//
    /* Frame Length, 11 bits, nb of words(4 bytes) in STC, EOH and MST
     * if NST=0, FL=1+FICL words, FICL=24 or 32 depending on the mode.
     * The FL is given in words (4 octets), see ETS 300 799 5.3.6 for details
     */
    unsigned short FLtmp = 1 + FICL + (fc->NST);

    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        // Add STLsbch
        FLtmp += (*subchannel)->getSizeWord();
    }

    fc->setFrameLength(FLtmp);
    index = 8;

    /******* Section STC **************************************************/
    // Stream Characterization,
    //  number of channels * 4 octets = nb octets total
    int edi_stream_id = 1;
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        dabProtection* protection = &(*subchannel)->protection;
        eti_STC *sstc = (eti_STC *) & etiFrame[index];

        sstc->SCID = (*subchannel)->id;
        sstc->startAddress_high = (*subchannel)->startAddress / 256;
        sstc->startAddress_low = (*subchannel)->startAddress % 256;
        // depends on the desired protection form
        if (protection->form == UEP) {
            sstc->TPL = 0x10 |
                ProtectionLevelTable[protection->uep.tableIndex];
        }
        else if (protection->form == EEP) {
            sstc->TPL = 0x20 | (protection->eep.GetOption() << 2) | protection->level;
        }

        // Sub-channel Stream Length, multiple of 64 bits
        sstc->STL_high = (*subchannel)->getSizeDWord() / 256;
        sstc->STL_low = (*subchannel)->getSizeDWord() % 256;

        TagESTn tag_ESTn;
        tag_ESTn.id = edi_stream_id++;
        tag_ESTn.scid = (*subchannel)->id;
        tag_ESTn.sad = (*subchannel)->startAddress;
        tag_ESTn.tpl = sstc->TPL;
        tag_ESTn.rfa = 0; // two bits
        tag_ESTn.mst_length = (*subchannel)->getSizeByte() / 8;
        tag_ESTn.mst_data = nullptr;
        assert((*subchannel)->getSizeByte() % 8 == 0);

        edi_subchannelToTag[*subchannel] = tag_ESTn;
        index += 4;
    }

    /******* Section EOH **************************************************/
    // End of Header 4 octets
    eti_EOH *eoh = (eti_EOH *) & etiFrame[index];

    //MNSC Multiplex Network Signalling Channel, 2 octets

    eoh->MNSC = 0;

    struct tm *time_tm = gmtime(&mnsc_time.tv_sec);
    switch (fc->FP & 0x3)
    {
        case 0:
            if (MNSC_increment_time)
            {
                MNSC_increment_time = false;
                mnsc_time.tv_sec += 1;
            }
            {

                eti_MNSC_TIME_0 *mnsc = (eti_MNSC_TIME_0 *) &eoh->MNSC;
                // Set fields according to ETS 300 799 -- 5.5.1 and A.2.2
                mnsc->type = 0;
                mnsc->identifier = 0;
                mnsc->rfa = 0;
            }
            break;
        case 1:
            {
                eti_MNSC_TIME_1 *mnsc = (eti_MNSC_TIME_1 *) &eoh->MNSC;
                mnsc->setFromTime(time_tm);
                mnsc->accuracy = 1;
                mnsc->sync_to_frame = 1;
            }
            break;
        case 2:
            {
                eti_MNSC_TIME_2 *mnsc = (eti_MNSC_TIME_2 *) &eoh->MNSC;
                mnsc->setFromTime(time_tm);
            }
            break;
        case 3:
            {
                eti_MNSC_TIME_3 *mnsc = (eti_MNSC_TIME_3 *) &eoh->MNSC;
                mnsc->setFromTime(time_tm);
            }
            break;
    }

    edi_tagDETI.mnsc = eoh->MNSC;

    // CRC Cyclic Redundancy Checksum of the FC, STC and MNSC, 2 octets
    unsigned short nbBytesCRC = 4 + ((fc->NST) * 4) + 2;

    unsigned short CRCtmp = 0xFFFF;
    CRCtmp = crc16(CRCtmp, &etiFrame[4], nbBytesCRC);
    CRCtmp ^= 0xffff;
    eoh->CRC = htons(CRCtmp);

    /******* Section MST **************************************************/
    // Main Stream Data, if FICF=1 the first 96 or 128 bytes carry the FIC
    // (depending on mode)
    index = ((fc->NST) + 2 + 1) * 4;
    edi_tagDETI.fic_data = &etiFrame[index];
    edi_tagDETI.fic_length = FICL * 4;

    // Insert all FIBs
    fig_carousel.update(currentFrame);
    const bool fib3_present = ensemble->mode == 3;
    index += fig_carousel.write_fibs(&etiFrame[index], currentFrame % 4, fib3_present);

    /**********************************************************************
     ******  Input Data Reading *******************************************
     **********************************************************************/

    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {

        TagESTn& tag = edi_subchannelToTag[*subchannel];

        int sizeSubchannel = (*subchannel)->getSizeByte();
        int result = (*subchannel)->input->readFrame(
                &etiFrame[index], sizeSubchannel);

        if (result < 0) {
            etiLog.log(info,
                    "Subchannel %d read failed at ETI frame number: %d\n",
                    (*subchannel)->id, currentFrame);
        }

        // save pointer to Audio or Data Stream into correct TagESTn for EDI
        tag.mst_data = &etiFrame[index];

        index += sizeSubchannel;
    }


    index = (3 + fc->NST + FICL) * 4;
    for (subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        index += (*subchannel)->getSizeByte();
    }

    /******* Section EOF **************************************************/
    // End of Frame, 4 octets
    index = (FLtmp + 1 + 1) * 4;
    eti_EOF *eof = (eti_EOF *) & etiFrame[index];

    // CRC of Main Stream data (MST), 16 bits
    index = ((fc->NST) + 2 + 1) * 4;            // MST position

    unsigned short MSTsize = ((FLtmp) - 1 - (fc->NST)) * 4;    // data size

    CRCtmp = 0xffff;
    CRCtmp = crc16(CRCtmp, &etiFrame[index], MSTsize);
    CRCtmp ^= 0xffff;
    eof->CRC = htons(CRCtmp);

    //RFU, Reserved for future use, 2 bytes, should be 0xFFFF
    eof->RFU = htons(0xFFFF);

    /******* Section TIST *************************************************/
    // TimeStamps, 24 bits + 1 octet
    index = (FLtmp + 2 + 1) * 4;
    eti_TIST *tist = (eti_TIST *) & etiFrame[index];

    bool enableTist = m_pt.get("general.tist", false);
    if (enableTist) {
        tist->TIST = htonl(timestamp) | 0xff;
        edi_tagDETI.tsta = timestamp & 0xffffff;
    }
    else {
        tist->TIST = htonl(0xffffff) | 0xff;
        edi_tagDETI.tsta = 0xffffff;
    }

    edi_tagDETI.atstf = 1;
    edi_tagDETI.utco = 0;
    edi_tagDETI.seconds = 0;
#if HAVE_OUTPUT_EDI
    try {
        bool tist_enabled = m_pt.get("general.tist", false);

        if (tist_enabled and edi_conf.enabled()) {
            edi_tagDETI.set_utco(m_clock_tai.get_offset());
            edi_tagDETI.set_seconds(edi_time);
        }

    }
    catch (std::runtime_error& e) {
        etiLog.level(error) << "Could not get UTC-TAI offset for EDI timestamp";
    }
#endif


    /* Coding of the TIST, according to ETS 300 799 Annex C

    Bit number      b0(MSb)..b6     b7..b9   b10..b17   b18..b20   b21..b23(LSb)
    Bit number      b23(MSb)..b17   b16..b14 b13..b6    b5..b3     b2..b0(LSb)
    as uint32_t
    Width           7               3        8          3          3
    Timestamp level 1               2        3          4          5
    Valid range     [0..124], 127   [0..7]   [0..255]   [0..7]     [0..7]
    Approximate     8 ms            1 ms     3,91 us    488 ns     61 ns
    time resolution
    */

    timestamp += 24 << 14; // Shift 24ms by 14 to Timestamp level 2
    if (timestamp > 0xf9FFff)
    {
        timestamp -= 0xfa0000; // Substract 16384000, corresponding to one second

        // Also update MNSC time for next time FP==0
        MNSC_increment_time = true;

        // Immediately update edi time
        edi_time += chrono::seconds(1);
    }



    /********************************************************************** 
     ***********   Section FRPD   *****************************************
     **********************************************************************/

    int frame_size = (FLtmp + 1 + 1 + 1 + 1) * 4;

    // Give the data to the outputs
    for (auto output : outputs) {
        if (output->Write(etiFrame, frame_size) == -1) {
            etiLog.level(error) <<
                "Can't write to output " <<
                output->get_info();
        }
    }

#if HAVE_OUTPUT_EDI
    /********************************************************************** 
     ***********   Finalise and send EDI   ********************************
     **********************************************************************/

    if (edi_conf.enabled()) {
        // put tags *ptr, DETI and all subchannels into one TagPacket
        edi_tagpacket.tag_items.push_back(&edi_tagStarPtr);
        edi_tagpacket.tag_items.push_back(&edi_tagDETI);

        for (auto& tag : edi_subchannelToTag) {
            edi_tagpacket.tag_items.push_back(&tag.second);
        }

        // Assemble into one AF Packet
        AFPacket edi_afpacket = edi_afPacketiser.Assemble(edi_tagpacket);

        if (edi_conf.enable_pft) {
            // Apply PFT layer to AF Packet (Reed Solomon FEC and Fragmentation)
            vector< PFTFragment > edi_fragments =
                edi_pft.Assemble(edi_afpacket);

            // Send over ethernet
            for (const auto& edi_frag : edi_fragments) {
                for (auto& dest : edi_conf.destinations) {
                    InetAddress addr;
                    addr.setAddress(dest.dest_addr.c_str());
                    addr.setPort(edi_conf.dest_port);

                    dest.socket->send(edi_frag, addr);
                }

                if (edi_conf.dump) {
                    std::ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
                    std::copy(edi_frag.begin(), edi_frag.end(), debug_iterator);
                }
            }

            if (edi_conf.verbose) {
                fprintf(stderr, "EDI number of PFT fragments %zu\n",
                        edi_fragments.size());
            }
        }
        else {
            // Send over ethernet
            for (auto& dest : edi_conf.destinations) {
                InetAddress addr;
                addr.setAddress(dest.dest_addr.c_str());
                addr.setPort(edi_conf.dest_port);

                dest.socket->send(edi_afpacket, addr);
            }

            if (edi_conf.dump) {
                std::ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
                std::copy(edi_afpacket.begin(), edi_afpacket.end(), debug_iterator);
            }
        }
    }
#endif // HAVE_OUTPUT_EDI

#if _DEBUG
    /********************************************************************** 
     ***********   Output a small message *********************************
     **********************************************************************/
    if (currentFrame % 100 == 0) {
        if (enableTist) {
            etiLog.log(info, "ETI frame number %i Timestamp: %d + %f\n",
                    currentFrame, mnsc_time.tv_sec,
                    (timestamp & 0xFFFFFF) / 16384000.0);
        }
        else {
            etiLog.log(info, "ETI frame number %i Time: %d, no TIST\n",
                    currentFrame, mnsc_time.tv_sec);
        }
    }
#endif

    currentFrame++;
}

void DabMultiplexer::print_info()
{
    // Print settings before starting
    etiLog.log(info, "--- Multiplex configuration ---");
    printEnsemble(ensemble);

    etiLog.log(info, "--- Subchannels list ---");
    printSubchannels(ensemble->subchannels);

    etiLog.log(info, "--- Services list ---");
    printServices(ensemble->services);

    etiLog.log(info, "--- Components list ---");
    printComponents(ensemble->components);
}


void DabMultiplexer::set_parameter(const std::string& parameter,
               const std::string& value)
{
    if (parameter == "frames") {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' of " << get_rc_name() <<
            " is read-only";
        throw ParameterError(ss.str());
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

/* Getting a parameter always returns a string. */
const std::string DabMultiplexer::get_parameter(const std::string& parameter) const
{
    stringstream ss;
    if (parameter == "frames") {
        ss << currentFrame;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

