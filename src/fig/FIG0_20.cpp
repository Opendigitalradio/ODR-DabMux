/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2024
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#include "fig/FIG0structs.h"
#include "fig/FIG0_20.h"
#include "utils.h"

namespace FIC {

// FIG 0/20 header structure for programme services (16-bit SId)
struct FIGtype0_20_programme_header {
    uint8_t SId_high;
    uint8_t SId_low;
    uint8_t sc_flag:1;
    uint8_t pt_flag:1;
    uint8_t change_flags:2;
    uint8_t SCIdS:4;
} PACKED;

// FIG 0/20 header structure for data services (32-bit SId)
struct FIGtype0_20_data_header {
    uint8_t SId_byte0;
    uint8_t SId_byte1;
    uint8_t SId_byte2;
    uint8_t SId_byte3;
    uint8_t sc_flag:1;
    uint8_t pt_flag:1;
    uint8_t change_flags:2;
    uint8_t SCIdS:4;
} PACKED;

// SC description structure (1 byte, only present if SC flag = 1)
struct FIGtype0_20_sc_desc {
    uint8_t SCTy:6;
    uint8_t ad_flag:1;
    uint8_t ca_flag:1;
} PACKED;

FIG0_20::FIG0_20(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{ }

FillStatus FIG0_20::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;

    auto ensemble = m_rti->ensemble;

    if (ensemble->sci_entries.empty()) {
        fs.complete_fig_transmitted = true;
        fs.num_bytes_written = 0;
        return fs;
    }

    if (not m_initialised) {
        sci_it = ensemble->sci_entries.end();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; sci_it != ensemble->sci_entries.end(); ++sci_it) {
        auto& sci = *sci_it;

        if (!sci->is_active()) {
            continue;
        }

        // Calculate required size for this SCI entry
        size_t required_size = 0;

        if (sci->isProgramme) {
            required_size += 3;  // 2 bytes SId + 1 byte flags
        } else {
            required_size += 5;  // 4 bytes SId + 1 byte flags
        }

        if (sci->sc_flag) {
            required_size += 1;
        }

        // Date-time field: 3 bytes
        required_size += 3;

        // Transfer SId
        if (sci->sid_flag) {
            required_size += sci->isProgramme ? 2 : 4;
        }

        // Transfer EId
        if (sci->eid_flag) {
            required_size += 2;
        }

        if (fig0 == NULL) {
            if (remaining < 2 + (ssize_t)required_size) {
                break;
            }

            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN = 0;
            fig0->OE = 0;
            fig0->PD = sci->isProgramme ? 0 : 1;
            fig0->Extension = 20;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < (ssize_t)required_size) {
            break;
        }

        uint8_t* entry_start = buf;

        if (sci->isProgramme) {
            auto header = (FIGtype0_20_programme_header*)buf;
            header->SId_high = (sci->SId >> 8) & 0xFF;
            header->SId_low = sci->SId & 0xFF;
            header->SCIdS = sci->SCIdS & 0x0F;
            header->change_flags = static_cast<uint8_t>(sci->change_flags) & 0x03;
            header->pt_flag = sci->part_time ? 1 : 0;
            header->sc_flag = sci->sc_flag ? 1 : 0;
            buf += 3;
        }
        else {
            auto header = (FIGtype0_20_data_header*)buf;
            header->SId_byte0 = (sci->SId >> 24) & 0xFF;
            header->SId_byte1 = (sci->SId >> 16) & 0xFF;
            header->SId_byte2 = (sci->SId >> 8) & 0xFF;
            header->SId_byte3 = sci->SId & 0xFF;
            header->SCIdS = sci->SCIdS & 0x0F;
            header->change_flags = static_cast<uint8_t>(sci->change_flags) & 0x03;
            header->pt_flag = sci->part_time ? 1 : 0;
            header->sc_flag = sci->sc_flag ? 1 : 0;
            buf += 5;
        }

        if (sci->sc_flag) {
            auto sc_desc = (FIGtype0_20_sc_desc*)buf;
            sc_desc->ca_flag = sci->ca_flag ? 1 : 0;
            sc_desc->ad_flag = sci->ad_flag ? 1 : 0;
            sc_desc->SCTy = sci->SCTy & 0x3F;
            buf += 1;
        }

        // Date-time field (3 bytes)
        // Byte 0: date[4:0] + hour[4:2]
        // Byte 1: hour[1:0] + minute[5:0]
        // Byte 2: second[5:0] + sid_flag + eid_flag
        uint8_t date_val = sci->date & 0x1F;
        uint8_t hour_val = sci->hour & 0x1F;
        uint8_t minute_val = sci->minute & 0x3F;
        uint8_t second_val = sci->second & 0x3F;

        buf[0] = (date_val << 3) | (hour_val >> 2);
        buf[1] = ((hour_val & 0x03) << 6) | minute_val;
        buf[2] = (second_val << 2) | ((sci->sid_flag ? 1 : 0) << 1) | (sci->eid_flag ? 1 : 0);
        buf += 3;

        if (sci->sid_flag) {
            if (sci->isProgramme) {
                buf[0] = (sci->transfer_sid >> 8) & 0xFF;
                buf[1] = sci->transfer_sid & 0xFF;
                buf += 2;
            }
            else {
                buf[0] = (sci->transfer_sid >> 24) & 0xFF;
                buf[1] = (sci->transfer_sid >> 16) & 0xFF;
                buf[2] = (sci->transfer_sid >> 8) & 0xFF;
                buf[3] = sci->transfer_sid & 0xFF;
                buf += 4;
            }
        }

        if (sci->eid_flag) {
            buf[0] = (sci->transfer_eid >> 8) & 0xFF;
            buf[1] = sci->transfer_eid & 0xFF;
            buf += 2;
        }

        size_t entry_size = buf - entry_start;
        fig0->Length += entry_size;
        remaining -= entry_size;
    }

    if (sci_it == ensemble->sci_entries.end()) {
        sci_it = ensemble->sci_entries.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}
