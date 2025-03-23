/*
   Copyright (C) 2025
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   UDP and TCP transports and their configuration

   */
/*
   This file is part of the ODR-mmbTools.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace edi {

/** Configuration for EDI output */

struct pft_settings_t {
    // protection and fragmentation settings
    bool verbose       = false;
    bool enable_pft    = false;
    unsigned chunk_len = 207;        // RSk, data length of each chunk
    unsigned fec       = 0;          // number of fragments that can be recovered
    double fragment_spreading_factor = 0.95;
    // Spread transmission of fragments in time. 1.0 = 100% means spreading over the whole duration of a frame (24ms)
    // Above 100% means that the fragments are spread over several 24ms periods, interleaving the AF packets.
};

struct destination_t {
    virtual ~destination_t() {};

    pft_settings_t pft_settings = {};
};


// Can represent both unicast and multicast destinations
struct udp_destination_t : public destination_t {
    std::string dest_addr;
    uint16_t dest_port = 0;
    std::string source_addr;
    uint16_t source_port = 0;
    uint8_t ttl = 10;
};

// TCP server that can accept multiple connections
struct tcp_server_t : public destination_t {
    unsigned int listen_port = 0;
    size_t max_frames_queued = 1024;

    // The TCP Server output can preroll a fixed number of previous buffers each time a new client connects.
    size_t tcp_server_preroll_buffers = 0;
};

// TCP client that connects to one endpoint
struct tcp_client_t : public destination_t {
    std::string dest_addr;
    unsigned int dest_port = 0;
    size_t max_frames_queued = 1024;
};

struct configuration_t {
    bool verbose = false;
    unsigned int tagpacket_alignment = 0;
    std::vector<std::shared_ptr<destination_t> > destinations;

    bool enabled() const { return destinations.size() > 0; }

    void print() const;
};

}


