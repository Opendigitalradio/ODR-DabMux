/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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
#include <boost/thread.hpp>
#include "input/inputs.h"
#include "Socket.h"

namespace Inputs {

/* A Udp input that takes incoming datagrams, concatenates them
 * together and gives them back.
 */
class Udp : public InputBase {
    public:
        virtual int open(const std::string& name);
        virtual int readFrame(uint8_t* buffer, size_t size);
        virtual int setBitrate(int bitrate);
        virtual int close();

    protected:
        Socket::UDPSocket m_sock;
        std::string m_name;

        void openUdpSocket(const std::string& endpoint);

    private:
        // The content of the UDP packets gets written into the
        // buffer, and the UDP packet boundaries disappear there.
        std::vector<uint8_t> m_buffer;
};

/* An input for STI-D(LI) carried in STI(PI, X) inside RTP inside UDP.
 * Reorders incoming datagrams which must contain an RTP header and valid
 * STI-D data.
 *
 * This is intended to be compatible with encoders from AVT.
 */
class Sti_d_Rtp : public Udp {
    using vec_u8 = std::vector<uint8_t>;

    public:
        virtual int open(const std::string& name);
        virtual int readFrame(uint8_t* buffer, size_t size);

    private:
        void receive_packet(void);
        std::deque<vec_u8> m_queue;
};

};

