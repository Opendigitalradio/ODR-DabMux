/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2024
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

static vector<string> split_pipe_separated_string(const std::string& s)
{
    stringstream ss;
    ss << s;

    string elem;
    vector<string> components;
    while (getline(ss, elem, '|')) {
        components.push_back(elem);
    }
    return components;
}

DabMultiplexer::DabMultiplexer(
        boost::property_tree::ptree pt) :
    RemoteControllable("mux"),
    m_pt(pt),
    ensemble(std::make_shared<dabEnsemble>()),
    m_clock_tai(split_pipe_separated_string(pt.get("general.tai_clock_bulletins", ""))),
    fig_carousel(ensemble)
{
    RC_ADD_PARAMETER(frames, "Show number of frames generated [read-only]");
    RC_ADD_PARAMETER(tist_offset, "Timestamp offset in integral number of seconds");

    rcs.enrol(&m_clock_tai);
}

DabMultiplexer::~DabMultiplexer()
{
    rcs.remove_controllable(&m_clock_tai);
}

void DabMultiplexer::set_edi_config(const edi::configuration_t& new_edi_conf)
{
    edi_conf = new_edi_conf;
    edi_sender = make_shared<edi::Sender>(edi_conf);
}


// Run a set of checks on the configuration
void DabMultiplexer::prepare(bool require_tai_clock)
{
    parse_ptree(m_pt, ensemble);

    rcs.enrol(this);
    rcs.enrol(ensemble.get());

    prepare_subchannels();
    prepare_services_components();
    prepare_data_inputs();
    if (not ensemble->validate_linkage_sets()) {
        etiLog.log(error, "Linkage set definition error");
        throw MuxInitException();
    }

    if (ensemble->subchannels.size() == 0) {
        etiLog.log(error, "can't multiplex no subchannel!");
        throw MuxInitException();
    }

    const auto last_subchannel = *(ensemble->subchannels.end() - 1);

    if (last_subchannel->startAddress + last_subchannel->getSizeCu() > 864) {
        etiLog.log(error, "Total size in CU exceeds 864");
        printSubchannels(ensemble->subchannels);
        throw MuxInitException();
    }

    /* At startup, derive edi_time, TIST and CIF count such that there is
     * a consistency across mux restarts. Ensure edi_time and TIST represent
     * current time.
     *
     * FCT and DLFC are directly derived from m_currentFrame.
     * Every 6s, FCT overflows. DLFC overflows at 5000 every 120s.
     *
     * Keep a granularity of 24ms, which corresponds to the duration of an ETI
     * frame, to get nicer timestamps.
     */
    using Sec = chrono::seconds;
    const auto now = chrono::system_clock::now();
    const time_t t_now = chrono::system_clock::to_time_t(chrono::time_point_cast<Sec>(now));

    m_edi_time = t_now - (t_now % 6);
    m_currentFrame = 0;
    time_t edi_time_at_cif0 = t_now - (t_now % 120);
    while (edi_time_at_cif0 < m_edi_time) {
        edi_time_at_cif0 += 6;
        m_currentFrame += 250;
    }

    if (edi_time_at_cif0 != m_edi_time) {
        throw std::logic_error("Invalid startup offset calculation for CIF!");
    }

    const auto offset = now - chrono::time_point_cast<Sec>(now);
    if (offset >= chrono::seconds(1)) {
        throw std::logic_error("Invalid startup offset calculation for TIST! " +
                to_string(chrono::duration_cast<chrono::milliseconds>(offset).count()) +
                " ms");
    }

    int64_t offset_ms = chrono::duration_cast<chrono::milliseconds>(offset).count();
    offset_ms += 1000 * (t_now - m_edi_time);

    m_timestamp = 0;
    while (offset_ms >= 24) {
        increment_timestamp();
        m_currentFrame++;
        offset_ms -= 24;
    }

    mnsc_increment_time = false;
    mnsc_time = m_edi_time;

    etiLog.log(info, "Startup CIF Count %i with timestamp: %d + %f",
            m_currentFrame, m_edi_time,
            (m_timestamp & 0xFFFFFF) / 16384000.0);

    // Try to load offset once

    bool tist_enabled = m_pt.get("general.tist", false);
    m_tist_offset = m_pt.get<int>("general.tist_offset", 0);

    m_tai_clock_required = (tist_enabled and edi_conf.enabled()) or require_tai_clock;

    if (m_tai_clock_required) {
        try {
            m_clock_tai.get_offset();
        }
        catch (const std::runtime_error& e) {
            etiLog.level(error) <<
                "Could not initialise TAI clock properly. "
                "TAI clock is required when TIST is enabled with an EDI output, "
                "or when a ZMQ output with metadata is used. "
                "Error: " << e.what();
            throw;
        }
    }

    if (ensemble->reconfig_counter == dabEnsemble::RECONFIG_COUNTER_HASH) {
        vector<uint32_t> data_to_hash;
        data_to_hash.push_back(ensemble->id);
        data_to_hash.push_back(ensemble->ecc);

        for (const auto& srv : ensemble->services) {
            data_to_hash.push_back(srv->id);
            data_to_hash.push_back(srv->ecc);
        }

        for (const auto& sc : ensemble->components) {
            data_to_hash.push_back(sc->serviceId);
            data_to_hash.push_back(sc->subchId);
            data_to_hash.push_back(sc->type);
            data_to_hash.push_back(sc->SCIdS);
        }


        for (const auto& sub : ensemble->subchannels) {
            data_to_hash.push_back(sub->id);
            data_to_hash.push_back(sub->startAddress);
            data_to_hash.push_back(sub->bitrate);

            uint32_t t = 0;
            switch (sub->type) {
                case subchannel_type_t::DABAudio : t = 1; break;
                case subchannel_type_t::DABPlusAudio: t = 2; break;
                case subchannel_type_t::DataDmb: t = 3; break;
                case subchannel_type_t::Packet: t= 4; break;
            }
            data_to_hash.push_back(t);
            data_to_hash.push_back(sub->protection.to_tpl());
        }

        uint16_t crc_tmp = 0xFFFF;
        crc_tmp = crc16(crc_tmp,
                reinterpret_cast<uint16_t*>(data_to_hash.data()),
                data_to_hash.size() * sizeof(data_to_hash.data()) / sizeof(uint16_t));

        ensemble->reconfig_counter = crc_tmp % 1024;
        etiLog.level(info) << "Calculated FIG 0/7 Count = " << ensemble->reconfig_counter;
    }
}


// Check and adjust subchannels
void DabMultiplexer::prepare_subchannels()
{
    set<unsigned char> ids;

    for (auto subchannel : ensemble->subchannels) {
        if (ids.find(subchannel->id) != ids.end()) {
            etiLog.log(error,
                    "Subchannel %u is set more than once!",
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

    vec_sp_component::iterator component;
    vec_sp_subchannel::iterator subchannel;

    for (auto service : ensemble->services) {
        if (ids.find(service->id) != ids.end()) {
            etiLog.log(error,
                    "Service id 0x%x (%u) is set more than once!",
                    service->id, service->id);
            throw MuxInitException();
        }

        // Get first component of this service
        component = getComponent(ensemble->components, service->id);
        if (component == ensemble->components.end()) {
            etiLog.log(error,
                    "Service id 0x%x (%u) includes no component!",
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
                        "links to the invalid subchannel %u",
                        (*component)->serviceId, (*component)->subchId);
                throw MuxInitException();
            }

            protection = &(*subchannel)->protection;
            switch ((*subchannel)->type) {
                case subchannel_type_t::DABPlusAudio:
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
                case subchannel_type_t::DABAudio:
                    {
                        if (protection->form == EEP) {
                            /* ASCTy change to 0x0, because DAB mp2 is using
                             */
                            (*component)->type = 0x0;
                        }
                    }
                    break;
                case subchannel_type_t::DataDmb:
                case subchannel_type_t::Packet:
                    break;
                default:
                    etiLog.log(error,
                            "Error, unknown subchannel type");
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
                    "of service %i",
                    component->subchId, component->serviceId);
            throw MuxInitException();
        }

        if ((*subchannel)->type == subchannel_type_t::Packet) {
            component->packet.id = cur_packetid++;
        }

        rcs.enrol(component.get());
    }
}

void DabMultiplexer::prepare_data_inputs()
{
    dabProtection* protection = nullptr;

    // Prepare and check the data inputs
    for (auto subchannel = ensemble->subchannels.begin();
            subchannel != ensemble->subchannels.end();
            ++subchannel) {
        protection = &(*subchannel)->protection;
        if (subchannel == ensemble->subchannels.begin()) {
            (*subchannel)->startAddress = 0;
        } else {
            (*subchannel)->startAddress = (*(subchannel - 1))->startAddress +
                (*(subchannel - 1))->getSizeCu();
        }

        (*subchannel)->input->open((*subchannel)->inputUri);

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

void DabMultiplexer::increment_timestamp()
{
    m_timestamp += 24 << 14; // Shift 24ms by 14 to Timestamp level 2
    if (m_timestamp > 0xf9FFff) {
        m_timestamp -= 0xfa0000; // Substract 16384000, corresponding to one second
        m_edi_time += 1;

        // Also update MNSC time for next time FP==0
        mnsc_increment_time = true;
    }
}

/*  Each call creates one ETI frame */
void DabMultiplexer::mux_frame(std::vector<std::shared_ptr<DabOutput> >& outputs)
{
    unsigned char etiFrame[6144];
    unsigned short index = 0;

    // FIC Length, DAB Mode I, II, IV -> FICL = 24, DAB Mode III -> FICL = 32
    unsigned FICL =
        (ensemble->transmission_mode == TransmissionMode_e::TM_III ? 32 : 24);

    // For EDI, save ETI(LI) Management data into a TAG Item DETI
    edi::TagDETI edi_tagDETI;
    edi::TagStarPTR edi_tagStarPtr("DETI");
    vector<edi::TagESTn> edi_est_tags;

    // The above Tag Items will be assembled into a TAG Packet
    edi::TagPacket edi_tagpacket(edi_conf.tagpacket_alignment);

    const bool tist_enabled = m_pt.get("general.tist", false);

    int tai_utc_offset = 0;
    if (tist_enabled and m_tai_clock_required) {
        try {
            tai_utc_offset = m_clock_tai.get_offset();
        }
        catch (const std::runtime_error& e) {
            etiLog.level(error) << "Could not get UTC-TAI offset for EDI timestamp";
        }
    }
    update_dab_time();

    const auto edi_time = m_edi_time + m_tist_offset;

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
    if ((m_currentFrame & 1) == 0) {
        etiSync->FSYNC = ETI_FSYNC1;
    }
    else {
        etiSync->FSYNC = ETI_FSYNC1 ^ 0xffffff;
    }

    /**********************************************************************
     ***********   Section LIDATA of ETI(NI, G703)   **********************
     **********************************************************************/

    // See ETS 300 799 Figure 5 for a better overview of these fields.

    //****** Section FC ***************************************************/
    // 4 octets, starts at offset 4
    eti_FC *fc = (eti_FC *) &etiFrame[4];

    //****** FCT ******//
    // Incremente for each frame, overflows at 249
    fc->FCT = m_currentFrame % 250;
    edi_tagDETI.dlfc = m_currentFrame % 5000;

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
    fc->FP = edi_tagDETI.fp = m_currentFrame & 0x7;

    //****** MID ******//
    //Mode Identity, 2 bits, 01 ModeI, 10 modeII, 11 ModeIII, 00 ModeIV
    switch (ensemble->transmission_mode) {
        case TransmissionMode_e::TM_I:
            fc->MID = edi_tagDETI.mid = 1;
            break;
        case TransmissionMode_e::TM_II:
            fc->MID = edi_tagDETI.mid = 2;
            break;
        case TransmissionMode_e::TM_III:
            fc->MID = edi_tagDETI.mid = 3;
            break;
        case TransmissionMode_e::TM_IV:
            fc->MID = edi_tagDETI.mid = 0;
            break;
    }

    //****** FL ******//
    /* Frame Length, 11 bits, nb of words(4 bytes) in STC, EOH and MST
     * if NST=0, FL=1+FICL words, FICL=24 or 32 depending on the mode.
     * The FL is given in words (4 octets), see ETS 300 799 5.3.6 for details
     */
    unsigned short FLtmp = 1 + FICL + (fc->NST);

    for (auto subchannel = ensemble->subchannels.begin();
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
    for (auto subchannel : ensemble->subchannels) {
        eti_STC *sstc = (eti_STC *) & etiFrame[index];

        sstc->SCID = subchannel->id;
        sstc->startAddress_high = subchannel->startAddress / 256;
        sstc->startAddress_low = subchannel->startAddress % 256;
        sstc->TPL = subchannel->protection.to_tpl();

        // Sub-channel Stream Length, multiple of 64 bits
        sstc->STL_high = subchannel->getSizeDWord() / 256;
        sstc->STL_low = subchannel->getSizeDWord() % 256;

        edi::TagESTn tag_ESTn;
        tag_ESTn.id = edi_stream_id++;
        tag_ESTn.scid = subchannel->id;
        tag_ESTn.sad = subchannel->startAddress;
        tag_ESTn.tpl = sstc->TPL;
        tag_ESTn.rfa = 0; // two bits
        tag_ESTn.mst_length = subchannel->getSizeByte() / 8;
        tag_ESTn.mst_data = nullptr;
        assert(subchannel->getSizeByte() % 8 == 0);

        edi_est_tags.push_back(std::move(tag_ESTn));
        index += 4;
    }

    /******* Section EOH **************************************************/
    // End of Header 4 octets
    eti_EOH *eoh = (eti_EOH *) & etiFrame[index];

    //MNSC Multiplex Network Signalling Channel, 2 octets

    eoh->MNSC = 0;

    struct tm time_tm;
    gmtime_r(&mnsc_time, &time_tm);

    switch (fc->FP & 0x3) {
        case 0:
            {
                eti_MNSC_TIME_0 *mnsc = (eti_MNSC_TIME_0 *) &eoh->MNSC;
                // Set fields according to ETS 300 799 -- 5.5.1 and A.2.2
                mnsc->type = 0;
                mnsc->identifier = 0;
                mnsc->rfa = 0;
            }

            if (mnsc_increment_time)
            {
                mnsc_increment_time = false;
                mnsc_time += 1;
            }
            break;
        case 1:
            {
                eti_MNSC_TIME_1 *mnsc = (eti_MNSC_TIME_1 *) &eoh->MNSC;
                mnsc->setFromTime(&time_tm);
                mnsc->accuracy = 1;
                mnsc->sync_to_frame = 1;
            }
            break;
        case 2:
            {
                eti_MNSC_TIME_2 *mnsc = (eti_MNSC_TIME_2 *) &eoh->MNSC;
                mnsc->setFromTime(&time_tm);
            }
            break;
        case 3:
            {
                eti_MNSC_TIME_3 *mnsc = (eti_MNSC_TIME_3 *) &eoh->MNSC;
                mnsc->setFromTime(&time_tm);
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
    const bool fib3_present = (ensemble->transmission_mode == TransmissionMode_e::TM_III);
    index += fig_carousel.write_fibs(&etiFrame[index], m_currentFrame, fib3_present);

    /**********************************************************************
     ******  Input Data Reading *******************************************
     **********************************************************************/

    for (size_t i = 0; i < ensemble->subchannels.size(); i++) {
        auto& subchannel = ensemble->subchannels[i];

        int sizeSubchannel = subchannel->getSizeByte();
        // no need to check enableTist because we always increment the timestamp
        int result = subchannel->readFrame(&etiFrame[index],
                        sizeSubchannel, edi_time, tai_utc_offset, m_timestamp);

        if (result < 0) {
            etiLog.log(info,
                    "Subchannel %d read failed at ETI frame number: %d",
                    subchannel->id, m_currentFrame);
        }

        // save pointer to Audio or Data Stream into correct TagESTn for EDI
        edi_est_tags[i].mst_data = &etiFrame[index];

        index += sizeSubchannel;
    }


    index = (3 + fc->NST + FICL) * 4;
    for (auto subchannel : ensemble->subchannels) {
        index += subchannel->getSizeByte();
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
        tist->TIST = htonl(m_timestamp) | 0xff;
        edi_tagDETI.tsta = m_timestamp & 0xffffff;
    }
    else {
        tist->TIST = htonl(0xffffff) | 0xff;
        edi_tagDETI.tsta = 0xffffff;
    }

    if (tist_enabled and m_tai_clock_required) {
        edi_tagDETI.set_edi_time(edi_time, tai_utc_offset);
        edi_tagDETI.atstf = true;

        for (auto output : outputs) {
            shared_ptr<OutputMetadata> md_utco =
                make_shared<OutputMetadataUTCO>(edi_tagDETI.utco);
            output->setMetadata(md_utco);

            shared_ptr<OutputMetadata> md_edi_time =
                make_shared<OutputMetadataEDITime>(edi_tagDETI.seconds);
            output->setMetadata(md_edi_time);

            shared_ptr<OutputMetadata> md_dlfc =
                make_shared<OutputMetadataDLFC>(m_currentFrame % 5000);
            output->setMetadata(md_dlfc);
        }
    }

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

    increment_timestamp();

    /**********************************************************************
     ***********   Section FRPD   *****************************************
     **********************************************************************/

    int frame_size = (FLtmp + 1 + 1 + 1 + 1) * 4;

    for (auto output : outputs) {
        auto out_zmq = std::dynamic_pointer_cast<DabOutputZMQ>(output);
        if (out_zmq) {
            // The separator allows the receiver to associate the right
            // metadata with the right ETI frame, since the output gathers
            // four ETI frames into one message
            shared_ptr<OutputMetadata> md_sep =
                make_shared<OutputMetadataSeparation>();
            out_zmq->setMetadata(md_sep);
        }
    }

    // Give the data to the outputs
    for (auto output : outputs) {
        if (output->Write(etiFrame, frame_size) == -1) {
            etiLog.level(error) <<
                "Can't write to output " <<
                output->get_info();
        }
    }

    /**********************************************************************
     ***********   Finalise and send EDI   ********************************
     **********************************************************************/
    if (edi_sender and edi_conf.enabled()) {
        // put tags *ptr, DETI and all subchannels into one TagPacket
        edi_tagpacket.tag_items.push_back(&edi_tagStarPtr);
        edi_tagpacket.tag_items.push_back(&edi_tagDETI);

        for (auto& tag : edi_est_tags) {
            edi_tagpacket.tag_items.push_back(&tag);
        }

        edi_sender->write(edi_tagpacket);
    }

#if _DEBUG
    /**********************************************************************
     ***********   Output a small message *********************************
     **********************************************************************/
    if (m_currentFrame % 100 == 0) {
        if (enableTist) {
            etiLog.log(info, "ETI frame number %i Timestamp: %d + %f",
                    m_currentFrame, edi_time,
                    (m_timestamp & 0xFFFFFF) / 16384000.0);
        }
        else {
            etiLog.log(info, "ETI frame number %i Time: %d, no TIST",
                    m_currentFrame, edi_time);
        }
    }
#endif

    m_currentFrame++;
}

void DabMultiplexer::print_info()
{
    // Print settings before starting
    printEnsemble(ensemble);
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
    else if (parameter == "tist_offset") {
        m_tist_offset = std::stoi(value);
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
        ss << m_currentFrame;
    }
    else if (parameter == "tist_offset") {
        ss << m_tist_offset;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

const json::map_t DabMultiplexer::get_all_values() const
{
    json::map_t map;
    map["frames"].v = m_currentFrame;
    map["tist_offset"].v = m_tist_offset;
    return map;
}

