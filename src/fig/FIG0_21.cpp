/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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
#include "fig/FIG0_21.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_21_header {
    // This was RegionId in EN 300 401 V1.4.1
    uint8_t rfaHigh;

    uint8_t length_fi:5;
    uint8_t rfaLow:3;
} PACKED;

struct FIGtype0_21_fi_list_header {
    uint8_t idHigh;

    uint8_t idLow;

    uint8_t length_freq_list:3;
    uint8_t continuity:1;
    uint8_t range_modulation:4;

    void setId(uint16_t id) {
        idHigh = id >> 8;
        idLow = id & 0xFF;
    }
} PACKED;

struct FIGtype0_21_fi_dab_entry {
    uint8_t freqHigh:3;
    uint8_t control_field:5;

    uint8_t freqMid;

    uint8_t freqLow;

    void setFreq(uint32_t freq) {
        freqHigh = (freq >> 16) & 0x7;
        freqMid = (freq >> 8) & 0xff;
        freqLow = freq & 0xff;
    }
} PACKED;


FIG0_21::FIG0_21(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_21::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_21_TRACE debug
    FillStatus fs;
    size_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    etiLog.level(FIG0_21_TRACE) << "FIG0_21::fill init " <<
        (m_initialised ? 1 : 0) << " ********************************";

    if (not m_initialised) {
        freqInfoFIG0_21 = ensemble->frequency_information.begin();
        m_initialised = true;
    }

    FIGtype0* fig0 = nullptr;

    for (; freqInfoFIG0_21 != ensemble->frequency_information.end();
            ++freqInfoFIG0_21) {

        size_t required_fi_size = 2; // RegionId + length of FI list
        for (const auto& fle : (*freqInfoFIG0_21)->frequency_information) {
            size_t list_entry_size = sizeof(FIGtype0_21_fi_list_header);
            switch (fle.rm) {
                case RangeModulation::dab_ensemble:
                    list_entry_size += fle.fi_dab.frequencies.size() * 3;
                    break;
                case RangeModulation::fm_with_rds:
                    list_entry_size += fle.fi_fm.frequencies.size() * 1;
                    break;
                case RangeModulation::amss:
                    list_entry_size += 1; // Id field 2
                    list_entry_size += fle.fi_amss.frequencies.size() * 2;
                    break;
                case RangeModulation::drm:
                    list_entry_size += 1; // Id field 2
                    list_entry_size += fle.fi_drm.frequencies.size() * 2;
                    break;
            }
            required_fi_size += list_entry_size;
        }

        const size_t required_size = sizeof(struct FIGtype0_21_header) + required_fi_size;

        etiLog.level(FIG0_21_TRACE) << "FIG0_21::loop " << (*freqInfoFIG0_21)->uid;

        if (fig0 == nullptr) {
            if (remaining < 2 + required_size) {
                etiLog.level(FIG0_21_TRACE) << "FIG0_21::no space for fig0";
                break;
            }
            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;

            // Database start or continuation flag, EN 300 401 Clause 5.2.2.1 part b)
            fig0->CN =
                (freqInfoFIG0_21 == ensemble->frequency_information.begin() ? 0 : 1);
            fig0->OE = 0;
            fig0->PD = false;
            fig0->Extension = 21;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            etiLog.level(FIG0_21_TRACE) << "FIG0_21::no space";
            break;
        }

        for (const auto& fle : (*freqInfoFIG0_21)->frequency_information) {
            auto *fig0_21_header = (FIGtype0_21_header*)buf;
            fig0_21_header->rfaHigh = 0;
            fig0_21_header->rfaLow = 0;
            fig0_21_header->length_fi = sizeof(struct FIGtype0_21_fi_list_header);
            switch (fle.rm) {
                case RangeModulation::dab_ensemble:
                    fig0_21_header->length_fi += 3 * fle.fi_dab.frequencies.size();
                    etiLog.level(FIG0_21_TRACE) << "FIG0_21::fle hdr DAB " <<
                        fle.fi_dab.frequencies.size();
                    break;
                case RangeModulation::fm_with_rds:
                    fig0_21_header->length_fi += fle.fi_fm.frequencies.size();
                    etiLog.level(FIG0_21_TRACE) << "FIG0_21::fle hdr FM " <<
                        fle.fi_fm.frequencies.size();
                    break;
                case RangeModulation::drm:
                    fig0_21_header->length_fi += 1; // ID field 2
                    fig0_21_header->length_fi += 2*fle.fi_drm.frequencies.size();
                    etiLog.level(FIG0_21_TRACE) << "FIG0_21::fle hdr DRM " <<
                        fle.fi_drm.frequencies.size();
                    break;
                case RangeModulation::amss:
                    fig0_21_header->length_fi += 1; // ID field 2
                    fig0_21_header->length_fi += 2*fle.fi_amss.frequencies.size();
                    etiLog.level(FIG0_21_TRACE) << "FIG0_21::fle hdr AMSS " <<
                        fle.fi_amss.frequencies.size();
                    break;
            }

            fig0->Length += sizeof(struct FIGtype0_21_header);
            buf += sizeof(struct FIGtype0_21_header);
            remaining -= sizeof(struct FIGtype0_21_header);

            auto *fi_list_header = (FIGtype0_21_fi_list_header*)buf;
            fig0->Length += sizeof(struct FIGtype0_21_fi_list_header);
            buf += sizeof(struct FIGtype0_21_fi_list_header);
            remaining -= sizeof(struct FIGtype0_21_fi_list_header);

            fi_list_header->continuity = fle.continuity;
            fi_list_header->length_freq_list = 0;
            fi_list_header->range_modulation = static_cast<uint8_t>(fle.rm);

            switch (fle.rm) {
                case RangeModulation::dab_ensemble:
                    fi_list_header->setId(fle.fi_dab.eid);
                    assert(fle.fi_dab.frequencies.size() < 8);

                    for (const auto& freq : fle.fi_dab.frequencies) {
                        auto *field = (FIGtype0_21_fi_dab_entry*)buf;
                        field->control_field = static_cast<uint8_t>(freq.control_field);
                        field->setFreq(static_cast<uint32_t>(
                                    freq.frequency * 1000.0f / 16.0f));

                        fi_list_header->length_freq_list += 3;
                        fig0->Length += 3;
                        buf += 3;
                        remaining -= 3;
                        etiLog.level(FIG0_21_TRACE) << "FIG0_21::freq " <<
                            freq.frequency;
                    }
                    break;
                case RangeModulation::fm_with_rds:
                    fi_list_header->setId(fle.fi_fm.pi_code);

                    for (const auto& freq : fle.fi_fm.frequencies) {
                        // RealFreq = 87.5 MHz + (F * 100kHz)
                        // => F = (RealFreq - 87.5 MHz) / 100kHz
                        // Do the whole calculation in kHz:
                        *buf = (freq * 1000.0f - 87500.0f) / 100.0f;

                        fi_list_header->length_freq_list += 1;
                        fig0->Length += 1;
                        buf += 1;
                        remaining -= 1;
                        etiLog.level(FIG0_21_TRACE) << "FIG0_21::freq " <<
                            freq;
                    }
                    break;
                case RangeModulation::drm:
                    fi_list_header->setId((fle.fi_drm.drm_service_id) & 0xFFFF);

                    // Id field 2
                    *buf = (fle.fi_drm.drm_service_id >> 16) & 0xFF;

                    fi_list_header->length_freq_list += 1;
                    fig0->Length += 1;
                    buf += 1;
                    remaining -= 1;

                    for (const auto& freq : fle.fi_drm.frequencies) {
                        uint16_t freq_field = static_cast<uint16_t>(freq * 1000.0f);
                        buf[0] = freq_field >> 8;
                        buf[1] = freq_field & 0xFF;

                        fi_list_header->length_freq_list += 2;
                        fig0->Length += 2;
                        buf += 2;
                        remaining -= 2;
                        etiLog.level(FIG0_21_TRACE) << "FIG0_21::freq_field " <<
                            freq_field;
                    }
                    break;
                case RangeModulation::amss:
                    fi_list_header->setId((fle.fi_amss.amss_service_id) & 0xFFFF);

                    // Id field 2
                    *buf = (fle.fi_amss.amss_service_id >> 16) & 0xFF;

                    fi_list_header->length_freq_list += 1;
                    fig0->Length += 1;
                    buf += 1;
                    remaining -= 1;

                    for (const auto& freq : fle.fi_amss.frequencies) {
                        uint16_t freq_field = static_cast<uint16_t>(freq * 1000.0f);
                        buf[0] = freq_field >> 8;
                        buf[1] = freq_field & 0xFF;

                        fi_list_header->length_freq_list += 2;
                        fig0->Length += 2;
                        buf += 2;
                        remaining -= 2;
                        etiLog.level(FIG0_21_TRACE) << "FIG0_21::freq_field " <<
                            freq_field;
                    }
                    break;
            } // switch (RM)
            etiLog.level(FIG0_21_TRACE) << "FIG0_21::fle end len=" <<
                static_cast<size_t>(fig0->Length) << " rem=" << remaining;
        } // for over fle
        etiLog.level(FIG0_21_TRACE) << "FIG0_21::next FI "
            " ********************************";
    } // for over FI

    if (freqInfoFIG0_21 == ensemble->frequency_information.end()) {
        fs.complete_fig_transmitted = true;
        freqInfoFIG0_21 = ensemble->frequency_information.begin();
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}

