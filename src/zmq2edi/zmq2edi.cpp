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

#include "Sender.h"
#include "dabOutput/dabOutput.h"

constexpr size_t MAX_ERROR_COUNT = 10;
constexpr long ZMQ_TIMEOUT_MS = 1000;
constexpr long DEFAULT_BACKOFF = 5000;

static edi::configuration_t edi_conf;

static Sender edisender;

static void usage()
{
    using namespace std;

    cerr << "Usage:" << endl;
    cerr << "odr-zmq2edi [options] <source>" << endl << endl;

    cerr << "ODR-ZMQ2EDI can output to both EDI and ZMQ. It buffers and releases frames according to their timestamp." << endl;

    cerr << "Options:" << endl;
    cerr << "The following options can be given only once:" << endl;
    cerr << " <source> is a ZMQ URL that points to a ODR-DabMux ZMQ output." << endl;
    cerr << " -w <delay>            Keep every ETI frame until TIST is <delay> milliseconds after current system time." << endl;
    cerr << "                       Negative delay values are also allowed." << endl;
    cerr << " -C <path to script>   Before starting, run the given script, and only start if it returns 0." << endl;
    cerr << "                       This is useful for checking that NTP is properly synchronised" << endl;
    cerr << " -x                    Drop frames where for which the wait time would be negative, i.e. frames that arrived too late." << endl;
    cerr << " -b <backoff>          Number of milliseconds to backoff after an input reset (default " << DEFAULT_BACKOFF << ")." << endl << endl;

    cerr << " ZMQ Output options:" << endl;
    cerr << " -Z <url>              Add a zmq output to URL, e.g. tcp:*:9876 to listen for connections on port 9876 " << endl << endl;

    cerr << " EDI Output options:" << endl;
    cerr << " -v                    Enables verbose mode." << endl;
    cerr << " -P                    Disable PFT and send AFPackets." << endl;
    cerr << " -f <fec>              Set the FEC." << endl;
    cerr << " -i <spread>           Configure the UDP packet spread/interleaver with given percentage: 0% send all fragments at once, 100% spread over 24ms, >100% spread and interleave. Default 95%\n";
    cerr << " -D                    Dump the EDI to edi.debug file." << endl;
    cerr << " -a <alignement>       Set the alignment of the TAG Packet (default 8)." << endl;

    cerr << "The following options can be given several times, when more than EDI/UDP destination is desired:" << endl;
    cerr << " -d <destination ip>   Set the destination ip." << endl;
    cerr << " -p <destination port> Set the destination port." << endl;
    cerr << " -s <source port>      Set the source port." << endl;
    cerr << " -S <source ip>        Select the source IP in case we want to use multicast." << endl;
    cerr << " -t <ttl>              Set the packet's TTL." << endl << endl;

    cerr << "The input socket will be reset if no data is received for " <<
        (int)(MAX_ERROR_COUNT * ZMQ_TIMEOUT_MS / 1000.0) << " seconds." << endl;
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
static bool dest_port_set = false;
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

    edi_conf.destinations.push_back(std::move(edi_destination));
    edi_destination = std::make_shared<edi::udp_destination_t>();

    dest_port_set = false;
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
        case 'p':
            if (dest_port_set) {
                add_edi_destination();
            }
            edi_destination->dest_port = std::stoi(optarg);
            dest_port_set = true;
            break;
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

class FCTDiscontinuity { };

int start(int argc, char **argv)
{
    edi_conf.enable_pft = true;

    if (argc == 0) {
        usage();
        return 1;
    }

    int delay_ms = 500;
    bool drop_late_packets = false;
    uint32_t backoff_after_reset_ms = DEFAULT_BACKOFF;
    std::string startupcheck;

    zmq_send_config_t zmq_conf;

    int ch = 0;
    while (ch != -1) {
        ch = getopt(argc, argv, "C:d:p:s:S:t:Pf:i:Dva:b:w:xhZ:");
        switch (ch) {
            case -1:
                break;
            case 'C':
                startupcheck = optarg;
                break;
            case 'd':
            case 's':
            case 'S':
            case 't':
            case 'p':
                parse_destination_args(ch);
                break;
            case 'P':
                edi_conf.enable_pft = false;
                break;
            case 'f':
                edi_conf.fec = std::stoi(optarg);
                break;
            case 'i':
                {
                    int spread_percent = std::stoi(optarg);
                    if (spread_percent < 0) {
                        throw std::runtime_error("EDI output: negative spread value is invalid.");
                    }

                    edi_conf.fragment_spreading_factor = (double)spread_percent / 100.0;
                    if (edi_conf.fragment_spreading_factor > 30000) {
                        throw std::runtime_error("EDI output: interleaving set for more than 30 seconds!");
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
            case 'b':
                backoff_after_reset_ms = std::stoi(optarg);
                break;
            case 'w':
                delay_ms = std::stoi(optarg);
                break;
            case 'x':
                drop_late_packets = true;
                break;
            case 'Z':
                zmq_conf.urls.push_back(optarg);
                break;
            case 'h':
            default:
                usage();
                return 1;
        }
    }

    if (dest_addr_set) {
        add_edi_destination();
    }

    if (optind >= argc) {
        etiLog.level(error) << "source option is missing";
        return 1;
    }

    if (edi_conf.destinations.empty() and zmq_conf.urls.empty()) {
        etiLog.level(error) << "No destinations set";
        return 1;
    }

    if (not edi_conf.destinations.empty()) {
        edisender.print_configuration();
    }

    if (not zmq_conf.urls.empty()) {
        etiLog.level(info) << "Setting up ZMQ to:";
        for (const auto& url : zmq_conf.urls) {
            etiLog.level(info) << " " << url;
        }
    }


    if (not startupcheck.empty()) {
        etiLog.level(info) << "Running startup check '" << startupcheck << "'";
        int wstatus = system(startupcheck.c_str());

        if (WIFEXITED(wstatus)) {
            if (WEXITSTATUS(wstatus) == 0) {
                etiLog.level(info) << "Startup check ok";
            }
            else {
                etiLog.level(error) << "Startup check failed, returned " << WEXITSTATUS(wstatus);
                return 1;
            }
        }
        else {
            etiLog.level(error) << "Startup check failed, child didn't terminate normally";
            return 1;
        }
    }

    etiLog.level(info) << "Setting up Sender with delay " << delay_ms << " ms. " <<
        (drop_late_packets ? "Will" : "Will not") << " drop late packets";
    edisender.start(edi_conf, zmq_conf, delay_ms, drop_late_packets);

    const char* source_url = argv[optind];

    zmq::context_t zmq_ctx(1);
    etiLog.level(info) << "Opening ZMQ input: " << source_url;

    while (true) {
        zmq::socket_t zmq_sock(zmq_ctx, ZMQ_SUB);
        zmq_sock.connect(source_url);
        zmq_sock.setsockopt(ZMQ_SUBSCRIBE, NULL, 0); // subscribe to all messages

        size_t error_count = 0;
        int previous_fct = -1;

        try {
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
                    // Event received: recv will not block
                    const auto recv_result = zmq_sock.recv(incoming, zmq::recv_flags::none);
                    if (not recv_result.has_value()) {
                        continue;
                    }

                    const auto received_at = std::chrono::steady_clock::now();

                    // Casting incoming.data() to zmq_dab_message_t* is not allowed, because
                    // it might be misaligned
                    zmq_dab_message_t dab_msg;
                    memcpy(&dab_msg, incoming.data(), ZMQ_DAB_MESSAGE_HEAD_LENGTH);

                    if (dab_msg.version != 1) {
                        etiLog.level(error) << "ZeroMQ wrong packet version " << dab_msg.version;
                        error_count++;
                    }

                    int offset = sizeof(dab_msg.version) +
                        NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg.buflen);

                    std::vector<frame_t> all_frames;
                    all_frames.reserve(NUM_FRAMES_PER_ZMQ_MESSAGE);

                    for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
                        if (dab_msg.buflen[i] <= 0 or dab_msg.buflen[i] > 6144) {
                            etiLog.level(error) << "ZeroMQ buffer " << i <<
                                " has invalid length " << dab_msg.buflen[i];
                            error_count++;
                        }
                        else {
                            frame_t frame;
                            frame.data.resize(6144, 0x55);
                            frame.received_at = received_at;

                            const int framesize = dab_msg.buflen[i];

                            memcpy(frame.data.data(),
                                    ((uint8_t*)incoming.data()) + offset,
                                    framesize);

                            const int fct = frame.data[4];

                            const int expected_fct = (previous_fct + 1) % 250;
                            if (previous_fct != -1 and expected_fct != fct) {
                                etiLog.level(error) << "ETI wrong frame counter FCT=" << fct << " expected " << expected_fct;
                                throw FCTDiscontinuity();
                            }
                            previous_fct = fct;

                            all_frames.push_back(std::move(frame));

                            offset += framesize;
                        }
                    }

                    for (auto &f : all_frames) {
                        size_t consumed_bytes = 0;

                        f.metadata = get_md_one_frame(
                                static_cast<uint8_t*>(incoming.data()) + offset,
                                incoming.size() - offset,
                                &consumed_bytes);

                        offset += consumed_bytes;
                    }

                    if (not all_frames.empty()) {
                        all_frames[0].original_zmq_message = std::move(incoming);
                    }

                    for (auto &f : all_frames) {
                        edisender.push_frame(std::move(f));
                    }
                }
            }

            etiLog.level(info) << "Backoff " << backoff_after_reset_ms <<
                "ms due to ZMQ input (" << source_url << ") timeout";
        }
        catch (const FCTDiscontinuity&) {
            etiLog.level(info) << "Backoff " << backoff_after_reset_ms << "ms FCT discontinuity";
        }

        zmq_sock.close();
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_after_reset_ms));
    }

    return 0;
}

int main(int argc, char **argv)
{
    // Version handling is done very early to ensure nothing else but the version gets printed out
    if (argc == 2 and strcmp(argv[1], "--version") == 0) {
        fprintf(stdout, "%s\n",
#if defined(GITVERSION)
                GITVERSION
#else
                PACKAGE_VERSION
#endif
               );
        return 0;
    }

    etiLog.level(info) << "ZMQ2EDI converter from " <<
        PACKAGE_NAME << " " <<
#if defined(GITVERSION)
        GITVERSION <<
#else
        PACKAGE_VERSION <<
#endif
        " starting up";

    int ret = 1;

    try {
        ret = start(argc, argv);
    }
    catch (const std::runtime_error &e) {
        etiLog.level(error) << "Runtime error: " << e.what();
    }
    catch (const std::logic_error &e) {
        etiLog.level(error) << "Logic error! " << e.what();
    }

    // To make sure things get printed to stderr
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    return ret;
}
