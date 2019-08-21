/*
   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output.
    This implements an AF Packet as defined ETSI TS 102 821.
    Also see ETSI TS 102 693

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

#include "config.h"
#include <vector>
#include <cstdint>
#include "TagItems.h"
#include "TagPacket.h"

namespace edi {

typedef std::vector<uint8_t> AFPacket;

// ETSI TS 102 821, 6.1 AF packet structure
class AFPacketiser
{
    public:
        AFPacketiser() :
            m_verbose(false) {};
        AFPacketiser(bool verbose) :
            m_verbose(verbose) {};

        AFPacket Assemble(TagPacket tag_packet);

    private:
        static const bool have_crc = true;

        uint16_t seq = 0; //counter that overflows at 0xFFFF

        bool m_verbose;
};

}

