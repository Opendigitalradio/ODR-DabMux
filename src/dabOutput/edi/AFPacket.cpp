/*
   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   EDI output.
    This implements an AF Packet as defined ETSI TS 102 821.
    Also see ETSI TS 102 693

   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */
#include "config.h"
#include "crc.h"
#include "AFPacket.h"
#include "TagItems.h"
#include "TagPacket.h"
#include <vector>
#include <string>
#include <stdint.h>
#include <arpa/inet.h>

// Header PT field. AF packet contains TAG payload
#define AFHEADER_PT_TAG 'T'

// AF Packet Major (3 bits) and Minor (4 bits) version
#define AFHEADER_VERSION 0x8 // MAJ=1, MIN=0

std::vector<uint8_t> AFPacket::Assemble(TagPacket tag_packet)
{
    std::vector<uint8_t> payload = tag_packet.Assemble();

    std::string pack_data("AF"); // SYNC
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    uint32_t taglength = payload.size();

    // write length into packet
    packet[2] = (taglength >> 24) & 0xFF;
    packet[3] = (taglength >> 16) & 0xFF;
    packet[4] = (taglength >> 8) & 0xFF;
    packet[5] = taglength & 0xFF;

    // fill rest of header
    packet.push_back(seq >> 8);
    packet.push_back(seq & 0xFF);
    packet.push_back((have_crc ? 0x80 : 0) | AFHEADER_VERSION); // ar_cf: CRC=1
    packet.push_back(AFHEADER_PT_TAG);

    // insert payload, must have a length multiple of 8 bytes
    packet.insert(packet.end(), payload.begin(), payload.end());

    // calculate CRC over AF Header and payload
    uint16_t crc = 0xffff;
    crc = crc16(crc, &(packet.back()), packet.size());
    crc ^= 0xffff;
    crc = htons(crc);

    packet.push_back((crc >> 24) & 0xFF);
    packet.push_back((crc >> 16) & 0xFF);
    packet.push_back((crc >> 8) & 0xFF);
    packet.push_back(crc & 0xFF);

    return packet;
}

