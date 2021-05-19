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

#pragma once

#include "TagItems.h"
#include <vector>
#include <string>
#include <list>
#include <cstdint>

namespace edi {

// A TagPacket is nothing else than a list of tag items, with an
// Assemble function that puts the bytestream together and adds
// padding such that the total length is a multiple of 8 Bytes.
//
// Alternatively, a raw tagpacket can be used instead of the
// items list
//
// ETSI TS 102 821, 5.1 Tag Packet
class TagPacket
{
    public:
        TagPacket(unsigned int alignment);
        std::vector<uint8_t> Assemble();

        std::list<TagItem*> tag_items;

        std::vector<uint8_t> raw_tagpacket;

    private:
        unsigned int m_alignment;
};

}

