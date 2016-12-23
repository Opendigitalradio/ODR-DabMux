/*
   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output.
    This defines a TAG Packet.
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

#include "config.h"
#include "Eti.h"
#include "TagPacket.h"
#include "TagItems.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>
#include <stdint.h>
#include <cassert>

namespace edi {

TagPacket::TagPacket(unsigned int alignment) : m_alignment(alignment)
{ }

std::vector<uint8_t> TagPacket::Assemble()
{
    std::list<TagItem*>::iterator tag;

    std::vector<uint8_t> packet;

    //std::cerr << "Assemble TAGPacket" << std::endl;

    for (tag = tag_items.begin(); tag != tag_items.end(); ++tag) {
        std::vector<uint8_t> tag_data = (*tag)->Assemble();
        packet.insert(packet.end(), tag_data.begin(), tag_data.end());

        //std::cerr << "     Add TAGItem of length " << tag_data.size() << std::endl;
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

