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
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include "Socket.h"
#include "edi/common.hpp"
#include "utils.h"

using namespace std;

namespace Inputs {

constexpr size_t TCP_BLOCKSIZE = 2048;

Edi::Edi(const std::string& name, const dab_input_edi_config_t& config) :
    RemoteControllable(name),
    m_tcp_receive_server(TCP_BLOCKSIZE),
    m_sti_writer(bind(&Edi::m_new_sti_frame_callback, this, placeholders::_1)),
    m_sti_decoder(m_sti_writer),
    m_max_frames_overrun(config.buffer_size),
    m_num_frames_prebuffering(config.prebuffering),
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
    const std::regex re_udp_multicast("udp://@([0-9.]+):([0-9]+)");
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
    else if (std::regex_match(name, m, re_udp_multicast)) {
        const string multicast_address = m[1].str();
        const int udp_port = std::stoi(m[2].str());
        m_input_used = InputUsed::UDP;
        m_udp_sock.reinit(udp_port);
        m_udp_sock.setBlocking(false);
        m_udp_sock.joinGroup(multicast_address.c_str());
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
            etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " <<
                m_pending_sti_frame.frame.size() << " received, " << size << " requested";
            memset(buffer, 0, size * sizeof(*buffer));
            m_stats.notifyUnderrun();
            return 0;
        }
        else {
            if (not m_pending_sti_frame.version_data.version.empty()) {
                m_stats.notifyVersion(
                        m_pending_sti_frame.version_data.version,
                        m_pending_sti_frame.version_data.uptime_s);
            }
            m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left, m_pending_sti_frame.audio_levels.right);

            copy(m_pending_sti_frame.frame.begin(),
                    m_pending_sti_frame.frame.end(),
                    buffer);
            m_pending_sti_frame.frame.clear();
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
                m_stats.notifyVersion(
                        sti.version_data.version,
                        sti.version_data.uptime_s);
            }
            m_stats.notifyPeakLevels(sti.audio_levels.left, sti.audio_levels.right);

            copy(sti.frame.cbegin(), sti.frame.cend(), buffer);
            return size;
        }
        else {
            etiLog.level(debug) << "EDI input " << m_name << " size mismatch: " <<
                sti.frame.size() << " received, " << size << " requested";
            memset(buffer, 0, size * sizeof(*buffer));
            m_stats.notifyUnderrun();
            return 0;
        }
    }
    else {
        memset(buffer, 0, size * sizeof(*buffer));
        m_is_prebuffering = true;
        etiLog.level(info) << "EDI input " << m_name << " re-enabling pre-buffering";
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
                if (m_pending_sti_frame.timestamp.valid()) {
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
                            m_stats.notifyVersion(
                                    m_pending_sti_frame.version_data.version,
                                    m_pending_sti_frame.version_data.uptime_s);
                        }

                        m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left,
                                m_pending_sti_frame.audio_levels.right);
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
        else if (not m_pending_sti_frame.timestamp.valid()) {
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
                    m_stats.notifyVersion(
                            m_pending_sti_frame.version_data.version,
                            m_pending_sti_frame.version_data.uptime_s);
                }

                m_stats.notifyPeakLevels(m_pending_sti_frame.audio_levels.left,
                        m_pending_sti_frame.audio_levels.right);
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
                ss << "Timestamped";
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

}
