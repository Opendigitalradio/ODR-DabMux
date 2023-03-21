/*
   Copyright (C) 2022
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

#include "EDIConfig.h"
#include "edi_ts.h"
#include "AFPacket.h"
#include "PFT.h"
#include "Socket.h"
#include <vector>
#include <chrono>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <fstream>
#include <cstdint>
#include <thread>
#include <mutex>

namespace edi {

/** STI sender for EDI output */

class Sender {
    public:
        Sender(const configuration_t& conf);
        Sender(const Sender&) = delete;
        Sender operator=(const Sender&) = delete;
        ~Sender();

        // Assemble the tagpacket into an AF packet, and if needed,
        // apply PFT and then schedule for transmission.
        void write(const TagPacket& tagpacket);

        // Schedule an already assembled AF Packet for transmission,
        // applying PFT if needed.
        void write(const AFPacket& af_packet);

        // Set the sequence numbers to be used for the next call to write()
        // seq is for the AF layer
        // pseq is for the PFT layer
        void override_af_sequence(uint16_t seq);
        void override_pft_sequence(uint16_t pseq);

    private:
        void run();

        bool m_udp_fragmentation_warning_printed = false;

        configuration_t m_conf;
        std::ofstream edi_debug_file;

        // The TagPacket will then be placed into an AFPacket
        edi::AFPacketiser edi_afPacketiser;

        // The AF Packet will be protected with reed-solomon and split in fragments
        edi::PFT edi_pft;

        std::unordered_map<udp_destination_t*, std::shared_ptr<Socket::UDPSocket>> udp_sockets;
        std::unordered_map<tcp_server_t*, std::shared_ptr<Socket::TCPDataDispatcher>> tcp_dispatchers;
        std::unordered_map<tcp_client_t*, std::shared_ptr<Socket::TCPSendClient>> tcp_senders;
        std::unordered_map<ts_destination_t*, std::shared_ptr<edi_ts>> ts_senders;

        // PFT spreading requires sending UDP packets at specific time, independently of
        // time when write() gets called
        std::thread m_thread;
        std::mutex m_mutex;
        bool m_running = false;
        std::map<std::chrono::steady_clock::time_point, edi::PFTFragment> m_pending_frames;

        size_t m_last_num_pft_fragments = 0;
};

}

