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

#include "EDIConfig.h"
#include "AFPacket.h"
#include "PFT.h"
#include "Socket.h"
#include <chrono>
#include <map>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>

namespace edi {

/** ETI/STI sender for EDI output */
class Sender {
    public:
        Sender(const configuration_t& conf);
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;
        Sender(Sender&&) = delete;
        Sender& operator=(Sender&&) = delete;
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

        struct stats_t {
            uint16_t listen_port;
            std::vector<Socket::TCPConnection::stats_t> stats;
        };
        std::vector<stats_t> get_tcp_server_stats() const;

    private:
        configuration_t m_conf;

        // The TagPacket will then be placed into an AFPacket
        edi::AFPacketiser edi_af_packetiser;

        // PFT spreading requires sending UDP packets at specific time,
        // independently of time when write() gets called
        bool m_running = false;
        std::thread m_thread;
        virtual void run();





        struct i_sender {
            virtual void send_packet(const std::vector<uint8_t> &frame) = 0;
            virtual ~i_sender() { }
        };

        struct udp_sender_t : public i_sender {
            udp_sender_t(
                    std::string dest_addr,
                    uint16_t dest_port,
                    Socket::UDPSocket&& sock);

            std::string dest_addr;
            uint16_t dest_port;
            Socket::UDPSocket sock;

            virtual void send_packet(const std::vector<uint8_t> &frame) override;
        };

        struct tcp_dispatcher_t : public i_sender {
            tcp_dispatcher_t(
                    uint16_t listen_port,
                    size_t max_frames_queued,
                    size_t tcp_server_preroll_buffers);

            uint16_t listen_port;
            Socket::TCPDataDispatcher sock;
            virtual void send_packet(const std::vector<uint8_t> &frame) override;
        };

        struct tcp_send_client_t : public i_sender {
            tcp_send_client_t(
                    const std::string& dest_addr,
                    uint16_t dest_port);

            Socket::TCPSendClient sock;
            virtual void send_packet(const std::vector<uint8_t> &frame) override;
        };

        class PFTSpreader {
            public:
                using sender_sp = std::shared_ptr<i_sender>;
                PFTSpreader(const pft_settings_t &conf, sender_sp sender);
                sender_sp sender;
                edi::PFT edi_pft;

                void send_af_packet(const AFPacket &af_packet);
                void tick(const std::chrono::steady_clock::time_point& now);

            private:
                // send_af_packet() and tick() are called from different threads, both
                // are accessing m_pending_frames
                std::mutex m_mutex;
                std::map<std::chrono::steady_clock::time_point, edi::PFTFragment> m_pending_frames;
                pft_settings_t settings;
                size_t last_num_pft_fragments = 0;
        };

        std::vector<std::shared_ptr<PFTSpreader>> m_pft_spreaders;
};
}
