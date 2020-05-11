/*
   Copyright (C) 2020
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output.
    This defines a TAG Packet.
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

#include "config.h"
#include "TagPacket.h"
#include "TagItems.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>
#include <cstdint>
#include <cassert>

namespace edi {

TagPacket::TagPacket(unsigned int alignment) : m_alignment(alignment)
{ }

std::vector<uint8_t> TagPacket::Assemble()
{
    if (raw_tagpacket.size() > 0 and tag_items.size() > 0) {
        throw std::logic_error("TagPacket: both raw and items used!");
    }

    if (raw_tagpacket.size() > 0) {
        return raw_tagpacket;
    }

    std::vector<uint8_t> packet;

    for (auto tag : tag_items) {
        std::vector<uint8_t> tag_data = tag->Assemble();
        packet.insert(packet.end(), tag_data.begin(), tag_data.end());
    }

    if (m_alignment == 0) { /* no padding */ }
    else if (m_alignment == 8) {
        // Add padding inside TAG packet
        while (packet.size() % 8 > 0) {
            packet.push_back(0); // TS 102 821, 5.1, "padding shall be undefined"
        }
    }
    else if (m_alignment > 8) {
        TagStarDMY dmy(m_alignment - 8);
        auto dmy_data = dmy.Assemble();
        packet.insert(packet.end(), dmy_data.begin(), dmy_data.end());
    }
    else {
        std::cerr << "Invalid alignment requirement " << m_alignment <<
            " defined in TagPacket" << std::endl;
    }

    return packet;
}

}

