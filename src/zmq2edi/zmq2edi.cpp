/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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

#include "dabOutput/dabOutput.h"
#include "dabOutput/edi/TagItems.h"
#include "dabOutput/edi/TagPacket.h"
#include "dabOutput/edi/AFPacket.h"
#include "dabOutput/edi/PFT.h"
#include "dabOutput/edi/Interleaver.h"
#include "Log.h"
#include "zmq.hpp"
#include "math.h"
#include <getopt.h>
#include <string.h>
#include <iostream>
#include <iterator>
#include <vector>

static edi_configuration_t edi_conf;

static std::ofstream edi_debug_file;

// The TagPacket will then be placed into an AFPacket
static edi::AFPacketiser edi_afPacketiser;

// The AF Packet will be protected with reed-solomon and split in fragments
static edi::PFT edi_pft;

// To mitigate for burst packet loss, PFT fragments can be sent out-of-order
static edi::Interleaver edi_interleaver;

// This metadata gets transmitted in the zmq stream
struct metadata_t {
    uint32_t edi_time;
    int16_t utc_offset;
    uint16_t dlfc;
};

void usage(void)
{
    using namespace std;

    cerr << "Usage:" << endl;
    cerr << "odr-zmq2edi [options] <source>" << endl << endl;

    cerr << "Where the options are:" << endl;
    cerr << " <source> is a ZMQ URL that points to a ODR-DabMux ZMQ output." << endl;
    cerr << " -d <destination ip> sets the destination ip." << endl;
    cerr << " -p <destination port> sets the destination port." << endl;
    cerr << " -s <source port> sets the source port." << endl;
    cerr << " -S <source ip> select the source IP in case we want to use multicast." << endl;
    cerr << " -t <ttl> set the packet's TTL." << endl;
    cerr << " -P Disable PFT and send AFPackets." << endl;
    cerr << " -f <fec> sets the FEC." << endl;
    cerr << " -i <interleave> enables the interleaved with this latency." << endl;
    cerr << " -D dumps the EDI to edi.debug file." << endl;
    cerr << " -v Enables verbose mode." << endl;
    cerr << " -a <tagpacket alignement> sets the alignment of the TAG Packet (default 8)." << endl;
}

static void edi_setup(void) {
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
                    throw std::runtime_error("EDI socket set source failed!");
                }
                err = edi_output->setMulticastTTL(edi_destination.ttl);
                if (err) {
                    throw std::runtime_error("EDI socket set TTL failed!");
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
}

static void print_edi_conf(void)
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

static metadata_t get_md_one_frame(uint8_t *buf, size_t size, size_t *consumed_bytes)
{
    size_t remaining = size;
    if (remaining < 3) {
        etiLog.level(warn) << "Insufficient data to parse metadata";
        throw std::runtime_error("Insufficient data");
    }

    metadata_t md;
    bool utc_offset_received = false;
    bool edi_time_received = false;
    bool dlfc_received = false;

    while (remaining) {
        uint8_t id = buf[0];
        uint16_t len = (((uint16_t)buf[1]) << 8) + buf[2];

        if (id == static_cast<uint8_t>(output_metadata_id_e::separation_marker)) {
            if (len != 0) {
                etiLog.level(warn) << "Invalid length " << len << " for metadata: separation_marker";
            }

            if (not utc_offset_received or not edi_time_received or not dlfc_received) {
                throw std::runtime_error("Incomplete metadata received");
            }

            remaining -= 3;
            *consumed_bytes = size - remaining;
            return md;
        }
        else if (id == static_cast<uint8_t>(output_metadata_id_e::utc_offset)) {
            if (len != 2) {
                etiLog.level(warn) << "Invalid length " << len << " for metadata: utc_offset";
            }
            if (remaining < 2) {
                throw std::runtime_error("Insufficient data for utc_offset");
            }
            uint16_t utco;
            std::memcpy(&utco, buf + 3, sizeof(utco));
            md.utc_offset = ntohs(utco);
            utc_offset_received = true;
            remaining -= 5;
            buf += 5;
        }
        else if (id == static_cast<uint8_t>(output_metadata_id_e::edi_time)) {
            if (len != 4) {
                etiLog.level(warn) << "Invalid length " << len << " for metadata: edi_time";
            }
            if (remaining < 4) {
                throw std::runtime_error("Insufficient data for edi_time");
            }
            uint32_t edi_time;
            std::memcpy(&edi_time, buf + 3, sizeof(edi_time));
            md.edi_time = ntohl(edi_time);
            edi_time_received = true;
            remaining -= 7;
            buf += 7;
        }
        else if (id == static_cast<uint8_t>(output_metadata_id_e::dlfc)) {
            if (len != 2) {
                etiLog.level(warn) << "Invalid length " << len << " for metadata: dlfc";
            }
            if (remaining < 2) {
                throw std::runtime_error("Insufficient data for dlfc");
            }
            uint16_t dlfc;
            std::memcpy(&dlfc, buf + 3, sizeof(dlfc));
            md.dlfc = ntohs(dlfc);
            dlfc_received = true;
            remaining -= 5;
            buf += 5;
        }
    }

    throw std::runtime_error("Insufficient data");
}

static void send_eti_frame(uint8_t* p, metadata_t metadata)
{
    edi::TagDETI edi_tagDETI;
    edi::TagStarPTR edi_tagStarPtr;
    std::map<int, edi::TagESTn> edi_subchannelToTag;
    // The above Tag Items will be assembled into a TAG Packet
    edi::TagPacket edi_tagpacket(edi_conf.tagpacket_alignment);

    // SYNC
    edi_tagDETI.stat = p[0];

    // LIDATA FCT
    edi_tagDETI.dlfc = metadata.dlfc;

    const int fct = p[4];
    if (metadata.dlfc % 250 != fct) {
        etiLog.level(warn) << "Frame FCT=" << fct << " does not correspond to DLFC=" << metadata.dlfc;
    }


    bool ficf = (p[5] & 0x80) >> 7;

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

    std::vector<uint32_t> sad(nst);
    std::vector<uint32_t> stl(nst);
    // Loop over STC subchannels:
    for (int i=0; i < nst; i++) {
        uint32_t scid = (p[8 + 4*i] & 0xFC) >> 2;
        sad[i] = (p[8+4*i] & 0x03) * 256 + p[9+4*i];
        uint32_t tpl = (p[10+4*i] & 0xFC) >> 2;
        stl[i] = (p[10+4*i] & 0x03) * 256 + \
                 p[11+4*i];

        edi::TagESTn tag_ESTn;
        tag_ESTn.id = i;
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
            std::vector<edi::PFTFragment> edi_fragments = edi_pft.Assemble(edi_afpacket);

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

int start(int argc, char **argv)
{
    edi_destination_t edi_destination;

    edi_conf.enable_pft = true;

    if (argc == 0) {
        usage();
        return 1;
    }

    char ch = 0;
    while (ch != -1) {
        ch = getopt(argc, argv, "d:p:s:S:t:Pf:i:Dva:");
        switch (ch) {
            case -1:
                break;
            case 'd':
                edi_destination.dest_addr = optarg;
                break;
            case 'p':
                edi_conf.dest_port = std::stoi(optarg);
                break;
            case 's':
                edi_destination.source_port = std::stoi(optarg);
                break;
            case 'S':
                edi_destination.source_addr = optarg;
                break;
            case 't':
                edi_destination.ttl = std::stoi(optarg);
                break;
            case 'P':
                edi_conf.enable_pft = false;
                break;
            case 'f':
                edi_conf.fec = std::stoi(optarg);
                break;
            case 'i':
                {
                double interleave_ms = std::stod(optarg);
                if (interleave_ms != 0.0) {
                    if (interleave_ms < 0) {
                        throw std::runtime_error("EDI output: negative interleave value is invalid.");
                    }

                    auto latency_rounded = lround(interleave_ms / 24.0);
                    if (latency_rounded * 24 > 30000) {
                        throw std::runtime_error("EDI output: interleaving set for more than 30 seconds!");
                    }

                    edi_conf.latency_frames = latency_rounded;
                }
                }
                break;
            case 'D':
                edi_conf.dump = true;
                break;
            case 'v':
                edi_conf.verbose = true;
                break;
            case 'a':
                edi_conf.tagpacket_alignment = std::stoi(optarg);
                break;
            case 'h':
                usage();
                return 1;
            default:
                etiLog.log(error, "Option '%c' not understood", ch);
                usage();
                return 1;
        }
    }

    if (optind >= argc) {
        etiLog.level(error) << "source option is missing";
        return 1;
    }

    edi_conf.destinations.push_back(edi_destination);

    print_edi_conf();
    edi_setup();

    const char* source_url = argv[optind];

    etiLog.level(info) << "Opening ZMQ input: " << source_url;
    zmq::context_t zmq_ctx(1);
    zmq::socket_t zmq_sock(zmq_ctx, ZMQ_SUB);

    zmq_sock.connect(source_url);
    zmq_sock.setsockopt(ZMQ_SUBSCRIBE, NULL, 0); // subscribe to all messages

    etiLog.level(info) << "Entering main loop";
    size_t frame_count = 0;
    size_t loop_counter = 0;
    size_t error_count = 0;
    while (error_count < 10)
    {
        zmq::message_t incoming;
        zmq_sock.recv(&incoming);

        zmq_dab_message_t* dab_msg = (zmq_dab_message_t*)incoming.data();

        if (dab_msg->version != 1) {
            etiLog.level(error) << "ZeroMQ wrong packet version " << dab_msg->version;
            error_count++;
        }

        int offset = sizeof(dab_msg->version) +
            NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg->buflen);

        std::list<std::pair<std::vector<uint8_t>, metadata_t> > all_frames;

        for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
            if (dab_msg->buflen[i] <= 0 ||
                dab_msg->buflen[i] > 6144)
            {
                etiLog.level(error) << "ZeroMQ buffer " << i << " has invalid length " <<
                    dab_msg->buflen[i];
                error_count++;
            }
            else {
                std::vector<uint8_t> buf(6144, 0x55);

                const int framesize = dab_msg->buflen[i];

                memcpy(&buf.front(),
                        ((uint8_t*)incoming.data()) + offset,
                        framesize);

                all_frames.emplace_back(
                        std::piecewise_construct,
                        std::make_tuple(std::move(buf)),
                        std::make_tuple());

                offset += framesize;
            }
        }

        for (auto &f : all_frames) {
            size_t consumed_bytes = 0;

            f.second = get_md_one_frame(
                    static_cast<uint8_t*>(incoming.data()) + offset,
                    incoming.size() - offset,
                    &consumed_bytes);

            offset += consumed_bytes;
        }

        for (auto &f : all_frames) {
            send_eti_frame(f.first.data(), f.second);
            frame_count++;
        }

        loop_counter++;
        if (loop_counter > 250) {
            etiLog.level(info) << "Transmitted " << frame_count << " ETI frames";
            loop_counter = 0;
        }
    }

    return error_count > 0 ? 2 : 0;
}

int main(int argc, char **argv)
{
    etiLog.level(info) << "ZMQ2EDI converter from " <<
        PACKAGE_NAME << " " <<
#if defined(GITVERSION)
        GITVERSION <<
#else
        PACKAGE_VERSION <<
#endif
        " starting up";

    try {
        return start(argc, argv);
    }
    catch (std::runtime_error &e) {
        etiLog.level(error) << "Error: " << e.what();
    }

    return 1;
}
