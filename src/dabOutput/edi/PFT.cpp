/*
   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   Protection, Fragmentation and Transport. (PFT)

   Are supported:
    Reed-Solomon and Fragmentation

   This implements part of PFT as defined ETSI TS 102 821.

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
#include <vector>
#include <list>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <sstream>
#include "PFT.h"
#include "crc.h"
#include "ReedSolomon.h"

namespace edi {

using namespace std;

// An integer division that rounds up, i.e. ceil(a/b)
#define CEIL_DIV(a, b) (a % b == 0  ? a / b : a / b + 1)

RSBlock PFT::Protect(AFPacket af_packet)
{
    RSBlock rs_block;

    // number of chunks is ceil(afpacketsize / m_k)
    // TS 102 821 7.2.2: c = ceil(l / k_max)
    m_num_chunks = CEIL_DIV(af_packet.size(), m_k);

    if (m_verbose) {
        fprintf(stderr, "Protect %zu chunks of size %zu\n",
                m_num_chunks, af_packet.size());
    }

    // calculate size of chunk:
    // TS 102 821 7.2.2: k = ceil(l / c)
    // chunk_len does not include the 48 bytes of protection.
    const size_t chunk_len = CEIL_DIV(af_packet.size(), m_num_chunks);
    if (chunk_len > 207) {
        std::stringstream ss;
        ss << "Chunk length " << chunk_len << " too large (>207)";
        throw std::runtime_error(ss.str());
    }

    // The last RS chunk is zero padded
    // TS 102 821 7.2.2: z = c*k - l
    const size_t zero_pad = m_num_chunks * chunk_len - af_packet.size();

    // Create the RS(k+p,k) encoder
    const int firstRoot = 1; // Discovered by analysing EDI dump
    const int gfPoly = 0x11d;
    const bool reverse = false;
    // The encoding has to be 255, 207 always, because the chunk has to
    // be padded at the end, and not at the beginning as libfec would
    // do
    ReedSolomon rs_encoder(255, 207, reverse, gfPoly, firstRoot);

    // add zero padding to last chunk
    for (size_t i = 0; i < zero_pad; i++) {
        af_packet.push_back(0);
    }

    if (m_verbose) {
        fprintf(stderr, "        add %zu zero padding\n", zero_pad);
    }

    // Calculate RS for each chunk and assemble RS block
    for (size_t i = 0; i < af_packet.size(); i+= chunk_len) {
        vector<uint8_t> chunk(207);
        vector<uint8_t> protection(ParityBytes);

        // copy chunk_len bytes into new chunk
        memcpy(&chunk.front(), &af_packet[i], chunk_len);

        // calculate RS for chunk with padding
        rs_encoder.encode(&chunk.front(), &protection.front(), 207);

        // Drop the padding
        chunk.resize(chunk_len);

        // append new chunk and protection to the RS Packet
        rs_block.insert(rs_block.end(), chunk.begin(), chunk.end());
        rs_block.insert(rs_block.end(), protection.begin(), protection.end());
    }

    return rs_block;
}

vector< vector<uint8_t> > PFT::ProtectAndFragment(AFPacket af_packet)
{
    const bool enable_RS = (m_m > 0);

    if (enable_RS) {
        RSBlock rs_block = Protect(af_packet);

#if 0
        fprintf(stderr, "  af_packet (%zu):", af_packet.size());
        for (size_t i = 0; i < af_packet.size(); i++) {
            fprintf(stderr, "%02x ", af_packet[i]);
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "  rs_block (%zu):", rs_block.size());
        for (size_t i = 0; i < rs_block.size(); i++) {
            fprintf(stderr, "%02x ", rs_block[i]);
        }
        fprintf(stderr, "\n");
#endif

        // TS 102 821 7.2.2: s_max = MIN(floor(c*p/(m+1)), MTU - h))
        const size_t max_payload_size = ( m_num_chunks * ParityBytes ) / (m_m + 1);

        // Calculate fragment count and size
        // TS 102 821 7.2.2: ceil((l + c*p + z) / s_max)
        // l + c*p + z = length of RS block
        const size_t num_fragments = CEIL_DIV(rs_block.size(), max_payload_size);

        // TS 102 821 7.2.2: ceil((l + c*p + z) / f)
        const size_t fragment_size = CEIL_DIV(rs_block.size(), num_fragments);

        if (m_verbose)
            fprintf(stderr, "  PnF fragment_size %zu, num frag %zu\n",
                    fragment_size, num_fragments);

        vector< vector<uint8_t> > fragments(num_fragments);

        for (size_t i = 0; i < num_fragments; i++) {
            fragments[i].resize(fragment_size);
            for (size_t j = 0; j < fragment_size; j++) {
                const size_t ix = j*num_fragments + i;
                if (ix < rs_block.size()) {
                    fragments[i][j] = rs_block[ix];
                }
                else {
                    fragments[i][j] = 0;
                }
            }
        }

        return fragments;
    }
    else { // No RS, only fragmentation
        // TS 102 821 7.2.2: s_max = MTU - h
        // Ethernet MTU is 1500, but maybe you are routing over a network which
        // has some sort of packet encapsulation. Add some margin.
        const size_t max_payload_size = 1400;

        // Calculate fragment count and size
        // TS 102 821 7.2.2: ceil((l + c*p + z) / s_max)
        // l + c*p + z = length of AF packet
        const size_t num_fragments = CEIL_DIV(af_packet.size(), max_payload_size);

        // TS 102 821 7.2.2: ceil((l + c*p + z) / f)
        const size_t fragment_size = CEIL_DIV(af_packet.size(), num_fragments);
        vector< vector<uint8_t> > fragments(num_fragments);

        for (size_t i = 0; i < num_fragments; i++) {
            fragments[i].reserve(fragment_size);

            for (size_t j = 0; j < fragment_size; j++) {
                const size_t ix = i*fragment_size + j;
                if (ix < af_packet.size()) {
                    fragments[i].push_back(af_packet.at(ix));
                }
                else {
                    break;
                }
            }
        }

        return fragments;
    }
}

std::vector< PFTFragment > PFT::Assemble(AFPacket af_packet)
{
    vector< vector<uint8_t> > fragments = ProtectAndFragment(af_packet);
    vector< vector<uint8_t> > pft_fragments; // These contain PF headers

    const bool enable_RS = (m_m > 0);
    const bool enable_transport = true;

    unsigned int findex = 0;

    unsigned fcount = fragments.size();

    // calculate size of chunk:
    // TS 102 821 7.2.2: k = ceil(l / c)
    // chunk_len does not include the 48 bytes of protection.
    const size_t chunk_len = enable_RS ?
        CEIL_DIV(af_packet.size(), m_num_chunks) : 0;

    // The last RS chunk is zero padded
    // TS 102 821 7.2.2: z = c*k - l
    const size_t zero_pad = enable_RS ?
        m_num_chunks * chunk_len - af_packet.size() : 0;

    for (const auto &fragment : fragments) {
        // Psync
        std::string psync("PF");
        std::vector<uint8_t> packet(psync.begin(), psync.end());

        // Pseq
        packet.push_back(m_pseq >> 8);
        packet.push_back(m_pseq & 0xFF);

        // Findex
        packet.push_back(findex >> 16);
        packet.push_back(findex >> 8);
        packet.push_back(findex & 0xFF);
        findex++;

        // Fcount
        packet.push_back(fcount >> 16);
        packet.push_back(fcount >> 8);
        packet.push_back(fcount & 0xFF);

        // RS (1 bit), transport (1 bit) and Plen (14 bits)
        unsigned int plen = fragment.size();
        if (enable_RS) {
            plen |= 0x8000; // Set FEC bit
        }

        if (enable_transport) {
            plen |= 0x4000; // Set ADDR bit
        }

        packet.push_back(plen >> 8);
        packet.push_back(plen & 0xFF);

        if (enable_RS) {
            packet.push_back(chunk_len);   // RSk
            packet.push_back(zero_pad);    // RSz
        }

        if (enable_transport) {
            // Source (16 bits)
            uint16_t addr_source = 0;
            packet.push_back(addr_source >> 8);
            packet.push_back(addr_source & 0xFF);

            // Dest (16 bits)
            packet.push_back(m_dest_port >> 8);
            packet.push_back(m_dest_port & 0xFF);
        }

        // calculate CRC over AF Header and payload
        uint16_t crc = 0xffff;
        crc = crc16(crc, &(packet.front()), packet.size());
        crc ^= 0xffff;

        packet.push_back((crc >> 8) & 0xFF);
        packet.push_back(crc & 0xFF);

        // insert payload, must have a length multiple of 8 bytes
        packet.insert(packet.end(), fragment.begin(), fragment.end());

        pft_fragments.push_back(packet);

#if 0
        fprintf(stderr, "* PFT pseq %d, findex %d, fcount %d, plen %d\n",
                m_pseq, findex, fcount, plen & ~0x8000);
#endif
    }

    m_pseq++;

    return pft_fragments;
}

}

