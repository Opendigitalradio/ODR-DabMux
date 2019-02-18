/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#include "EDISender.h"
#include "Log.h"
#include <cmath>
#include <numeric>
#include <map>
#include <algorithm>

using namespace std;

EDISender::~EDISender()
{
    if (running.load()) {
        running.store(false);

        // Unblock thread
        frame_t emptyframe;
        frames.push(emptyframe);

        process_thread.join();
    }
}

void EDISender::start(const edi_configuration_t& conf,
        int delay_ms, bool drop_late_packets)
{
    edi_conf = conf;
    tist_delay_ms = delay_ms;
    drop_late = drop_late_packets;

    if (edi_conf.verbose) {
        etiLog.log(info, "Setup EDI");
    }

    if (edi_conf.dump) {
        edi_debug_file.open("./edi.debug");
    }

    if (edi_conf.enabled()) {
        for (auto& edi_destination : edi_conf.destinations) {
            auto edi_output = make_shared<UdpSocket>(edi_destination.source_port);

            if (not edi_destination.source_addr.empty()) {
                int err = edi_output->setMulticastSource(edi_destination.source_addr.c_str());
                if (err) {
                    throw runtime_error("EDI socket set source failed!");
                }
                err = edi_output->setMulticastTTL(edi_destination.ttl);
                if (err) {
                    throw runtime_error("EDI socket set TTL failed!");
                }
            }

            edi_destination.socket = edi_output;
        }
    }

    if (edi_conf.verbose) {
        etiLog.log(info, "EDI set up");
    }

    // The AF Packet will be protected with reed-solomon and split in fragments
    edi::PFT pft(edi_conf);
    edi_pft = pft;

    if (edi_conf.interleaver_enabled()) {
        edi_interleaver.SetLatency(edi_conf.latency_frames);
    }

    startTime = std::chrono::steady_clock::now();
    running.store(true);
    process_thread = thread(&EDISender::process, this);
}

void EDISender::push_frame(const frame_t& frame)
{
    frames.push(frame);
}

void EDISender::print_configuration()
{
    if (edi_conf.enabled()) {
        etiLog.level(info) << "EDI";
        etiLog.level(info) << " verbose     " << edi_conf.verbose;
        for (auto& edi_dest : edi_conf.destinations) {
            etiLog.level(info) << " to " << edi_dest.dest_addr << ":" << edi_conf.dest_port;
            if (not edi_dest.source_addr.empty()) {
                etiLog.level(info) << "  source      " << edi_dest.source_addr;
                etiLog.level(info) << "  ttl         " << edi_dest.ttl;
            }
            etiLog.level(info) << "  source port " << edi_dest.source_port;
        }
        if (edi_conf.interleaver_enabled()) {
            etiLog.level(info) << " interleave     " << edi_conf.latency_frames * 24 << " ms";
        }
    }
    else {
        etiLog.level(info) << "EDI disabled";
    }
}

void EDISender::send_eti_frame(uint8_t* p, metadata_t metadata)
{
    edi::TagDETI edi_tagDETI;
    edi::TagStarPTR edi_tagStarPtr;
    map<int, edi::TagESTn> edi_subchannelToTag;
    // The above Tag Items will be assembled into a TAG Packet
    edi::TagPacket edi_tagpacket(edi_conf.tagpacket_alignment);

    // SYNC
    edi_tagDETI.stat = p[0];

    // LIDATA FCT
    edi_tagDETI.dlfc = metadata.dlfc;

    const int fct = p[4];
    if (metadata.dlfc % 250 != fct) {
        etiLog.level(warn) << "Frame FCT=" << fct <<
            " does not correspond to DLFC=" << metadata.dlfc;
    }

    bool ficf = (p[5] & 0x80) >> 7;
    edi_tagDETI.ficf = ficf;

    const int nst = p[5] & 0x7F;

    edi_tagDETI.fp = (p[6] & 0xE0) >> 5;
    const int mid = (p[6] & 0x18) >> 3;
    edi_tagDETI.mid = mid;
    //const int fl = (p[6] & 0x07) * 256 + p[7];

    int ficl = 0;
    if (ficf == 0) {
        etiLog.level(warn) << "Not FIC in data stream!";
        return;
    }
    else if (mid == 3) {
        ficl = 32;
    }
    else {
        ficl = 24;
    }

    vector<uint32_t> sad(nst);
    vector<uint32_t> stl(nst);
    // Loop over STC subchannels:
    for (int i=0; i < nst; i++) {
        // EDI stream index is 1-indexed
        const int edi_stream_id = i + 1;

        uint32_t scid = (p[8 + 4*i] & 0xFC) >> 2;
        sad[i] = (p[8+4*i] & 0x03) * 256 + p[9+4*i];
        uint32_t tpl = (p[10+4*i] & 0xFC) >> 2;
        stl[i] = (p[10+4*i] & 0x03) * 256 + \
                 p[11+4*i];

        edi::TagESTn tag_ESTn;
        tag_ESTn.id = edi_stream_id;
        tag_ESTn.scid = scid;
        tag_ESTn.sad = sad[i];
        tag_ESTn.tpl = tpl;
        tag_ESTn.rfa = 0; // two bits
        tag_ESTn.mst_length = stl[i];
        tag_ESTn.mst_data = nullptr;

        edi_subchannelToTag[i] = tag_ESTn;
    }

    const uint16_t mnsc = p[8 + 4*nst] * 256 + \
                          p[8 + 4*nst + 1];
    edi_tagDETI.mnsc = mnsc;

    /*const uint16_t crc1 = p[8 + 4*nst + 2]*256 + \
                          p[8 + 4*nst + 3]; */

    edi_tagDETI.fic_data = p + 12 + 4*nst;
    edi_tagDETI.fic_length = ficl * 4;

    // loop over MSC subchannels
    int offset = 0;
    for (int i=0; i < nst; i++) {
        edi::TagESTn& tag = edi_subchannelToTag[i];
        tag.mst_data = (p + 12 + 4*nst + ficf*ficl*4 + offset);

        offset += stl[i] * 8;
    }

    /*
    const uint16_t crc2 = p[12 + 4*nst + ficf*ficl*4 + offset] * 256 + \
                          p[12 + 4*nst + ficf*ficl*4 + offset + 1]; */

    // TIST
    const size_t tist_ix = 12 + 4*nst + ficf*ficl*4 + offset + 4;
    uint32_t tist = (uint32_t)(p[tist_ix]) << 24 |
                    (uint32_t)(p[tist_ix+1]) << 16 |
                    (uint32_t)(p[tist_ix+2]) << 8 |
                    (uint32_t)(p[tist_ix+3]);

    std::time_t posix_timestamp_1_jan_2000 = 946684800;

    // Wait until our time is tist_delay after the TIST before
    // we release that frame

    using namespace std::chrono;

    const auto pps_offset = milliseconds(std::lrint((tist & 0xFFFFFF) / 16384.0));
    const auto t_frame = system_clock::from_time_t(
            metadata.edi_time + posix_timestamp_1_jan_2000 - metadata.utc_offset) + pps_offset;

    const auto t_release = t_frame + milliseconds(tist_delay_ms);
    const auto t_now = system_clock::now();

    const auto wait_time = t_release - t_now;
    wait_times.push_back(duration_cast<microseconds>(wait_time).count());

    if (t_release > t_now) {
        std::this_thread::sleep_for(wait_time);
    }
    else if (drop_late) {
        return;
    }

    edi_tagDETI.tsta = tist;
    edi_tagDETI.atstf = 1;
    edi_tagDETI.utco = metadata.utc_offset;
    edi_tagDETI.seconds = metadata.edi_time;

    if (edi_conf.enabled()) {
        // put tags *ptr, DETI and all subchannels into one TagPacket
        edi_tagpacket.tag_items.push_back(&edi_tagStarPtr);
        edi_tagpacket.tag_items.push_back(&edi_tagDETI);

        for (auto& tag : edi_subchannelToTag) {
            edi_tagpacket.tag_items.push_back(&tag.second);
        }

        // Assemble into one AF Packet
        edi::AFPacket edi_afpacket = edi_afPacketiser.Assemble(edi_tagpacket);

        if (edi_conf.enable_pft) {
            // Apply PFT layer to AF Packet (Reed Solomon FEC and Fragmentation)
            vector<edi::PFTFragment> edi_fragments = edi_pft.Assemble(edi_afpacket);

            if (edi_conf.verbose) {
                fprintf(stderr, "EDI number of PFT fragment before interleaver %zu\n",
                        edi_fragments.size());
            }

            if (edi_conf.interleaver_enabled()) {
                edi_fragments = edi_interleaver.Interleave(edi_fragments);
            }

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
}

void EDISender::process()
{
    while (running.load()) {
        frame_t frame;
        frames.wait_and_pop(frame);

        if (not running.load() or frame.first.empty()) {
            break;
        }

        if (frame.first.size() == 6144) {
            send_eti_frame(frame.first.data(), frame.second);
        }
        else {
            etiLog.level(warn) << "Ignoring short ETI frame, "
                "DFLC=" << frame.second.dlfc << ", len=" <<
                frame.first.size();
        }

        if (wait_times.size() == 250) { // every six seconds
            const double n = wait_times.size();

            double sum = accumulate(wait_times.begin(), wait_times.end(), 0);
            size_t num_late = std::count_if(wait_times.begin(), wait_times.end(),
                    [](double v){ return v < 0; });
            double mean = sum / n;

            double sq_sum = 0;
            for (const auto t : wait_times) {
                sq_sum += (t-mean) * (t-mean);
            }
            double stdev = sqrt(sq_sum / n);
            auto min_max = minmax_element(wait_times.begin(), wait_times.end());

            /* Debug code
            stringstream ss;
            ss << "times:";
            for (const auto t : wait_times) {
                ss << " " << t;
            }
            etiLog.level(debug) << ss.str();
            */

            etiLog.level(info) << "Wait time statistics [microseconds]:"
                " min: " << *min_max.first <<
                " max: " << *min_max.second <<
                " mean: " << mean <<
                " stdev: " << stdev <<
                " late: " <<
                num_late << " of " << wait_times.size() << " (" <<
                num_late * 100.0 / n << "%)";

            wait_times.clear();
        }
    }
}
