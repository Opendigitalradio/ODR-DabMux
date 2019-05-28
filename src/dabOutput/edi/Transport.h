/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   UDP and TCP transports and their configuration

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

#include "config.h"
#include "dabOutput/edi/Config.h"
#include "AFPacket.h"
#include "PFT.h"
#include "Interleaver.h"
#include "Socket.h"
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>

namespace edi {

/** Configuration for EDI output */

class Sender {
    public:
        Sender(const configuration_t& conf);

        void write(const TagPacket& tagpacket);

    private:
        configuration_t m_conf;
        std::ofstream edi_debug_file;

        // The TagPacket will then be placed into an AFPacket
        edi::AFPacketiser edi_afPacketiser;

        // The AF Packet will be protected with reed-solomon and split in fragments
        edi::PFT edi_pft;

        // To mitigate for burst packet loss, PFT fragments can be sent out-of-order
        edi::Interleaver edi_interleaver;

        std::unordered_map<udp_destination_t*, std::shared_ptr<Socket::UDPSocket>> udp_sockets;
        std::unordered_map<tcp_destination_t*, std::shared_ptr<Socket::TCPDataDispatcher>> tcp_dispatchers;
};

}

