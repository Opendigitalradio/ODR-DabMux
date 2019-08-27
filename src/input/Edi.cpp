/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
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

#include "input/Edi.h"

#include <regex>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include "utils.h"

using namespace std;

namespace Inputs {

constexpr bool VERBOSE = false;
constexpr size_t TCP_BLOCKSIZE = 2048;
constexpr size_t MAX_FRAMES_QUEUED = 1000;

Edi::Edi() :
    m_tcp_receive_server(TCP_BLOCKSIZE),
    m_sti_writer(),
    m_sti_decoder(m_sti_writer, VERBOSE)
{ }

Edi::~Edi() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Edi::open(const std::string& name)
{
    const std::regex re_udp("udp://:([0-9]+)");
    const std::regex re_tcp("tcp://(.*):([0-9]+)");

    lock_guard<mutex> lock(m_mutex);

    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }

    std::smatch m;
    if (std::regex_match(name, m, re_udp)) {
        const int udp_port = std::stoi(m[1].str());
        m_input_used = InputUsed::UDP;
        m_udp_sock.reinit(udp_port);
        m_udp_sock.setBlocking(false);
        // TODO multicast
    }
    else if (std::regex_match(name, m, re_tcp)) {
        m_input_used = InputUsed::TCP;
        const string addr = m[1].str();
        const int tcp_port = std::stoi(m[2].str());
        m_tcp_receive_server.start(tcp_port, addr);
    }
    else {
        throw runtime_error("Cannot parse EDI input URI");
    }

    m_name = name;

    m_running = true;
    m_thread = std::thread(&Edi::m_run, this);
}

int Edi::readFrame(uint8_t* buffer, size_t size)
{
    if (m_pending_sti_frame.frame.empty()) {
        m_frames.try_pop(m_pending_sti_frame);
    }

    if (not m_pending_sti_frame.frame.empty()) {
        if (m_pending_sti_frame.frame.size() != size) {
            etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " <<
                m_pending_sti_frame.frame.size() << " received, " << size << " requested";
            memset(buffer, 0, size * sizeof(*buffer));
        }
        else {
            const auto now = chrono::system_clock::now();

            if (m_pending_sti_frame.timestamp.to_system_clock() <= now) {
                etiLog.level(debug) << "EDI input take frame with TS " <<
                    m_pending_sti_frame.timestamp.to_string() << " queue size " << m_frames.size();

                std::copy(m_pending_sti_frame.frame.cbegin(), m_pending_sti_frame.frame.cend(), buffer);
                m_pending_sti_frame.frame.clear();
            }
            else {
                etiLog.level(debug) << "EDI input skip frame with TS " <<
                    m_pending_sti_frame.timestamp.to_string() << " queue size " << m_frames.size();
            }
        }
    }

    return size;

#if 0
    EdiDecoder::sti_frame_t sti;
    if (m_is_prebuffering) {
        m_is_prebuffering = m_frames.size() < 10;
        if (not m_is_prebuffering) {
            etiLog.level(info) << "EDI input " << m_name << " pre-buffering complete.";
        }
    }
    else if (m_frames.try_pop(sti)) {
        if (sti.frame.size() == 0) {
            etiLog.level(debug) << "EDI input " << m_name << " empty frame";
            return 0;
        }
        else if (sti.frame.size() == size) {
            std::copy(sti.frame.cbegin(), sti.frame.cend(), buffer);
        }
        else {
            etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " << sti.frame.size() <<
                " received, " << size << " requested";
            memset(buffer, 0, size * sizeof(*buffer));
        }
    }
    else {
        memset(buffer, 0, size * sizeof(*buffer));
        m_is_prebuffering = true;
        etiLog.level(info) << "EDI input " << m_name << " re-enabling pre-buffering";
    }
    return size;
#endif
}

void Edi::m_run()
{
    while (m_running) {
        bool work_done = false;

        switch (m_input_used) {
            case InputUsed::UDP:
                {
                    constexpr size_t packsize = 2048;
                    const auto packet = m_udp_sock.receive(packsize);
                    if (packet.buffer.size() == packsize) {
                        fprintf(stderr, "Warning, possible UDP truncation\n");
                    }
                    if (not packet.buffer.empty()) {
                        m_sti_decoder.push_packet(packet.buffer);
                        work_done = true;
                    }
                }
                break;
            case InputUsed::TCP:
                {
                    auto packet = m_tcp_receive_server.receive();
                    if (not packet.empty()) {
                        m_sti_decoder.push_bytes(packet);
                        work_done = true;
                    }
                }
                break;
            default:
                throw logic_error("unimplemented input");
        }

        const auto sti = m_sti_writer.getFrame();
        if (not sti.frame.empty()) {
            m_frames.push_wait_if_full(move(sti), MAX_FRAMES_QUEUED);
            work_done = true;
        }

        if (not work_done) {
            // Avoid fast loop
            this_thread::sleep_for(chrono::milliseconds(12));
        }
    }
}

int Edi::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        throw invalid_argument("Invalid bitrate " + to_string(bitrate) + " for " + m_name);
    }

    return bitrate;
}

int Edi::close()
{
    m_udp_sock.close();
    return 0;
}

}
