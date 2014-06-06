/*
   Copyright (C) 2013,2014 Matthias P. Braendli
   http://mpb.li

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
#include <string>
#include <list>
#include <stdint.h>


std::vector<uint8_t> TagPacket::Assemble()
{
    std::list<TagItem*>::iterator tag;

    std::vector<uint8_t> packet;

    size_t packet_length = 0;
    for (tag = tag_items.begin(); tag != tag_items.end(); ++tag) {
        std::vector<uint8_t> tag_data = (*tag)->Assemble();
        packet.insert(packet.end(), tag_data.begin(), tag_data.end());

        packet_length += tag_data.size();
    }

    // Add padding
    while (packet_length % 8 > 0)
    {
        packet.push_back(0); // TS 102 821, 5.1, "padding shall be undefined"
        packet_length++;
    }

    return packet;
}

