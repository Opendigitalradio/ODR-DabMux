/*
   Copyright (C) 2020
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
#include "Transport.h"
#include <iterator>
#include <cmath>

using namespace std;

namespace edi {

void configuration_t::print() const
{
    etiLog.level(info) << "EDI Output";
    etiLog.level(info) << " verbose     " << verbose;
    for (auto edi_dest : destinations) {
        if (auto udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(edi_dest)) {
            etiLog.level(info) << " UDP to " << udp_dest->dest_addr << ":" << dest_port;
            if (not udp_dest->source_addr.empty()) {
                etiLog.level(info) << "  source      " << udp_dest->source_addr;
                etiLog.level(info) << "  ttl         " << udp_dest->ttl;
            }
            etiLog.level(info) << "  source port " << udp_dest->source_port;
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(edi_dest)) {
            etiLog.level(info) << " TCP listening on port " << tcp_dest->listen_port;
            etiLog.level(info) << "  max frames queued    " << tcp_dest->max_frames_queued;
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(edi_dest)) {
            etiLog.level(info) << " TCP client connecting to " << tcp_dest->dest_addr << ":" << tcp_dest->dest_port;
            etiLog.level(info) << "  max frames queued    " << tcp_dest->max_frames_queued;
        }
        else {
            throw logic_error("EDI destination not implemented");
        }
    }
    if (interleaver_enabled()) {
        etiLog.level(info) << " interleave     " << latency_frames * 24 << " ms";
    }
}


Sender::Sender(const configuration_t& conf) :
    m_conf(conf),
    edi_pft(m_conf)
{
    if (m_conf.verbose) {
        etiLog.log(info, "Setup EDI Output");
    }

    for (const auto& edi_dest : m_conf.destinations) {
        if (const auto udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(edi_dest)) {
            auto udp_socket = std::make_shared<Socket::UDPSocket>(udp_dest->source_port);

            if (not udp_dest->source_addr.empty()) {
                udp_socket->setMulticastSource(udp_dest->source_addr.c_str());
                udp_socket->setMulticastTTL(udp_dest->ttl);
            }

            udp_sockets.emplace(udp_dest.get(), udp_socket);
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(edi_dest)) {
            auto dispatcher = make_shared<Socket::TCPDataDispatcher>(tcp_dest->max_frames_queued);
            dispatcher->start(tcp_dest->listen_port, "0.0.0.0");
            tcp_dispatchers.emplace(tcp_dest.get(), dispatcher);
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(edi_dest)) {
            auto tcp_send_client = make_shared<Socket::TCPSendClient>(tcp_dest->dest_addr, tcp_dest->dest_port);
            tcp_senders.emplace(tcp_dest.get(), tcp_send_client);
        }
        else {
            throw logic_error("EDI destination not implemented");
        }
    }

    if (m_conf.interleaver_enabled()) {
        edi_interleaver.SetLatency(m_conf.latency_frames);
    }

    if (m_conf.dump) {
        edi_debug_file.open("./edi.debug");
    }

    if (m_conf.verbose) {
        etiLog.log(info, "EDI output set up");
    }
}

void Sender::write(const TagPacket& tagpacket)
{
    // Assemble into one AF Packet
    edi::AFPacket af_packet = edi_afPacketiser.Assemble(tagpacket);

    if (m_conf.enable_pft) {
        // Apply PFT layer to AF Packet (Reed Solomon FEC and Fragmentation)
        vector<edi::PFTFragment> edi_fragments = edi_pft.Assemble(af_packet);

        if (m_conf.verbose) {
            fprintf(stderr, "EDI Output: Number of PFT fragment before interleaver %zu\n",
                    edi_fragments.size());
        }

        if (m_conf.interleaver_enabled()) {
            edi_fragments = edi_interleaver.Interleave(edi_fragments);
        }

        if (m_conf.verbose) {
            fprintf(stderr, "EDI Output: Number of PFT fragments %zu\n",
                    edi_fragments.size());
        }

        /* Spread out the transmission of all fragments over 75% of the 24ms AF packet duration
         * to reduce the risk of losing fragments because of congestion.
         *
         * 75% was chosen so that other outputs still have time to do their thing. */
        const auto inter_fragment_wait_time = std::chrono::microseconds(llrint(0.75 * 24000.0 / edi_fragments.size()));

        // Send over ethernet
        for (auto& edi_frag : edi_fragments) {
            if (m_conf.dump) {
                ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
                copy(edi_frag.begin(), edi_frag.end(), debug_iterator);
            }

            for (auto& dest : m_conf.destinations) {
                if (const auto& udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(dest)) {
                    Socket::InetAddress addr;
                    addr.resolveUdpDestination(udp_dest->dest_addr, m_conf.dest_port);

                    udp_sockets.at(udp_dest.get())->send(edi_frag, addr);
                }
                else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(dest)) {
                    tcp_dispatchers.at(tcp_dest.get())->write(edi_frag);
                }
                else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(dest)) {
                    tcp_senders.at(tcp_dest.get())->sendall(edi_frag);
                }
                else {
                    throw logic_error("EDI destination not implemented");
                }
            }

            std::this_thread::sleep_for(inter_fragment_wait_time);
        }
    }
    else {
        // Send over ethernet
        if (m_conf.dump) {
            ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
            copy(af_packet.begin(), af_packet.end(), debug_iterator);
        }

        for (auto& dest : m_conf.destinations) {
            if (const auto& udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(dest)) {
                Socket::InetAddress addr;
                addr.resolveUdpDestination(udp_dest->dest_addr, m_conf.dest_port);

                if (af_packet.size() > 1400 and not m_udp_fragmentation_warning_printed) {
                    fprintf(stderr, "EDI Output: AF packet larger than 1400,"
                            " consider using PFT to avoid UP fragmentation.\n");
                    m_udp_fragmentation_warning_printed = true;
                }

                udp_sockets.at(udp_dest.get())->send(af_packet, addr);
            }
            else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(dest)) {
                tcp_dispatchers.at(tcp_dest.get())->write(af_packet);
            }
            else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(dest)) {
                tcp_senders.at(tcp_dest.get())->sendall(af_packet);
            }
            else {
                throw logic_error("EDI destination not implemented");
            }
        }
    }
}

}
