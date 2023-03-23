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
#include "Transport.h"
#include <iterator>
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
        else if (auto ts_dest = dynamic_pointer_cast<edi::ts_destination_t>(edi_dest)) {
            etiLog.level(info) << " TS to " << ts_dest->output << "://" << ts_dest->output_host << ":" << ts_dest->output_port;
            etiLog.level(info) << " payload pid  " << ts_dest->payload_pid;
            etiLog.level(info) << " pmt pid      " << ts_dest->pmt_pid;
            etiLog.level(info) << " ts id      " << ts_dest->ts_id;
            etiLog.level(info) << " service type " << ts_dest->service_type;
            etiLog.level(info) << " service id   " << ts_dest->service_id;
            etiLog.level(info) << " service name " << ts_dest->service_name;
            etiLog.level(info) << " service provider name  " << ts_dest->service_provider_name;
            etiLog.level(info) << " output  " << ts_dest->output;
            etiLog.level(info) << " output host  " << ts_dest->output_host;
            etiLog.level(info) << " output port " << ts_dest->output_port;
            etiLog.level(info) << " output ttl  " << ts_dest->output_ttl;
            etiLog.level(info) << " output source ip " << ts_dest->output_source_address;
            
            if (ts_dest->output == "srt")
            {
                etiLog.level(info) << " SRT Passphrase " << ts_dest->output_srt_passphrase;
            }

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
}


Sender::Sender(const configuration_t& conf) :
    m_conf(conf),
    edi_pft(m_conf)
{
    if (m_conf.verbose) {
        etiLog.level(info) << "Setup EDI Output";
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
        //TODO
        else if (const auto ts_dest = dynamic_pointer_cast<edi::ts_destination_t>(edi_dest)) {
            
            auto ts = std::make_shared<edi_ts>();
            
            ts->payload_pid = ts_dest->payload_pid;
            ts->pmt_pid = ts_dest->pmt_pid;
            ts->ts_id = ts_dest->ts_id;
            ts->service_id = ts_dest->service_id;
            ts->service_type = ts_dest->service_type;
            ts->service_name = ts_dest->service_name;
            ts->service_provider_name = ts_dest->service_provider_name;
            ts->output = ts_dest->output;
            ts->output_host = ts_dest->output_host;
            ts->output_port = ts_dest->output_port;
            ts->output_ttl = ts_dest->output_ttl;
            ts->output_source_address = ts_dest->output_source_address;
            
            if (ts_dest->output == "srt")
            {
            ts->output_srt_passphrase = ts_dest->output_srt_passphrase;
            }

            ts->Open("TS_" + ts_dest->output_host);
            ts_senders.emplace(ts_dest.get(), ts);
        }
        else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(edi_dest)) {
            auto dispatcher = make_shared<Socket::TCPDataDispatcher>(
                    tcp_dest->max_frames_queued, tcp_dest->tcp_server_preroll_buffers);

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

    if (m_conf.dump) {
        edi_debug_file.open("./edi.debug");
    }

    if (m_conf.enable_pft) {
        unique_lock<mutex> lock(m_mutex);
        m_running = true;
        m_thread = thread(&Sender::run, this);
    }

    if (m_conf.verbose) {
        etiLog.log(info, "EDI output set up");
    }
}

Sender::~Sender()
{
    {
        unique_lock<mutex> lock(m_mutex);
        m_running = false;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Sender::write(const TagPacket& tagpacket)
{
    // Assemble into one AF Packet
    edi::AFPacket af_packet = edi_afPacketiser.Assemble(tagpacket);

    write(af_packet);
}

void Sender::write(const AFPacket& af_packet)
{
    if (m_conf.enable_pft) {
        // Apply PFT layer to AF Packet (Reed Solomon FEC and Fragmentation)
        vector<edi::PFTFragment> edi_fragments = edi_pft.Assemble(af_packet);

        if (m_conf.verbose and m_last_num_pft_fragments != edi_fragments.size()) {
            etiLog.log(debug, "EDI Output: Number of PFT fragments %zu\n",
                    edi_fragments.size());
            m_last_num_pft_fragments = edi_fragments.size();
        }

        /* Spread out the transmission of all fragments over part of the 24ms AF packet duration
         * to reduce the risk of losing a burst of fragments because of congestion. */
        using namespace std::chrono;
        auto inter_fragment_wait_time = microseconds(1);
        if (edi_fragments.size() > 1) {
            if (m_conf.fragment_spreading_factor > 0) {
                inter_fragment_wait_time =
                    microseconds(
                            llrint(m_conf.fragment_spreading_factor * 24000.0 / edi_fragments.size())
                            );
            }
        }

        /* Separate insertion into map and transmission so as to make spreading possible */
        const auto now = steady_clock::now();
        {
            auto tp = now;
            unique_lock<mutex> lock(m_mutex);
            for (auto& edi_frag : edi_fragments) {
                m_pending_frames[tp] = move(edi_frag);
                tp += inter_fragment_wait_time;
            }
        }

        // Transmission done in run() function
    }
    else /* PFT disabled */ {
        // Send over ethernet
        if (m_conf.dump) {
            ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
            copy(af_packet.begin(), af_packet.end(), debug_iterator);
        }

        for (auto& dest : m_conf.destinations) {
            if (const auto& udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(dest)) {
                Socket::InetAddress addr;
                addr.resolveUdpDestination(udp_dest->dest_addr, udp_dest->dest_port);

                if (af_packet.size() > 1400 and not m_udp_fragmentation_warning_printed) {
                    fprintf(stderr, "EDI Output: AF packet larger than 1400,"
                            " consider using PFT to avoid UP fragmentation.\n");
                    m_udp_fragmentation_warning_printed = true;
                }

                udp_sockets.at(udp_dest.get())->send(af_packet, addr);
            }
            //TODO
            else if (auto ts_dest = dynamic_pointer_cast<edi::ts_destination_t>(dest)) {
                //Socket::InetAddress addr;
                //addr.resolveUdpDestination(udp_dest->dest_addr, udp_dest->dest_port);

                if (af_packet.size() > 1400 and not m_udp_fragmentation_warning_printed) {
                    fprintf(stderr, "EDI Output: AF packet larger than 1400,"
                            " consider using PFT to avoid UP fragmentation.\n");
                    m_udp_fragmentation_warning_printed = true;
                }

            ts_senders.at(ts_dest.get())->send(af_packet);
           
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

void Sender::override_af_sequence(uint16_t seq)
{
    edi_afPacketiser.OverrideSeq(seq);
}

void Sender::override_pft_sequence(uint16_t pseq)
{
    edi_pft.OverridePSeq(pseq);
}

void Sender::run()
{
    while (m_running) {
        unique_lock<mutex> lock(m_mutex);
        const auto now = chrono::steady_clock::now();

        // Send over ethernet
        for (auto it = m_pending_frames.begin(); it != m_pending_frames.end(); ) {
            const auto& edi_frag = it->second;

            if (it->first <= now) {
                if (m_conf.dump) {
                    ostream_iterator<uint8_t> debug_iterator(edi_debug_file);
                    copy(edi_frag.begin(), edi_frag.end(), debug_iterator);
                }

                for (auto& dest : m_conf.destinations) {
                    if (const auto& udp_dest = dynamic_pointer_cast<edi::udp_destination_t>(dest)) {
                        Socket::InetAddress addr;
                        addr.resolveUdpDestination(udp_dest->dest_addr, udp_dest->dest_port);

                        udp_sockets.at(udp_dest.get())->send(edi_frag, addr);
                    }
                    else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_server_t>(dest)) {
                        tcp_dispatchers.at(tcp_dest.get())->write(edi_frag);
                    }
                    else if (auto tcp_dest = dynamic_pointer_cast<edi::tcp_client_t>(dest)) {
                        tcp_senders.at(tcp_dest.get())->sendall(edi_frag);
                    }
                    else if (auto ts_dest = dynamic_pointer_cast<edi::ts_destination_t>(dest)) {
                          ts_senders.at(ts_dest.get())->send(edi_frag);
                    }
                    else {
                        throw logic_error("EDI destination not implemented");
                    }
                }
                it = m_pending_frames.erase(it);
            }
            else {
                ++it;
            }
        }

        lock.unlock();
        this_thread::sleep_for(chrono::microseconds(500));
    }
}

}
