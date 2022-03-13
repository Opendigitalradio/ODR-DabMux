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
#include "STIWriter.hpp"
#include "crc.h"
#include "Log.h"
#include <cstdio>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace EdiDecoder {

using namespace std;

STIWriter::STIWriter(std::function<void(sti_frame_t&&)>&& frame_callback) :
    m_frame_callback(move(frame_callback))
{
}

void STIWriter::update_protocol(
        const std::string& proto,
        uint16_t major,
        uint16_t minor)
{
    m_proto_valid = (proto == "DSTI" and major == 0 and minor == 0);

    if (not m_proto_valid) {
        throw std::invalid_argument("Wrong EDI protocol");
    }
}


void STIWriter::update_stat(uint8_t stat, uint16_t spid)
{
    m_stat = stat;
    m_spid = spid;
    m_stat_valid = true;

    if (m_stat != 0xFF) {
        etiLog.log(warn, "STI errorlevel %02x", m_stat);
    }
}

void STIWriter::update_rfad(std::array<uint8_t, 9> rfad)
{
    (void)rfad;
}

void STIWriter::update_sti_management(const sti_management_data& data)
{
    m_management_data = data;
    m_management_data_valid = true;
}

void STIWriter::add_payload(sti_payload_data&& payload)
{
    m_payload = move(payload);
    m_payload_valid = true;
}

void STIWriter::update_audio_levels(const audio_level_data& data)
{
    m_audio_levels = data;
}

void STIWriter::update_odr_version(const odr_version_data& data)
{
    m_version_data = data;
}

void STIWriter::update_edi_time(
        uint32_t utco,
        uint32_t seconds)
{
    if (not m_proto_valid) {
        throw std::runtime_error("Cannot update time before protocol");
    }

    m_utco = utco;
    m_seconds = seconds;

    // TODO check validity
    m_time_valid = true;
}


void STIWriter::assemble(seq_info_t seq)
{
    if (not m_proto_valid) {
        throw std::runtime_error("Cannot assemble STI before protocol");
    }

    if (not m_management_data_valid) {
        throw std::runtime_error("Cannot assemble STI before management data");
    }

    if (not m_payload_valid) {
        throw std::runtime_error("Cannot assemble STI without frame data");
    }

    // TODO check time validity

    sti_frame_t stiFrame;
    stiFrame.dlfc = m_management_data.dlfc;
    stiFrame.frame = move(m_payload.istd);
    stiFrame.timestamp.seconds = m_seconds;
    stiFrame.timestamp.utco = m_utco;
    stiFrame.timestamp.tsta = m_management_data.tsta;
    stiFrame.audio_levels = m_audio_levels;
    stiFrame.version_data = m_version_data;
    stiFrame.sequence_counters = seq;

    m_frame_callback(move(stiFrame));

    m_proto_valid = false;
    m_management_data_valid = false;
    m_stat_valid = false;
    m_time_valid = false;
    m_payload_valid = false;
}

}
