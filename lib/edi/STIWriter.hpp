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
#pragma once

#include "common.hpp"
#include "STIDecoder.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <list>

namespace EdiDecoder {

struct sti_frame_t {
    std::vector<uint8_t> frame;
    uint16_t dlfc;
    frame_timestamp_t timestamp;
    audio_level_data audio_levels;
    odr_version_data version_data;
    seq_info_t sequence_counters;
};

class STIWriter : public STIDataCollector {
    public:
        // The callback gets called for every STI frame that gets assembled
        STIWriter(std::function<void(sti_frame_t&&)>&& frame_callback);

        // Tell the ETIWriter what EDI protocol we receive in *ptr.
        // This is not part of the ETI data, but is used as check
        virtual void update_protocol(
                const std::string& proto,
                uint16_t major,
                uint16_t minor) override;

        virtual void update_stat(uint8_t stat, uint16_t spid) override;

        virtual void update_edi_time(
                uint32_t utco,
                uint32_t seconds) override;

        virtual void update_rfad(std::array<uint8_t, 9> rfad) override;
        virtual void update_sti_management(const sti_management_data& data) override;
        virtual void add_payload(sti_payload_data&& payload) override;

        virtual void update_audio_levels(const audio_level_data& data) override;
        virtual void update_odr_version(const odr_version_data& data) override;

        virtual void assemble(seq_info_t seq) override;
    private:
        std::function<void(sti_frame_t&&)> m_frame_callback;

        bool m_proto_valid = false;

        bool m_management_data_valid = false;
        sti_management_data m_management_data;

        bool m_stat_valid = false;
        uint8_t m_stat = 0;
        uint16_t m_spid = 0;

        bool m_time_valid = false;
        uint32_t m_utco = 0;
        uint32_t m_seconds = 0;

        bool m_payload_valid = false;
        sti_payload_data m_payload;

        audio_level_data m_audio_levels;
        odr_version_data m_version_data;
};

}

