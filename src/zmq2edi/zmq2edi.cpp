/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019
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

#include "Log.h"
#include "zmq.hpp"
#include <getopt.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

#include "EDISender.h"
#include "dabOutput/dabOutput.h"

constexpr size_t MAX_ERROR_COUNT = 10;
constexpr size_t MAX_NUM_RESETS = 180;
constexpr long ZMQ_TIMEOUT_MS = 1000;

static edi::configuration_t edi_conf;

static EDISender edisender;

static void usage()
{
    using namespace std;

    cerr << "Usage:" << endl;
    cerr << "odr-zmq2edi [options] <source>" << endl << endl;

    cerr << "Options:" << endl;
    cerr << "The following options can be given only once:" << endl;
    cerr << " <source> is a ZMQ URL that points to a ODR-DabMux ZMQ output." << endl;
    cerr << " -w <delay> Keep every ETI frame until TIST is <delay> milliseconds after current system time." << endl;
    cerr << " -x Drop frames where for which the wait time would be negative, i.e. frames that arrived too late." << endl;
    cerr << " -p <destination port> sets the destination port." << endl;
    cerr << " -P Disable PFT and send AFPackets." << endl;
    cerr << " -f <fec> sets the FEC." << endl;
    cerr << " -i <interleave> enables the interleaved with this latency." << endl;
    cerr << " -D dumps the EDI to edi.debug file." << endl;
    cerr << " -v Enables verbose mode." << endl;
    cerr << " -a <tagpacket alignement> sets the alignment of the TAG Packet (default 8)." << endl << endl;

    cerr << "The following options can be given several times, when more than UDP destination is desired:" << endl;
    cerr << " -d <destination ip> sets the destination ip." << endl;
    cerr << " -s <source port> sets the source port." << endl;
    cerr << " -S <source ip> select the source IP in case we want to use multicast." << endl;
    cerr << " -t <ttl> set the packet's TTL." << endl << endl;

    cerr << "The input socket will be reset if no data is received for " <<
        (int)(MAX_ERROR_COUNT * ZMQ_TIMEOUT_MS / 1000.0) << " seconds." << endl;
    cerr << "After " << MAX_NUM_RESETS << " consecutive resets, the process will quit." << endl;
    cerr << "It is best practice to run this tool under a process supervisor that will restart it automatically." << endl;
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

/* There is some state inside the parsing of destination arguments,
 * because several destinations can be given.  */

static std::shared_ptr<edi::udp_destination_t> edi_destination;
static bool source_port_set = false;
static bool source_addr_set = false;
static bool ttl_set = false;
static bool dest_addr_set = false;

static void add_edi_destination(void)
{
    if (not dest_addr_set) {
        throw std::runtime_error("Destination address not specified for destination number " +
                std::to_string(edi_conf.destinations.size() + 1));
    }

    edi_conf.destinations.push_back(move(edi_destination));
    edi_destination = std::make_shared<edi::udp_destination_t>();

    source_port_set = false;
    source_addr_set = false;
    ttl_set = false;
    dest_addr_set = false;
}

static void parse_destination_args(char option)
{
    if (not edi_destination) {
        edi_destination = std::make_shared<edi::udp_destination_t>();
    }

    switch (option) {
        case 's':
            if (source_port_set) {
                add_edi_destination();
            }
            edi_destination->source_port = std::stoi(optarg);
            source_port_set = true;
            break;
        case 'S':
            if (source_addr_set) {
                add_edi_destination();
            }
            edi_destination->source_addr = optarg;
            source_addr_set = true;
            break;
        case 't':
            if (ttl_set) {
                add_edi_destination();
            }
            edi_destination->ttl = std::stoi(optarg);
            ttl_set = true;
            break;
        case 'd':
            if (dest_addr_set) {
                add_edi_destination();
            }
            edi_destination->dest_addr = optarg;
            dest_addr_set = true;
            break;
        default:
            throw std::logic_error("parse_destination_args invalid");
    }
}

int start(int argc, char **argv)
{
    edi_conf.enable_pft = true;

    if (argc == 0) {
        usage();
        return 1;
    }

    int delay_ms = 500;
    bool drop_late_packets = false;

    int ch = 0;
    while (ch != -1) {
        ch = getopt(argc, argv, "d:p:s:S:t:Pf:i:Dva:w:x");
        switch (ch) {
            case -1:
                break;
            case 'd':
            case 's':
            case 'S':
            case 't':
                parse_destination_args(ch);
                break;
            case 'p':
                edi_conf.dest_port = std::stoi(optarg);
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
            case 'w':
                delay_ms = std::stoi(optarg);
                break;
            case 'x':
                drop_late_packets = true;
                break;
            case 'h':
            default:
                usage();
                return 1;
        }
    }

    add_edi_destination();

    if (optind >= argc) {
        etiLog.level(error) << "source option is missing";
        return 1;
    }

    if (edi_conf.dest_port == 0) {
        etiLog.level(error) << "No EDI destination port defined";
        return 1;
    }

    if (edi_conf.destinations.empty()) {
        etiLog.level(error) << "No EDI destinations set";
        return 1;
    }

    etiLog.level(info) << "Setting up EDI Sender with delay " << delay_ms << " ms. " <<
        (drop_late_packets ? "Will" : "Will not") << " drop late packets";
    edisender.start(edi_conf, delay_ms, drop_late_packets);
    edisender.print_configuration();

    const char* source_url = argv[optind];

    zmq::context_t zmq_ctx(1);
    etiLog.level(info) << "Opening ZMQ input: " << source_url;

    size_t num_consecutive_resets = 0;
    while (num_consecutive_resets < MAX_NUM_RESETS) {
        zmq::socket_t zmq_sock(zmq_ctx, ZMQ_SUB);
        zmq_sock.connect(source_url);
        zmq_sock.setsockopt(ZMQ_SUBSCRIBE, NULL, 0); // subscribe to all messages

        size_t error_count = 0;

        while (error_count < MAX_ERROR_COUNT) {
            zmq::message_t incoming;
            zmq::pollitem_t items[1];
            items[0].socket = zmq_sock;
            items[0].events = ZMQ_POLLIN;
            const int num_events = zmq::poll(items, 1, ZMQ_TIMEOUT_MS);
            if (num_events == 0) { // timeout
                error_count++;
            }
            else {
                num_consecutive_resets = 0;

                // Event received: recv will not block
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
                    if (dab_msg->buflen[i] <= 0 or dab_msg->buflen[i] > 6144) {
                        etiLog.level(error) << "ZeroMQ buffer " << i <<
                            " has invalid length " << dab_msg->buflen[i];
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
                    edisender.push_frame(f);
                }
            }
        }

        num_consecutive_resets++;

        zmq_sock.close();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        etiLog.level(info) << "ZMQ input (" << source_url << ") timeout after " <<
            num_consecutive_resets << " consecutive resets.";
    }

    return 0;
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
