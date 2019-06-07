/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   UDP and TCP transports and their configuration

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

#pragma once

#include "config.h"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace edi {

/** Configuration for EDI output */

struct destination_t {
    virtual ~destination_t() {};
};

// Can represent both unicast and multicast destinations
struct udp_destination_t : public destination_t {
    std::string dest_addr;
    std::string source_addr;
    unsigned int source_port = 0;
    unsigned int ttl = 10;
};

// TCP server that can accept multiple connections
struct tcp_server_t : public destination_t {
    unsigned int listen_port = 0;
    size_t max_frames_queued = 1024;
};

// TCP client that connects to one endpoint
struct tcp_client_t : public destination_t {
    std::string dest_addr;
    unsigned int dest_port = 0;
    size_t max_frames_queued = 1024;
};

struct configuration_t {
    unsigned chunk_len = 207;        // RSk, data length of each chunk
    unsigned fec       = 0;          // number of fragments that can be recovered
    bool dump          = false;      // dump a file with the EDI packets
    bool verbose       = false;
    bool enable_pft    = false;      // Enable protection and fragmentation
    unsigned int tagpacket_alignment = 0;
    std::vector<std::shared_ptr<destination_t> > destinations;
    unsigned int dest_port = 0;      // common destination port, because it's encoded in the transport layer
    unsigned int latency_frames = 0; // if nonzero, enable interleaver with a latency of latency_frames * 24ms

    bool enabled() const { return destinations.size() > 0; }
    bool interleaver_enabled() const { return latency_frames > 0; }

    void print() const;
};

}


