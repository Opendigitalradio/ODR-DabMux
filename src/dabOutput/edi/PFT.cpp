/*
   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   EDI output,
   Protection, Fragmentation and Transport. (PFT)

   Are supported:
    Reed-Solomon and Fragmentation

   This implements part of PFT as defined ETSI TS 102 821.

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
#include <vector>
#include <list>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <arpa/inet.h>
#include "PFT.h"
#include "crc.h"
#include "ReedSolomon.h"

using namespace std;

typedef vector<uint8_t> Chunk;


RSPacket PFT::Protect(AFPacket af_packet)
{
    RSPacket rs_packet;

    // number of chunks is ceil(afpacketsize / m_k)
    if (af_packet.size() % m_k == 0) {
        m_num_chunks = af_packet.size() / m_k;
    }
    else {
        m_num_chunks = af_packet.size() / m_k + 1;
    }

    const size_t zero_pad = m_num_chunks * m_k - af_packet.size();

    // add zero padding to last chunk
    for (size_t i = 0; i < zero_pad; i++) {
        af_packet.push_back(0);
    }

    for (size_t i = 1; i < af_packet.size(); i+= m_k) {
        Chunk c(m_k + ParityBytes);

        // copy m_k bytes into new chunk
        memcpy(&c.front(), &af_packet[i], m_k);

        // calculate RS for chunk
        m_encoder.encode(&c.front(), c.size());

        // append new chunk to the RS Packet
        rs_packet.insert(rs_packet.end(), c.begin(), c.end());
    }

    return rs_packet;
}

vector< vector<uint8_t> > PFT::ProtectAndFragment(AFPacket af_packet)
{
    RSPacket rs_packet = Protect(af_packet);

    const size_t max_payload_size = ( m_num_chunks * ParityBytes ) / (m_m + 1);

    const size_t fragment_size = m_num_chunks * (m_k + ParityBytes) / max_payload_size;
    const size_t num_fragments = m_num_chunks * (m_k + ParityBytes) / fragment_size;

    vector< vector<uint8_t> > fragments(num_fragments);

    for (size_t i = 0; i < num_fragments; i++) {
        fragments[i].resize(fragment_size);
        for (size_t j = 0; j < fragment_size; j++) {
            fragments[i][j] = rs_packet[j*num_fragments + i];
        }
    }

    return fragments;
}

std::vector< PFTFragment > PFT::Assemble(AFPacket af_packet)
{
    vector< vector<uint8_t> > fragments = ProtectAndFragment(af_packet);
    vector< vector<uint8_t> > pft_fragments;

    unsigned int findex = 0;

    unsigned fcount = fragments.size();

    const size_t zero_pad = m_num_chunks * m_k - af_packet.size();

    for (size_t i = 0; i < fragments.size(); i++) {
        const vector<uint8_t>& fragment = fragments[i];

        std::string psync("PF"); // SYNC
        std::vector<uint8_t> packet(psync.begin(), psync.end());

        packet.push_back(m_pseq >> 8);
        packet.push_back(m_pseq & 0xFF);
        m_pseq++;

        packet.push_back(findex >> 16);
        packet.push_back(findex >> 8);
        packet.push_back(findex & 0xFF);
        findex++;

        packet.push_back(fcount >> 16);
        packet.push_back(fcount >> 8);
        packet.push_back(fcount & 0xFF);

        unsigned int plen = fragment.size();
        plen |= 0x8000; // Set FEC bit

        packet.push_back(plen >> 16);
        packet.push_back(plen >> 8);
        packet.push_back(plen & 0xFF);

        packet.push_back(m_k);
        packet.push_back(zero_pad);

        // calculate CRC over AF Header and payload
        uint16_t crc = 0xffff;
        crc = crc16(crc, &(packet.back()), packet.size());
        crc ^= 0xffff;
        crc = htons(crc);

        packet.push_back((crc >> 24) & 0xFF);
        packet.push_back((crc >> 16) & 0xFF);
        packet.push_back((crc >> 8) & 0xFF);
        packet.push_back(crc & 0xFF);

        // insert payload, must have a length multiple of 8 bytes
        packet.insert(packet.end(), fragment.begin(), fragment.end());

        pft_fragments.push_back(packet);
    }

    return pft_fragments;
}

