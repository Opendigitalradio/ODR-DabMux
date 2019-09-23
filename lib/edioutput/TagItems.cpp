/*
   EDI output.
    This defines a few TAG items as defined ETSI TS 102 821 and
    ETSI TS 102 693

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

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
#include "TagItems.h"
#include <vector>
#include <iostream>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace edi {

TagStarPTR::TagStarPTR(const std::string& protocol)
    : m_protocol(protocol)
{
    if (m_protocol.size() != 4) {
        throw std::runtime_error("TagStarPTR protocol invalid length");
    }
}

std::vector<uint8_t> TagStarPTR::Assemble()
{
    //std::cerr << "TagItem *ptr" << std::endl;
    std::string pack_data("*ptr");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0x40);

    packet.insert(packet.end(), m_protocol.begin(), m_protocol.end());

    // Major
    packet.push_back(0);
    packet.push_back(0);

    // Minor
    packet.push_back(0);
    packet.push_back(0);
    return packet;
}

std::vector<uint8_t> TagDETI::Assemble()
{
    std::string pack_data("deti");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());
    packet.reserve(256);

    // Placeholder for length
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    uint8_t fct  = dlfc % 250;
    uint8_t fcth = dlfc / 250;


    uint16_t detiHeader = fct | (fcth << 8) | (rfudf << 13) | (ficf << 14) | (atstf << 15);
    packet.push_back(detiHeader >> 8);
    packet.push_back(detiHeader & 0xFF);

    uint32_t etiHeader = mnsc | (rfu << 16) | (rfa << 17) |
                        (fp << 19) | (mid << 22) | (stat << 24);
    packet.push_back((etiHeader >> 24) & 0xFF);
    packet.push_back((etiHeader >> 16) & 0xFF);
    packet.push_back((etiHeader >> 8) & 0xFF);
    packet.push_back(etiHeader & 0xFF);

    if (atstf) {
        packet.push_back(utco);

        packet.push_back((seconds >> 24) & 0xFF);
        packet.push_back((seconds >> 16) & 0xFF);
        packet.push_back((seconds >> 8) & 0xFF);
        packet.push_back(seconds & 0xFF);

        packet.push_back((tsta >> 16) & 0xFF);
        packet.push_back((tsta >> 8) & 0xFF);
        packet.push_back(tsta & 0xFF);
    }

    if (ficf) {
        for (size_t i = 0; i < fic_length; i++) {
            packet.push_back(fic_data[i]);
        }
    }

    if (rfudf) {
        packet.push_back((rfud >> 16) & 0xFF);
        packet.push_back((rfud >> 8) & 0xFF);
        packet.push_back(rfud & 0xFF);
    }

    // calculate and update size
    // remove TAG name and TAG length fields and convert to bits
    uint32_t taglength = (packet.size() - 8) * 8;

    // write length into packet
    packet[4] = (taglength >> 24) & 0xFF;
    packet[5] = (taglength >> 16) & 0xFF;
    packet[6] = (taglength >> 8) & 0xFF;
    packet[7] = taglength & 0xFF;

    dlfc = (dlfc+1) % 5000;

    /*
    std::cerr << "TagItem deti, packet.size " << packet.size() << std::endl;
    std::cerr << "              fic length " << fic_length << std::endl;
    std::cerr << "              length " << taglength / 8 << std::endl;
    */
    return packet;
}

void TagDETI::set_edi_time(const std::time_t t, int tai_utc_offset)
{
    utco = tai_utc_offset - 32;

    const std::time_t posix_timestamp_1_jan_2000 = 946684800;

    seconds = t - posix_timestamp_1_jan_2000 + utco;
}

std::vector<uint8_t> TagESTn::Assemble()
{
    std::string pack_data("est");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());
    packet.reserve(mst_length*8 + 16);

    packet.push_back(id);

    // Placeholder for length
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    if (tpl > 0x3F) {
        throw std::runtime_error("TagESTn: invalid TPL value");
    }

    if (sad > 0x3FF) {
        throw std::runtime_error("TagESTn: invalid SAD value");
    }

    if (scid > 0x3F) {
        throw std::runtime_error("TagESTn: invalid SCID value");
    }

    uint32_t sstc = (scid << 18) | (sad << 8) | (tpl << 2) | rfa;
    packet.push_back((sstc >> 16) & 0xFF);
    packet.push_back((sstc >> 8) & 0xFF);
    packet.push_back(sstc & 0xFF);

    for (size_t i = 0; i < mst_length * 8; i++) {
        packet.push_back(mst_data[i]);
    }

    // calculate and update size
    // remove TAG name and TAG length fields and convert to bits
    uint32_t taglength = (packet.size() - 8) * 8;

    // write length into packet
    packet[4] = (taglength >> 24) & 0xFF;
    packet[5] = (taglength >> 16) & 0xFF;
    packet[6] = (taglength >> 8) & 0xFF;
    packet[7] = taglength & 0xFF;

    /*
    std::cerr << "TagItem ESTn, length " << packet.size() << std::endl;
    std::cerr << "              mst_length " << mst_length << std::endl;
    */
    return packet;
}

std::vector<uint8_t> TagDSTI::Assemble()
{
    std::string pack_data("dsti");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());
    packet.reserve(256);

    // Placeholder for length
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    uint8_t dfctl = dflc % 250;
    uint8_t dfcth = dflc / 250;


    uint16_t dstiHeader = dfctl | (dfcth << 8) | (rfadf << 13) | (atstf << 14) | (stihf << 15);
    packet.push_back(dstiHeader >> 8);
    packet.push_back(dstiHeader & 0xFF);

    if (stihf) {
        packet.push_back(stat);
        packet.push_back((spid >> 8) & 0xFF);
        packet.push_back(spid & 0xFF);
    }

    if (atstf) {
        packet.push_back(utco);

        packet.push_back((seconds >> 24) & 0xFF);
        packet.push_back((seconds >> 16) & 0xFF);
        packet.push_back((seconds >> 8) & 0xFF);
        packet.push_back(seconds & 0xFF);

        packet.push_back((tsta >> 16) & 0xFF);
        packet.push_back((tsta >> 8) & 0xFF);
        packet.push_back(tsta & 0xFF);
    }

    if (rfadf) {
        for (size_t i = 0; i < rfad.size(); i++) {
            packet.push_back(rfad[i]);
        }
    }
    // calculate and update size
    // remove TAG name and TAG length fields and convert to bits
    uint32_t taglength = (packet.size() - 8) * 8;

    // write length into packet
    packet[4] = (taglength >> 24) & 0xFF;
    packet[5] = (taglength >> 16) & 0xFF;
    packet[6] = (taglength >> 8) & 0xFF;
    packet[7] = taglength & 0xFF;

    dflc = (dflc+1) % 5000;

    /*
    std::cerr << "TagItem dsti, packet.size " << packet.size() << std::endl;
    std::cerr << "              length " << taglength / 8 << std::endl;
    */
    return packet;
}

void TagDSTI::set_edi_time(const std::time_t t, int tai_utc_offset)
{
    utco = tai_utc_offset - 32;

    const std::time_t posix_timestamp_1_jan_2000 = 946684800;

    seconds = t - posix_timestamp_1_jan_2000 + utco;
}

#if 0
/* Update the EDI time. t is in UTC, TAI offset is requested from adjtimex */
void TagDSTI::set_edi_time(const std::time_t t)
{
    if (tai_offset_cache_updated_at == 0 or tai_offset_cache_updated_at + 3600 < t) {
        struct timex timex_request;
        timex_request.modes = 0;

        int err = adjtimex(&timex_request);
        if (err == -1) {
            throw std::runtime_error("adjtimex failed");
        }

        if (timex_request.tai == 0) {
            throw std::runtime_error("CLOCK_TAI is not properly set up");
        }
        tai_offset_cache = timex_request.tai;
        tai_offset_cache_updated_at = t;

        fprintf(stderr, "adjtimex: %d, tai %d\n", err, timex_request.tai);
    }

    utco = tai_offset_cache - 32;

    const std::time_t posix_timestamp_1_jan_2000 = 946684800;

    seconds = t - posix_timestamp_1_jan_2000 + utco;
}
#endif

std::vector<uint8_t> TagSSm::Assemble()
{
    std::string pack_data("ss");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());
    packet.reserve(istd_length + 16);

    packet.push_back((id >> 8) & 0xFF);
    packet.push_back(id & 0xFF);

    // Placeholder for length
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    if (rfa > 0x1F) {
        throw std::runtime_error("TagSSm: invalid RFA value");
    }

    if (tid > 0x7) {
        throw std::runtime_error("TagSSm: invalid tid value");
    }

    if (tidext > 0x7) {
        throw std::runtime_error("TagSSm: invalid tidext value");
    }

    if (stid > 0x0FFF) {
        throw std::runtime_error("TagSSm: invalid stid value");
    }

    uint32_t istc = (rfa << 19) | (tid << 16) | (tidext << 13) | ((crcstf ? 1 : 0) << 12) | stid;
    packet.push_back((istc >> 16) & 0xFF);
    packet.push_back((istc >> 8) & 0xFF);
    packet.push_back(istc & 0xFF);

    for (size_t i = 0; i < istd_length; i++) {
        packet.push_back(istd_data[i]);
    }

    // calculate and update size
    // remove TAG name and TAG length fields and convert to bits
    uint32_t taglength = (packet.size() - 8) * 8;

    // write length into packet
    packet[4] = (taglength >> 24) & 0xFF;
    packet[5] = (taglength >> 16) & 0xFF;
    packet[6] = (taglength >> 8) & 0xFF;
    packet[7] = taglength & 0xFF;

    /*
    std::cerr << "TagItem SSm, length " << packet.size() << std::endl;
    std::cerr << "             istd_length " << istd_length << std::endl;
    */
    return packet;
}


std::vector<uint8_t> TagStarDMY::Assemble()
{
    std::string pack_data("*dmy");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    packet.resize(4 + 4 + length_);

    const uint32_t length_bits = length_ * 8;

    packet[4] = (length_bits >> 24) & 0xFF;
    packet[5] = (length_bits >> 16) & 0xFF;
    packet[6] = (length_bits >> 8) & 0xFF;
    packet[7] = length_bits & 0xFF;

    // The remaining bytes in the packet are "undefined data"

    return packet;
}

TagODRVersion::TagODRVersion(const std::string& version, uint32_t uptime_s) :
    m_version(version),
    m_uptime(uptime_s)
{
}

std::vector<uint8_t> TagODRVersion::Assemble()
{
    std::string pack_data("ODRv");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    const size_t length = m_version.size() + sizeof(uint32_t);

    packet.resize(4 + 4 + length);

    const uint32_t length_bits = length * 8;

    size_t i = 4;
    packet[i++] = (length_bits >> 24) & 0xFF;
    packet[i++] = (length_bits >> 16) & 0xFF;
    packet[i++] = (length_bits >> 8) & 0xFF;
    packet[i++] = length_bits & 0xFF;

    copy(m_version.cbegin(), m_version.cend(), packet.begin() + i);
    i += m_version.size();

    packet[i++] = (m_uptime >> 24) & 0xFF;
    packet[i++] = (m_uptime >> 16) & 0xFF;
    packet[i++] = (m_uptime >> 8) & 0xFF;
    packet[i++] = m_uptime & 0xFF;

    return packet;
}

TagODRAudioLevels::TagODRAudioLevels(int16_t audiolevel_left, int16_t audiolevel_right) :
    m_audio_left(audiolevel_left),
    m_audio_right(audiolevel_right)
{
}

std::vector<uint8_t> TagODRAudioLevels::Assemble()
{
    std::string pack_data("ODRa");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());

    constexpr size_t length = 2*sizeof(int16_t);

    packet.resize(4 + 4 + length);

    const uint32_t length_bits = length * 8;

    size_t i = 4;
    packet[i++] = (length_bits >> 24) & 0xFF;
    packet[i++] = (length_bits >> 16) & 0xFF;
    packet[i++] = (length_bits >> 8) & 0xFF;
    packet[i++] = length_bits & 0xFF;

    packet[i++] = (m_audio_left >> 8) & 0xFF;
    packet[i++] = m_audio_left & 0xFF;

    packet[i++] = (m_audio_right >> 8) & 0xFF;
    packet[i++] = m_audio_right & 0xFF;

    return packet;
}

}

