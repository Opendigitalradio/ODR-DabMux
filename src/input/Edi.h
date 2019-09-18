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

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include "Socket.h"
#include "input/inputs.h"
#include "edi/STIDecoder.hpp"
#include "edi/STIWriter.hpp"
#include "ThreadsafeQueue.h"
#include "ManagementServer.h"

namespace Inputs {

/*
 * Receives EDI from UDP or TCP in a separate thread and pushes that data
 * into the STIDecoder. Complete frames are then put into a queue for the consumer.
 *
 * This way, the EDI decoding happens in a separate thread.
 */
class Edi : public InputBase {
    public:
        Edi(const std::string& name);
        Edi(const Edi&) = delete;
        Edi& operator=(const Edi&) = delete;
        ~Edi();

        virtual void open(const std::string& name);
        virtual size_t readFrame(uint8_t *buffer, size_t size);
        virtual size_t readFrame(uint8_t *buffer, size_t size, std::time_t seconds, int utco, uint32_t tsta);
        virtual int setBitrate(int bitrate);
        virtual void close();

    protected:
        void m_run();

        std::mutex m_mutex;

        enum class InputUsed { Invalid, UDP, TCP };
        InputUsed m_input_used = InputUsed::Invalid;
        Socket::UDPSocket m_udp_sock;
        Socket::TCPReceiveServer m_tcp_receive_server;

        EdiDecoder::STIWriter m_sti_writer;
        EdiDecoder::STIDecoder m_sti_decoder;
        std::thread m_thread;
        std::atomic<bool> m_running = ATOMIC_VAR_INIT(false);
        ThreadsafeQueue<EdiDecoder::sti_frame_t> m_frames;

        // Used in timestamp-based buffer management
        EdiDecoder::sti_frame_t m_pending_sti_frame;

        // Used in prebuffering-based buffer management
        bool m_is_prebuffering = true;

        std::string m_name;
        InputStat m_stats;
};

};

