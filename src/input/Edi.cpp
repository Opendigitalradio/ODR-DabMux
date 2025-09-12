/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
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

#include "input/Edi.h"

#include <regex>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include "Socket.h"
#include "edi/common.hpp"
#include "utils.h"

using namespace std;

namespace Inputs {

constexpr std::array<uint8_t, 256> itel_bytemap_v6 = {
    0x00, 0x06, 0xAD, 0xA9, 0xEC, 0xF8, 0x70, 0x54, 0xD6, 0x88, 0xB4, 0x97, 0xBE, 0xF2, 0x38, 0x09,
    0x19, 0xA3, 0xE1, 0x9E, 0x76, 0x08, 0x66, 0x4E, 0x18, 0x81, 0x4A, 0xF5, 0x33, 0x5F, 0xCA, 0x1A,
    0x8E, 0x93, 0xE6, 0x2D, 0x26, 0x07, 0xCE, 0x96, 0xD3, 0x65, 0x7F, 0x98, 0x30, 0xC6, 0x0C, 0x2C,
    0x36, 0x4D, 0x91, 0x56, 0x37, 0x25, 0x45, 0xD4, 0xAC, 0x29, 0x0D, 0xBA, 0x0A, 0x6E, 0x68, 0x3D,
    0xB2, 0x4B, 0xE8, 0x0B, 0x10, 0x21, 0x72, 0xB5, 0x58, 0xCB, 0x53, 0x95, 0x47, 0xFE, 0x7D, 0x48,
    0xD7, 0x9F, 0xF9, 0x6F, 0xE7, 0x16, 0xF4, 0xCF, 0xAB, 0x3F, 0x92, 0xD1, 0xE3, 0x1B, 0xFB, 0x12,
    0x1C, 0x67, 0x6B, 0xC9, 0x8A, 0xB3, 0x35, 0x8C, 0x69, 0xB8, 0xE9, 0x15, 0x3B, 0x23, 0xA5, 0x7C,
    0xB6, 0x42, 0xE5, 0xB7, 0x13, 0xAE, 0x5D, 0x52, 0x63, 0x89, 0xDD, 0xF0, 0xB9, 0xF6, 0x59, 0x5B,
    0xF7, 0xD0, 0xA6, 0xC5, 0xDE, 0x43, 0x14, 0x41, 0x80, 0x34, 0x05, 0xC4, 0xE2, 0x74, 0x4C, 0xA4,
    0x94, 0x2A, 0x8F, 0xC0, 0x75, 0xF3, 0x20, 0x04, 0x7A, 0x2F, 0x3A, 0x55, 0xA8, 0x1D, 0x2B, 0x2E,
    0x82, 0xBD, 0xCD, 0x27, 0xFD, 0x6C, 0x73, 0x64, 0x8B, 0xE0, 0xD2, 0xC2, 0x9B, 0x60, 0x1F, 0x01,
    0xFA, 0xBF, 0xF1, 0xEB, 0xDC, 0x86, 0x99, 0xED, 0x83, 0xA7, 0xDB, 0x17, 0x6D, 0xC1, 0x62, 0xEE,
    0x57, 0x51, 0x5E, 0x79, 0xAF, 0xA1, 0x3C, 0x5A, 0x0F, 0xA2, 0x84, 0x3E, 0xDA, 0x85, 0x9A, 0x71,
    0x7E, 0x50, 0x11, 0xD5, 0xE4, 0xD9, 0x32, 0x02, 0x24, 0xC8, 0xC3, 0xC7, 0x1E, 0x46, 0x6A, 0x8D,
    0x90, 0xBB, 0x39, 0x78, 0xD8, 0xB0, 0xEA, 0xBC, 0x7B, 0x0E, 0x61, 0x49, 0xFC, 0xB1, 0x31, 0x87,
    0x5C, 0x22, 0x9C, 0x03, 0x44, 0xA0, 0x40, 0xEF, 0x9D, 0x77, 0xAA, 0xDF, 0x4F, 0xCC, 0x28, 0xFF
};

constexpr std::array<uint8_t, 256> itel_bytemap_v5 = {
    0x00, 0xF7, 0x06, 0xD0, 0xAD, 0xA6, 0xA9, 0xC5, 0xEC, 0xDE, 0xF8, 0x43, 0x70, 0x14, 0x54, 0x41,
    0xD6, 0x80, 0x88, 0x34, 0xB4, 0x05, 0x97, 0xC4, 0xBE, 0xE2, 0xF2, 0x74, 0x38, 0x4C, 0x09, 0xA4,
    0x19, 0x94, 0xA3, 0x2A, 0xE1, 0x8F, 0x9E, 0xC0, 0x76, 0x75, 0x08, 0xF3, 0x66, 0x20, 0x4E, 0x04,
    0x18, 0x7A, 0x81, 0x2F, 0x4A, 0x3A, 0xF5, 0x55, 0x33, 0xA8, 0x5F, 0x1D, 0xCA, 0x2B, 0x1A, 0x2E,
    0x8E, 0x82, 0x93, 0xBD, 0xE6, 0xCD, 0x2D, 0x27, 0x26, 0xFD, 0x07, 0x6C, 0xCE, 0x73, 0x96, 0x64,
    0xD3, 0x8B, 0x65, 0xE0, 0x7F, 0xD2, 0x98, 0xC2, 0x30, 0x9B, 0xC6, 0x60, 0x0C, 0x1F, 0x2C, 0x01,
    0x36, 0xFA, 0x4D, 0xBF, 0x91, 0xF1, 0x56, 0xEB, 0x37, 0xDC, 0x25, 0x86, 0x45, 0x99, 0xD4, 0xED,
    0xAC, 0x83, 0x29, 0xA7, 0x0D, 0xDB, 0xBA, 0x17, 0x0A, 0x6D, 0x6E, 0xC1, 0x68, 0x62, 0x3D, 0xEE,
    0xB2, 0x57, 0x4B, 0x51, 0xE8, 0x5E, 0x0B, 0x79, 0x10, 0xAF, 0x21, 0xA1, 0x72, 0x3C, 0xB5, 0x5A,
    0x58, 0x0F, 0xCB, 0xA2, 0x53, 0x84, 0x95, 0x3E, 0x47, 0xDA, 0xFE, 0x85, 0x7D, 0x9A, 0x48, 0x71,
    0xD7, 0x7E, 0x9F, 0x50, 0xF9, 0x11, 0x6F, 0xD5, 0xE7, 0xE4, 0x16, 0xD9, 0xF4, 0x32, 0xCF, 0x02,
    0xAB, 0x24, 0x3F, 0xC8, 0x92, 0xC3, 0xD1, 0xC7, 0xE3, 0x1E, 0x1B, 0x46, 0xFB, 0x6A, 0x12, 0x8D,
    0x1C, 0x90, 0x67, 0xBB, 0x6B, 0x39, 0xC9, 0x78, 0x8A, 0xD8, 0xB3, 0xB0, 0x35, 0xEA, 0x8C, 0xBC,
    0x69, 0x7B, 0xB8, 0x0E, 0xE9, 0x61, 0x15, 0x49, 0x3B, 0xFC, 0x23, 0xB1, 0xA5, 0x31, 0x7C, 0x87,
    0xB6, 0x5C, 0x42, 0x22, 0xE5, 0x9C, 0xB7, 0x03, 0x13, 0x44, 0xAE, 0xA0, 0x5D, 0x40, 0x52, 0xEF,
    0x63, 0x9D, 0x89, 0x77, 0xDD, 0xAA, 0xF0, 0xDF, 0xB9, 0x4F, 0xF6, 0xCC, 0x59, 0x28, 0x5B, 0xFF
};

constexpr size_t TCP_BLOCKSIZE = 2048;

Edi::Edi(const std::string& name, const dab_input_edi_config_t& config) :
    RemoteControllable(name),
    m_tcp_receive_server(TCP_BLOCKSIZE),
    m_sti_writer(bind(&Edi::m_new_sti_frame_callback, this, placeholders::_1)),
    m_sti_decoder(m_sti_writer),
    m_max_frames_overrun(config.buffer_size),
    m_num_frames_prebuffering(config.prebuffering),
    m_allow_non_compliant_itel(config.allow_non_compliant_itel),
    m_name(name),
    m_stats(name)
{
    constexpr bool VERBOSE = false;
    m_sti_decoder.set_verbose(VERBOSE);

    RC_ADD_PARAMETER(buffermanagement,
            "Set type of buffer management to use [prebuffering, timestamped]");

    RC_ADD_PARAMETER(buffer,
            "Size of the input buffer [24ms frames]");

    RC_ADD_PARAMETER(prebuffering,
            "Min buffer level before streaming starts [24ms frames]");

    RC_ADD_PARAMETER(tistdelay, "TIST delay to add [ms]");
}

Edi::~Edi() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Edi::open(const std::string& name)
{
    const std::regex re_udp("udp://:([0-9]+)");
    const std::regex re_udp_bindto("udp://([^:]+):([0-9]+)");
    const std::regex re_udp_multicast("udp://@([0-9.]+):([0-9]+)");
    const std::regex re_udp_multicast_bindto("udp://([0-9.])+@([0-9.]+):([0-9]+)");
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
    }
    else if (std::regex_match(name, m, re_udp_bindto)) {
        const int udp_port = std::stoi(m[2].str());
        m_input_used = InputUsed::UDP;
        m_udp_sock.reinit(udp_port, m[1].str());
        m_udp_sock.setBlocking(false);
    }
    else if (std::regex_match(name, m, re_udp_multicast_bindto)) {
        const string bind_to = m[1].str();
        const string multicast_address = m[2].str();
        const int udp_port = std::stoi(m[3].str());

        m_input_used = InputUsed::UDP;
        if (IN_MULTICAST(ntohl(inet_addr(multicast_address.c_str())))) {
            m_udp_sock.init_receive_multicast(udp_port, bind_to, multicast_address);
        }
        else {
            throw runtime_error(string("Address ") + multicast_address + " is not a multicast address");
        }
        m_udp_sock.setBlocking(false);
    }
    else if (std::regex_match(name, m, re_udp_multicast)) {
        const string multicast_address = m[1].str();
        const int udp_port = std::stoi(m[2].str());
        m_input_used = InputUsed::UDP;
        if (IN_MULTICAST(ntohl(inet_addr(multicast_address.c_str())))) {
            m_udp_sock.init_receive_multicast(udp_port, "0.0.0.0", multicast_address);
        }
        else {
            throw runtime_error(string("Address ") + multicast_address + " is not a multicast address");
        }
        m_udp_sock.setBlocking(false);
    }
    else if (std::regex_match(name, m, re_tcp)) {
        m_input_used = InputUsed::TCP;
        const string addr = m[1].str();
        const int tcp_port = std::stoi(m[2].str());
        m_tcp_receive_server.start(tcp_port, addr);
    }
    else {
        throw runtime_error(string("Cannot parse EDI input URI '") + name + "'");
    }

    m_stats.registerAtServer();

    m_running = true;
    m_thread = std::thread(&Edi::m_run, this);
}

void Edi::applyItelTransformation(std::vector<uint8_t>* frame) {
    if (!m_allow_non_compliant_itel) {
        return;
    }
    if (m_encoder_version == "ITEL-DabEnc-v6 ") {
        std::for_each((*frame).begin(), (*frame).end(), [&](uint8_t &i){
            i = itel_bytemap_v6[i];
        });
    } else if (m_encoder_version == "ITEL-DabEnc-v5 ") {
        std::for_each((*frame).begin(), (*frame).end(), [&](uint8_t &i){
            i = itel_bytemap_v5[i];
        });
    }
}

size_t Edi::readFrame(uint8_t *buffer, size_t size)
{
    // Save stats data in bytes, not in frames
    m_stats.notifyBuffer(m_frames.size() * size);
    m_stats.notifyTimestampOffset(0);

    EdiDecoder::sti_frame_t sti;
    if (m_is_prebuffering) {
        m_is_prebuffering = m_frames.size() < m_num_frames_prebuffering;
        if (not m_is_prebuffering) {
            etiLog.level(info) << "EDI input " << m_name << " pre-buffering complete.";
        }
        memset(buffer, 0, size * sizeof(*buffer));
        m_stats.notifyUnderrun();
        return 0;
    }
    else if (not m_pending_sti_frame.frame.empty()) {
        // Can only happen when switching from timestamp-based buffer management!
        if (m_pending_sti_frame.frame.size() != size) {
            if (not m_size_mismatch_printed) {
                etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " <<
                    m_pending_sti_frame.frame.size() << " received, " << size << " requested";
                m_size_mismatch_printed = true;
            }
            memset(buffer, 0, size * sizeof(*buffer));
            m_stats.notifyUnderrun();
            return 0;
        }
        else {
            if (not m_pending_sti_frame.version_data.version.empty()) {
                m_encoder_version = m_pending_sti_frame.version_data.version;
                m_stats.notifyVersion(
                        m_pending_sti_frame.version_data.version,
                        m_pending_sti_frame.version_data.uptime_s);
            }
            m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left, m_pending_sti_frame.audio_levels.right);

            applyItelTransformation(&m_pending_sti_frame.frame);

            copy(m_pending_sti_frame.frame.begin(),
                    m_pending_sti_frame.frame.end(),
                    buffer);
            m_pending_sti_frame.frame.clear();
            m_size_mismatch_printed = false;
            return size;
        }
    }
    else if (m_frames.try_pop(sti)) {
        if (sti.frame.size() == 0) {
            etiLog.level(debug) << "EDI input " << m_name << " empty frame";
            memset(buffer, 0, size * sizeof(*buffer));
            m_stats.notifyUnderrun();
            return 0;
        }
        else if (sti.frame.size() == size) {
            // Steady-state when everything works well
            if (m_frames.size() > m_max_frames_overrun) {
                m_stats.notifyOverrun();

                /* If the buffer is too full, we drop as many frames as needed
                 * to get down to the prebuffering size. We would like to have our buffer
                 * filled to the prebuffering length. */
                size_t over_max = m_frames.size() - m_num_frames_prebuffering;

                while (over_max--) {
                    EdiDecoder::sti_frame_t discard;
                    m_frames.try_pop(discard);
                }
            }

            if (not sti.version_data.version.empty()) {
                m_encoder_version = sti.version_data.version;
                m_stats.notifyVersion(
                        sti.version_data.version,
                        sti.version_data.uptime_s);
            }
            m_stats.notifyPeakLevels(sti.audio_levels.left, sti.audio_levels.right);

            applyItelTransformation(&sti.frame);

            copy(sti.frame.cbegin(), sti.frame.cend(), buffer);
            m_size_mismatch_printed = false;
            return size;
        }
        else {
            if (not m_size_mismatch_printed) {
                etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " <<
                    sti.frame.size() << " received, " << size << " requested";
                m_size_mismatch_printed = true;
            }
            memset(buffer, 0, size * sizeof(*buffer));
            m_stats.notifyUnderrun();
            return 0;
        }
    }
    else {
        memset(buffer, 0, size * sizeof(*buffer));
        m_is_prebuffering = true;
        etiLog.level(info) << "EDI input " << m_name << " re-enabling pre-buffering";
        m_size_mismatch_printed = false;
        m_stats.notifyUnderrun();
        return 0;
    }
}

size_t Edi::readFrame(uint8_t *buffer, size_t size, std::time_t seconds, int utco, uint32_t tsta)
{
    if (m_pending_sti_frame.frame.empty()) {
        m_frames.try_pop(m_pending_sti_frame);
    }

    m_stats.notifyBuffer(m_frames.size() * size);

    if (m_is_prebuffering) {
        size_t num_discarded_wrong_size = 0;
        size_t num_discarded_invalid_ts = 0;
        size_t num_discarded_late = 0;

        while (not m_pending_sti_frame.frame.empty()) {
            if (m_pending_sti_frame.frame.size() == size) {
                if (m_pending_sti_frame.timestamp.is_valid()) {
                    auto ts_req = EdiDecoder::frame_timestamp_t::from_unix_epoch(seconds, utco, tsta);
                    ts_req += m_tist_delay;
                    const double offset = ts_req.diff_s(m_pending_sti_frame.timestamp);
                    m_stats.notifyTimestampOffset(offset);

                    if (offset < 0) {
                        // Too far in the future
                        break;
                    }
                    else if (offset < 24e-3) {
                        // Just right
                        m_is_prebuffering = false;
                        etiLog.level(info) <<  "EDI input " << m_name << " valid timestamp, pre-buffering complete." <<
                            " Wrong size: " << num_discarded_wrong_size <<
                            " Invalid TS: " << num_discarded_invalid_ts <<
                            " Late: " << num_discarded_late;

                        if (not m_pending_sti_frame.version_data.version.empty()) {
                            m_encoder_version = m_pending_sti_frame.version_data.version;
                            m_stats.notifyVersion(
                                    m_pending_sti_frame.version_data.version,
                                    m_pending_sti_frame.version_data.uptime_s);
                        }

                        m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left,
                                m_pending_sti_frame.audio_levels.right);

                        applyItelTransformation(&m_pending_sti_frame.frame);

                        copy(m_pending_sti_frame.frame.cbegin(), m_pending_sti_frame.frame.cend(), buffer);
                        m_pending_sti_frame.frame.clear();
                        return size;
                    }
                    else {
                        // Too late
                        num_discarded_late++;
                    }
                }
                else {
                    num_discarded_invalid_ts++;
                }
            }
            else {
                num_discarded_wrong_size++;
            }

            m_pending_sti_frame.frame.clear();
            m_frames.try_pop(m_pending_sti_frame);
        }

        if (num_discarded_wrong_size > 0) {
            etiLog.level(warn) << "EDI input " << m_name << ": " <<
                num_discarded_wrong_size << " packets with wrong size.";
        }

        if (num_discarded_invalid_ts > 0) {
            etiLog.level(warn) << "EDI input " << m_name << ": " <<
                num_discarded_invalid_ts << " packets with invalid timestamp.";
        }

        memset(buffer, 0, size);
        m_stats.notifyUnderrun();
        return 0;
    }
    else {
        if (m_pending_sti_frame.frame.empty()) {
            etiLog.level(warn) << "EDI input " << m_name <<
                " empty, re-enabling pre-buffering";
            memset(buffer, 0, size);
            m_stats.notifyUnderrun();
            m_is_prebuffering = true;
            return 0;
        }
        else if (not m_pending_sti_frame.timestamp.is_valid()) {
            etiLog.level(warn) << "EDI input " << m_name <<
                " invalid timestamp, ignoring";
            memset(buffer, 0, size);
            m_pending_sti_frame.frame.clear();
            m_stats.notifyUnderrun();
            return 0;
        }
        else {
            auto ts_req = EdiDecoder::frame_timestamp_t::from_unix_epoch(seconds, utco, tsta);
            ts_req += m_tist_delay;
            const double offset = m_pending_sti_frame.timestamp.diff_s(ts_req);
            m_stats.notifyTimestampOffset(offset);

            if (-24e-3 < offset and offset <= 0) {
                if (not m_pending_sti_frame.version_data.version.empty()) {
                    m_encoder_version = m_pending_sti_frame.version_data.version;
                    m_stats.notifyVersion(
                            m_pending_sti_frame.version_data.version,
                            m_pending_sti_frame.version_data.uptime_s);
                }

                m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left,
                        m_pending_sti_frame.audio_levels.right);

                applyItelTransformation(&m_pending_sti_frame.frame);

                copy(m_pending_sti_frame.frame.cbegin(), m_pending_sti_frame.frame.cend(), buffer);
                m_pending_sti_frame.frame.clear();
                return size;
            }
            else {
                m_stats.notifyUnderrun();
                m_is_prebuffering = true;
                m_pending_sti_frame.frame.clear();
                etiLog.level(warn) << "EDI input " << m_name <<
                    " timestamp out of bounds, re-enabling pre-buffering";
                memset(buffer, 0, size);
                return 0;
            }
        }
    }
}

void Edi::m_run()
{
    while (m_running) {
        try {
            switch (m_input_used) {
                case InputUsed::UDP:
                    {
                        constexpr size_t packsize = 2048;
                        auto packet = m_udp_sock.receive(packsize);
                        if (packet.buffer.size() == packsize) {
                            fprintf(stderr, "Warning, possible UDP truncation\n");
                        }

                        if (not packet.buffer.empty()) {
                            EdiDecoder::Packet p(move(packet.buffer));
                            m_sti_decoder.push_packet(p);
                        }
                        else {
                            this_thread::sleep_for(chrono::milliseconds(12));
                        }
                    }
                    break;
                case InputUsed::TCP:
                    {
                        auto message = m_tcp_receive_server.receive();
                        if (auto data = dynamic_pointer_cast<Socket::TCPReceiveMessageData>(message)) {
                            m_sti_decoder.push_bytes(data->data);
                        }
                        else if (dynamic_pointer_cast<Socket::TCPReceiveMessageDisconnected>(message)) {
                            etiLog.level(info) << "EDI input " << m_name << " disconnected";
                            m_sti_decoder.push_bytes({}); // Push an empty frame to clear the internal state
                        }
                        else if (dynamic_pointer_cast<Socket::TCPReceiveMessageEmpty>(message)) {
                            this_thread::sleep_for(chrono::milliseconds(12));
                        }
                        else {
                            throw logic_error("unimplemented TCPReceiveMessage type");
                        }
                    }
                    break;
                default:
                    throw logic_error("unimplemented input");
            }
        }
        catch (const invalid_argument& e) {
            etiLog.level(warn) << "EDI input " << m_name << " exception: " << e.what();
            m_sti_decoder.push_bytes({}); // Push an empty frame to clear the internal state
            this_thread::sleep_for(chrono::milliseconds(8));
        }
        catch (const runtime_error& e) {
            etiLog.level(warn) << "EDI input " << m_name << " exception: " << e.what();
            m_sti_decoder.push_bytes({}); // Push an empty frame to clear the internal state
            this_thread::sleep_for(chrono::milliseconds(8));
        }
    }
}

void Edi::m_new_sti_frame_callback(EdiDecoder::sti_frame_t&& sti) {
    if (not sti.frame.empty()) {
        // We should not wait here, because we want the complete input buffering
        // happening inside m_frames. Using the blocking function is only a protection
        // against runaway memory usage if something goes wrong in the consumer.
        m_frames.push_wait_if_full(move(sti), m_max_frames_overrun * 2);
    }
}

int Edi::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        throw invalid_argument("Invalid bitrate " + to_string(bitrate) + " for " + m_name);
    }

    return bitrate;
}

void Edi::close()
{
    m_udp_sock.close();
}


void Edi::set_parameter(const std::string& parameter, const std::string& value)
{
    if (parameter == "buffer") {
        size_t new_limit = atol(value.c_str());
        m_max_frames_overrun = new_limit;
    }
    else if (parameter == "prebuffering") {
        size_t new_limit = atol(value.c_str());
        m_num_frames_prebuffering = new_limit;
    }
    else if (parameter == "buffermanagement") {
        if (value == "prebuffering") {
            setBufferManagement(Inputs::BufferManagement::Prebuffering);
        }
        else if (value == "timestamped") {
            setBufferManagement(Inputs::BufferManagement::Timestamped);
        }
        else {
            throw ParameterError("Invalid value for '" + parameter + "' in controllable " + get_rc_name());
        }
    }
    else if (parameter == "tistdelay") {
        m_tist_delay = chrono::milliseconds(stoi(value));
    }
    else {
        throw ParameterError("Parameter '" + parameter + "' is not exported by controllable " + get_rc_name());
    }
}

const std::string Edi::get_parameter(const std::string& parameter) const
{
    stringstream ss;
    if (parameter == "buffer") {
        ss << m_max_frames_overrun;
    }
    else if (parameter == "prebuffering") {
        ss << m_num_frames_prebuffering;
    }
    else if (parameter == "buffermanagement") {
        switch (getBufferManagement()) {
            case Inputs::BufferManagement::Prebuffering:
                ss << "prebuffering";
                break;
            case Inputs::BufferManagement::Timestamped:
                ss << "timestamped";
                break;
        }
    }
    else if (parameter == "tistdelay") {
        ss << m_tist_delay.count();
    }
    else {
        throw ParameterError("Parameter '" + parameter + "' is not exported by controllable " + get_rc_name());
    }
    return ss.str();
}

const json::map_t Edi::get_all_values() const
{
    json::map_t map;
    map["buffer"].v = m_max_frames_overrun;
    map["prebuffering"].v = m_num_frames_prebuffering;
    switch (getBufferManagement()) {
        case Inputs::BufferManagement::Prebuffering:
            map["buffermanagement"].v = "prebuffering";
            break;
        case Inputs::BufferManagement::Timestamped:
            map["buffermanagement"].v = "timestamped";
            break;
    }
    map["tistdelay"].v = m_tist_delay.count();
    return map;
}

}
