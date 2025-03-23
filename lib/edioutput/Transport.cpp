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
#include "Transport.h"
#include "Log.h"
#include <cmath>
#include <thread>

using namespace std;

namespace edi {

void configuration_t::print() const
{
    etiLog.level(info) << "EDI Output";
    etiLog.level(info) << " verbose     " << verbose;
    for (auto edi_dest : destinations) {
        if (auto udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(edi_dest)) {
            etiLog.level(info) << " UDP to " << udp_dest->dest_addr << ":" << udp_dest->dest_port;
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
        etiLog.level(info) << "  PFT=" << edi_dest->pft_settings.enable_pft;
        if (edi_dest->pft_settings.enable_pft) {
            etiLog.level(info) << "  FEC=" << edi_dest->pft_settings.fec;
            etiLog.level(info) << "  Chunk Len=" << edi_dest->pft_settings.chunk_len;
            etiLog.level(info) << "  Fragment spreading factor=" << edi_dest->pft_settings.fragment_spreading_factor;
        }
    }
}


Sender::Sender(const configuration_t& conf) :
    m_conf(conf)
{
    if (m_conf.verbose) {
        etiLog.level(info) << "Setup EDI Output";
    }

    for (const auto& edi_dest : m_conf.destinations) {
        if (const auto udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(edi_dest)) {
            Socket::UDPSocket udp_socket(udp_dest->source_port);

            if (not udp_dest->source_addr.empty()) {
                udp_socket.setMulticastSource(udp_dest->source_addr.c_str());
                udp_socket.setMulticastTTL(udp_dest->ttl);
            }

            auto sender = make_shared<udp_sender_t>(
                    udp_dest->dest_addr,
                    udp_dest->dest_port,
                    std::move(udp_socket));
            m_pft_spreaders.emplace_back(
                make_shared<PFTSpreader>(udp_dest->pft_settings, sender));
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(edi_dest)) {
            auto sender = make_shared<tcp_dispatcher_t>(
                    tcp_dest->listen_port,
                    tcp_dest->max_frames_queued,
                    tcp_dest->tcp_server_preroll_buffers);
            m_pft_spreaders.emplace_back(
                    make_shared<PFTSpreader>(tcp_dest->pft_settings, sender));
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(edi_dest)) {
            auto sender = make_shared<tcp_send_client_t>(tcp_dest->dest_addr, tcp_dest->dest_port);
            m_pft_spreaders.emplace_back(
                    make_shared<PFTSpreader>(tcp_dest->pft_settings, sender));
        }
        else {
            throw logic_error("EDI destination not implemented");
        }
    }

    {
        m_running = true;
        m_thread = thread(&Sender::run, this);
    }

    if (m_conf.verbose) {
        etiLog.log(info, "EDI output set up");
    }
}

void Sender::write(const TagPacket& tagpacket)
{
    // Assemble into one AF Packet
    edi::AFPacket af_packet = edi_af_packetiser.Assemble(tagpacket);

    write(af_packet);
}

void Sender::write(const AFPacket& af_packet)
{
    for (auto& sender : m_pft_spreaders) {
        sender->send_af_packet(af_packet);
    }
}

void Sender::override_af_sequence(uint16_t seq)
{
    edi_af_packetiser.OverrideSeq(seq);
}

void Sender::override_pft_sequence(uint16_t pseq)
{
    for (auto& spreader : m_pft_spreaders) {
        spreader->edi_pft.OverridePSeq(pseq);
    }
}

std::vector<Sender::stats_t> Sender::get_tcp_server_stats() const
{
    std::vector<Sender::stats_t> stats;

    for (auto& spreader : m_pft_spreaders) {
        if (auto sender = std::dynamic_pointer_cast<tcp_dispatcher_t>(spreader->sender)) {
            Sender::stats_t s;
            s.listen_port = sender->listen_port;
            s.stats = sender->sock.get_stats();
            stats.push_back(s);
        }
    }

    return stats;
}

Sender::~Sender()
{
    {
        m_running = false;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Sender::run()
{
    while (m_running) {
        const auto now = chrono::steady_clock::now();
        for (auto& spreader : m_pft_spreaders) {
            spreader->tick(now);
        }

        this_thread::sleep_for(chrono::microseconds(500));
    }
}


void Sender::udp_sender_t::send_packet(const std::vector<uint8_t> &frame)
{
    Socket::InetAddress addr;
    addr.resolveUdpDestination(dest_addr, dest_port);
    sock.send(frame, addr);
}

void Sender::tcp_dispatcher_t::send_packet(const std::vector<uint8_t> &frame)
{
    sock.write(frame);
}

void Sender::tcp_send_client_t::send_packet(const std::vector<uint8_t> &frame)
{
    sock.sendall(frame);
}

Sender::udp_sender_t::udp_sender_t(std::string dest_addr,
                                   uint16_t dest_port,
                                   Socket::UDPSocket&& sock) :
    dest_addr(dest_addr),
    dest_port(dest_port),
    sock(std::move(sock))
{
}

Sender::tcp_dispatcher_t::tcp_dispatcher_t(uint16_t listen_port,
                                           size_t max_frames_queued,
                                           size_t tcp_server_preroll_buffers) :
    listen_port(listen_port),
    sock(max_frames_queued, tcp_server_preroll_buffers)
{
    sock.start(listen_port, "0.0.0.0");
}

Sender::tcp_send_client_t::tcp_send_client_t(const std::string &dest_addr,
                                             uint16_t dest_port) :
    sock(dest_addr, dest_port)
{
}

Sender::PFTSpreader::PFTSpreader(const pft_settings_t& conf, sender_sp sender) :
    sender(sender),
    edi_pft(conf)
{
}

void Sender::PFTSpreader::send_af_packet(const AFPacket& af_packet)
{
    using namespace std::chrono;
    if (edi_pft.is_enabled()) {
        // Apply PFT layer to AF Packet (Reed Solomon FEC and Fragmentation)
        vector<edi::PFTFragment> edi_fragments = edi_pft.Assemble(af_packet);

        if (settings.verbose and last_num_pft_fragments != edi_fragments.size()) {
            etiLog.log(debug, "EDI Output: Number of PFT fragments %zu\n",
                    edi_fragments.size());
            last_num_pft_fragments = edi_fragments.size();
        }

        /* Spread out the transmission of all fragments over part of the 24ms AF packet duration
         * to reduce the risk of losing a burst of fragments because of congestion. */
        auto inter_fragment_wait_time = microseconds(1);
        if (edi_fragments.size() > 1) {
            if (settings.fragment_spreading_factor > 0) {
                inter_fragment_wait_time =
                    microseconds(llrint(
                                settings.fragment_spreading_factor * 24000.0 /
                                edi_fragments.size()
                            ));
            }
        }

        /* Separate insertion into map and transmission so as to make spreading possible */
        const auto now = steady_clock::now();
        {
            auto tp = now;
            unique_lock<mutex> lock(m_mutex);
            for (auto& edi_frag : edi_fragments) {
                m_pending_frames[tp] = std::move(edi_frag);
                tp += inter_fragment_wait_time;
            }
        }
    }
    else /* PFT disabled */ {
        const auto now = steady_clock::now();
        unique_lock<mutex> lock(m_mutex);
        m_pending_frames[now] = std::move(af_packet);
    }

    // Actual transmission done in tick() function
}

void Sender::PFTSpreader::tick(const std::chrono::steady_clock::time_point& now)
{
    unique_lock<mutex> lock(m_mutex);

    for (auto it = m_pending_frames.begin(); it != m_pending_frames.end(); ) {
        const auto& edi_frag = it->second;

        if (it->first <= now) {
            sender->send_packet(edi_frag);
            it = m_pending_frames.erase(it);
        }
        else {
            ++it;
        }
    }
}

} // namespace edi
