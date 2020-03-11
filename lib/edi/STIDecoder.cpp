/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

   http://opendigitalradio.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "STIDecoder.hpp"
#include "buffer_unpack.hpp"
#include "crc.h"
#include "Log.h"
#include <cstdio>
#include <cassert>
#include <sstream>

namespace EdiDecoder {

using namespace std;

STIDecoder::STIDecoder(STIDataCollector& data_collector, bool verbose) :
    m_data_collector(data_collector),
    m_dispatcher(std::bind(&STIDecoder::packet_completed, this), verbose)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    m_dispatcher.register_tag("*ptr",
            std::bind(&STIDecoder::decode_starptr, this, _1, _2));
    m_dispatcher.register_tag("dsti",
            std::bind(&STIDecoder::decode_dsti, this, _1, _2));
    m_dispatcher.register_tag("ss",
            std::bind(&STIDecoder::decode_ssn, this, _1, _2));
    m_dispatcher.register_tag("*dmy",
            std::bind(&STIDecoder::decode_stardmy, this, _1, _2));
    m_dispatcher.register_tag("ODRa",
            std::bind(&STIDecoder::decode_odraudiolevel, this, _1, _2));
    m_dispatcher.register_tag("ODRv",
            std::bind(&STIDecoder::decode_odrversion, this, _1, _2));
}

void STIDecoder::push_bytes(const vector<uint8_t> &buf)
{
    m_dispatcher.push_bytes(buf);
}

void STIDecoder::push_packet(const vector<uint8_t> &buf)
{
    m_dispatcher.push_packet(buf);
}

void STIDecoder::setMaxDelay(int num_af_packets)
{
    m_dispatcher.setMaxDelay(num_af_packets);
}

#define AFPACKET_HEADER_LEN 10 // includes SYNC

bool STIDecoder::decode_starptr(const vector<uint8_t> &value, uint16_t)
{
    if (value.size() != 0x40 / 8) {
        etiLog.log(warn, "Incorrect length %02lx for *PTR", value.size());
        return false;
    }

    char protocol_sz[5];
    protocol_sz[4] = '\0';
    copy(value.begin(), value.begin() + 4, protocol_sz);
    string protocol(protocol_sz);

    uint16_t major = read_16b(value.begin() + 4);
    uint16_t minor = read_16b(value.begin() + 6);

    m_data_collector.update_protocol(protocol, major, minor);

    return true;
}

bool STIDecoder::decode_dsti(const vector<uint8_t> &value, uint16_t)
{
    size_t offset = 0;

    const uint16_t dstiHeader = read_16b(value.begin() + offset);
    offset += 2;

    sti_management_data md;

    md.stihf = (dstiHeader >> 15) & 0x1;
    md.atstf = (dstiHeader >> 14) & 0x1;
    md.rfadf = (dstiHeader >> 13) & 0x1;
    uint8_t dfcth = (dstiHeader >> 8) & 0x1F;
    uint8_t dfctl = dstiHeader & 0xFF;

    md.dflc = dfcth * 250 + dfctl; // modulo 5000 counter

    const size_t expected_length = 2 +
        (md.stihf ? 3 : 0) +
        (md.atstf ? 1 + 4 + 3 : 0) +
        (md.rfadf ? 9 : 0);

    if (value.size() != expected_length) {
        throw std::runtime_error("EDI dsti: decoding error:"
                "value.size() != expected_length: " +
               to_string(value.size()) + " " +
               to_string(expected_length));
    }

    if (md.stihf) {
        const uint8_t stat = value[offset++];
        const uint16_t spid = read_16b(value.begin() + offset);
        m_data_collector.update_stat(stat, spid);
        offset += 2;
    }

    if (md.atstf) {
        uint8_t utco = value[offset];
        offset++;

        uint32_t seconds = read_32b(value.begin() + offset);
        offset += 4;

        m_data_collector.update_edi_time(utco, seconds);

        md.tsta = read_24b(value.begin() + offset);
        offset += 3;
    }
    else {
        // Null timestamp, ETSI ETS 300 799, C.2.2
        md.tsta = 0xFFFFFF;
    }


    if (md.rfadf) {
        std::array<uint8_t, 9> rfad;
        copy(value.cbegin() + offset,
             value.cbegin() + offset + 9,
             rfad.begin());
        offset += 9;

        m_data_collector.update_rfad(rfad);
    }

    m_data_collector.update_sti_management(md);

    return true;
}

bool STIDecoder::decode_ssn(const vector<uint8_t> &value, uint16_t n)
{
    sti_payload_data sti;

    sti.stream_index = n - 1; // n is 1-indexed
    sti.rfa = value[0] >> 3;
    sti.tid = value[0] & 0x07;

    uint16_t istc = read_24b(value.begin() + 1);
    sti.tidext = istc >> 13;
    sti.crcstf = (istc >> 12) & 0x1;
    sti.stid = istc & 0xFFF;

    if (sti.rfa != 0) {
        etiLog.level(warn) << "EDI: rfa field in SSnn tag non-null";
    }

    copy(   value.cbegin() + 3,
            value.cend(),
            back_inserter(sti.istd));

    m_data_collector.add_payload(move(sti));

    return true;
}

bool STIDecoder::decode_stardmy(const vector<uint8_t>& /*value*/, uint16_t)
{
    return true;
}

bool STIDecoder::decode_odraudiolevel(const vector<uint8_t>& value, uint16_t)
{
    constexpr size_t expected_length = 2 * sizeof(int16_t);

    audio_level_data audio_level;

    if (value.size() == expected_length) {
        audio_level.left = read_16b(value.begin());
        audio_level.right = read_16b(value.begin() + 2);
    }
    else {
        audio_level.left = 0;
        audio_level.right = 0;
        etiLog.level(warn) << "EDI: ODR AudioLevel TAG has wrong length!";
    }

    m_data_collector.update_audio_levels(audio_level);

    // Not being able to decode the audio level is a soft-failure, it should
    // not disrupt decoding the actual audio data.
    return true;
}

bool STIDecoder::decode_odrversion(const vector<uint8_t>& value, uint16_t)
{
    const auto vd = parse_odr_version_data(value);
    m_data_collector.update_odr_version(vd);

    return true;
}

void STIDecoder::packet_completed()
{
    m_data_collector.assemble();
}

}
