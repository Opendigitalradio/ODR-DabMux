/*
   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output.
    This implements an AF Packet as defined ETSI TS 102 821.
    Also see ETSI TS 102 693

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
#include "crc.h"
#include "AFPacket.h"
#include "TagItems.h"
#include "TagPacket.h"
#include <vector>
#include <string>
#include <iostream>
#include <cstdio>
#include <stdint.h>
#include <arpa/inet.h>

namespace edi {

// Header PT field. AF packet contains TAG payload
const uint8_t AFHEADER_PT_TAG = 'T';

// AF Packet Major (3 bits) and Minor (4 bits) version
const uint8_t AFHEADER_VERSION = 0x10; // MAJ=1, MIN=0

AFPacket AFPacketiser::Assemble(TagPacket tag_packet)
{
    std::vector<uint8_t> payload = tag_packet.Assemble();

    if (m_verbose)
        std::cerr << "Assemble AFPacket " << seq << std::endl;

    std::string pack_data("AF"); // SYNC
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    uint32_t taglength = payload.size();

    if (m_verbose)
        std::cerr << "         AFPacket payload size " << payload.size() << std::endl;

    // write length into packet
    packet.push_back((taglength >> 24) & 0xFF);
    packet.push_back((taglength >> 16) & 0xFF);
    packet.push_back((taglength >> 8) & 0xFF);
    packet.push_back(taglength & 0xFF);

    // fill rest of header
    packet.push_back(seq >> 8);
    packet.push_back(seq & 0xFF);
    seq++;
    packet.push_back((have_crc ? 0x80 : 0) | AFHEADER_VERSION); // ar_cf: CRC=1
    packet.push_back(AFHEADER_PT_TAG);

    // insert payload, must have a length multiple of 8 bytes
    packet.insert(packet.end(), payload.begin(), payload.end());

    // calculate CRC over AF Header and payload
    uint16_t crc = 0xffff;
    crc = crc16(crc, &(packet.front()), packet.size());
    crc ^= 0xffff;

    if (m_verbose)
        fprintf(stderr, "         AFPacket crc %x\n", crc);

    packet.push_back((crc >> 8) & 0xFF);
    packet.push_back(crc & 0xFF);

    if (m_verbose)
        std::cerr << "         AFPacket length " << packet.size() << std::endl;

    return packet;
}

}
