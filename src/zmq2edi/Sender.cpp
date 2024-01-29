/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2024
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

#include "Sender.h"
#include "Log.h"
#include <cmath>
#include <numeric>
#include <map>
#include <algorithm>
#include <limits>

using namespace std;

Sender::Sender() :
    zmq_ctx(2)
{
}

Sender::~Sender()
{
    if (running.load()) {
        running.store(false);

        // Unblock thread
        frame_t emptyframe;
        frames.push(std::move(emptyframe));

        process_thread.join();
    }
}

void Sender::start(const edi::configuration_t& conf,
        const zmq_send_config_t& zmq_conf,
        int delay_ms, bool drop_late_packets)
{
    edi_conf = conf;
    tist_delay_ms = delay_ms;
    drop_late = drop_late_packets;

    edi_sender = make_shared<edi::Sender>(edi_conf);

    for (const auto& url : zmq_conf.urls) {
        zmq::socket_t zmq_sock(zmq_ctx, ZMQ_PUB);
        zmq_sock.bind(url.c_str());
        zmq_sockets.emplace_back(std::move(zmq_sock));
    }

    running.store(true);
    process_thread = thread(&Sender::process, this);
}

void Sender::push_frame(frame_t&& frame)
{
    frames.push(std::move(frame));
}

void Sender::print_configuration()
{
    if (edi_conf.enabled()) {
        edi_conf.print();
    }
    else {
        etiLog.level(info) << "EDI disabled";
    }
}

void Sender::send_eti_frame(frame_t& frame)
{
    uint8_t *p = frame.data.data();

    edi::TagDETI edi_tagDETI;
    edi::TagStarPTR edi_tagStarPtr("DETI");
    map<int, edi::TagESTn> edi_subchannelToTag;
    // The above Tag Items will be assembled into a TAG Packet
    edi::TagPacket edi_tagpacket(edi_conf.tagpacket_alignment);

    // SYNC
    edi_tagDETI.stat = p[0];

    // LIDATA FCT
    edi_tagDETI.dlfc = frame.metadata.dlfc;

    const int fct = p[4];
    if (frame.metadata.dlfc % 250 != fct) {
        etiLog.level(warn) << "Frame FCT=" << fct <<
            " does not correspond to DLFC=" << frame.metadata.dlfc;
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

    uint16_t mnsc = 0;
    std::memcpy(&mnsc, p + 8 + 4*nst, sizeof(uint16_t));
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
            frame.metadata.edi_time + posix_timestamp_1_jan_2000 - frame.metadata.utc_offset) + pps_offset;

    const auto t_release = t_frame + milliseconds(tist_delay_ms);
    const auto t_now = system_clock::now();

    const bool late = t_release < t_now;

    buffering_stat_t stat;
    stat.late = late;

    if (not late) {
        const auto wait_time = t_release - t_now;
        std::this_thread::sleep_for(wait_time);
    }

    stat.buffering_time_us = duration_cast<microseconds>(steady_clock::now() - frame.received_at).count();
    buffering_stats.push_back(std::move(stat));

    if (late and drop_late) {
        return;
    }

    edi_tagDETI.tsta = tist;
    edi_tagDETI.atstf = 1;
    edi_tagDETI.utco = frame.metadata.utc_offset;
    edi_tagDETI.seconds = frame.metadata.edi_time;

    if (edi_sender and edi_conf.enabled()) {
        // put tags *ptr, DETI and all subchannels into one TagPacket
        edi_tagpacket.tag_items.push_back(&edi_tagStarPtr);
        edi_tagpacket.tag_items.push_back(&edi_tagDETI);

        for (auto& tag : edi_subchannelToTag) {
            edi_tagpacket.tag_items.push_back(&tag.second);
        }

        edi_sender->write(edi_tagpacket);
    }

    if (not frame.original_zmq_message.empty()) {
        for (auto& sock : zmq_sockets) {
            const auto send_result = sock.send(frame.original_zmq_message, zmq::send_flags::dontwait);
            if (not send_result.has_value()) {
                num_zmq_send_errors++;
            }
        }
    }
}

void Sender::process()
{
    while (running.load()) {
        frame_t frame;
        frames.wait_and_pop(frame);

        if (not running.load() or frame.data.empty()) {
            break;
        }

        if (frame.data.size() == 6144) {
            send_eti_frame(frame);
        }
        else {
            etiLog.level(warn) << "Ignoring short ETI frame, "
                "DFLC=" << frame.metadata.dlfc << ", len=" <<
                frame.data.size();
        }

        if (buffering_stats.size() == 250) { // every six seconds
            const double n = buffering_stats.size();

            size_t num_late = std::count_if(buffering_stats.begin(), buffering_stats.end(),
                    [](const buffering_stat_t& s){ return s.late; });

            double sum = 0.0;
            double min = std::numeric_limits<double>::max();
            double max = -std::numeric_limits<double>::max();
            for (const auto& s : buffering_stats) {
                // convert to milliseconds
                const double t = s.buffering_time_us / 1000.0;
                sum += t;

                if (t < min) {
                    min = t;
                }

                if (t > max) {
                    max = t;
                }
            }
            double mean = sum / n;

            double sq_sum = 0;
            for (const auto& s : buffering_stats) {
                const double t = s.buffering_time_us / 1000.0;
                sq_sum += (t-mean) * (t-mean);
            }
            double stdev = sqrt(sq_sum / n);

            /* Debug code
            stringstream ss;
            ss << "times:";
            for (const auto t : buffering_stats) {
                ss << " " << lrint(t.buffering_time_us / 1000.0);
            }
            etiLog.level(debug) << ss.str();
            // */

            etiLog.level(info) << "Buffering time statistics [milliseconds]:"
                " min: " << min <<
                " max: " << max <<
                " mean: " << mean <<
                " stdev: " << stdev <<
                " late: " <<
                num_late << " of " << buffering_stats.size() << " (" <<
                num_late * 100.0 / n << "%) " <<
                "Num ZMQ send errors: " << num_zmq_send_errors;

            buffering_stats.clear();
        }
    }
}
