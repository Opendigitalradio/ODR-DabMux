/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
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

using namespace std;

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

    void addToLength(uint8_t increment) {
        const uint32_t newlen = length_freq_list + increment;
        if (newlen > 0x7) {
            throw logic_error("FI freq list too long: " + to_string(newlen));
        }
        length_freq_list = newlen;
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
    m_rti(rti)
{
}

static size_t get_num_frequencies(
        std::vector<FrequencyInformation>::iterator fi)
{
    switch (fi->rm) {
        case RangeModulation::dab_ensemble:
            if (fi->fi_dab.frequencies.empty()) {
                throw std::logic_error("Empty DAB FI");
            }
            return fi->fi_dab.frequencies.size();
        case RangeModulation::fm_with_rds:
            if (fi->fi_fm.frequencies.empty()) {
                throw std::logic_error("Empty FM FI");
            }
            return fi->fi_fm.frequencies.size();
        case RangeModulation::amss:
            if (fi->fi_amss.frequencies.empty()) {
                throw std::logic_error("Empty AMSS FI");
            }
            return fi->fi_amss.frequencies.size();
        case RangeModulation::drm:
            if (fi->fi_drm.frequencies.empty()) {
                throw std::logic_error("Empty DRM FI");
            }
            return fi->fi_drm.frequencies.size();
    }
    throw logic_error("Unhandled get_num_frequencies");
}

FillStatus FIG0_21::fill(uint8_t *buf, size_t max_size)
{
#define FIG0_21_TRACE discard
    FillStatus fs;
    size_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    if (not m_initialised) {
        freqInfoFIG0_21 = ensemble->frequency_information.begin();
        fi_frequency_index = 0;

        if (freqInfoFIG0_21 != ensemble->frequency_information.end()) {
            m_last_oe = freqInfoFIG0_21->other_ensemble;
        }
        m_initialised = true;
    }

    FIGtype0* fig0 = nullptr;

    auto advance_loop = [&](void){
            if (fi_frequency_index == get_num_frequencies(freqInfoFIG0_21)) {
                ++freqInfoFIG0_21;
                fi_frequency_index = 0;
            }
        };

    for (; freqInfoFIG0_21 != ensemble->frequency_information.end();
            advance_loop()) {
        /* For better usage of FIC capacity, we want to transmit
         * frequency lists with
         * 2 DAB frequencies,
         * 7 FM frequencies,
         * 3 AM/DRM frequencies
         */

        // Check we have space for one frequency
        size_t required_fi_size = 2; // RegionId + length of FI list
        size_t list_entry_size = sizeof(struct FIGtype0_21_fi_list_header);
        switch (freqInfoFIG0_21->rm) {
            case RangeModulation::dab_ensemble:
                list_entry_size += 3;
                break;
            case RangeModulation::fm_with_rds:
                list_entry_size += 1;
                break;
            case RangeModulation::amss:
                list_entry_size += 1; // Id field 2
                list_entry_size += 2;
                break;
            case RangeModulation::drm:
                list_entry_size += 1; // Id field 2
                list_entry_size += 2;
                break;
        }
        required_fi_size += list_entry_size;

        const size_t required_size =
            sizeof(struct FIGtype0_21_header) + required_fi_size;

        if (m_last_oe != freqInfoFIG0_21->other_ensemble) {
            // Trigger resend of FIG0 when OE changes
            fig0 = nullptr;
            m_last_oe = freqInfoFIG0_21->other_ensemble;
            etiLog.level(FIG0_21_TRACE) << "FIG0_21::switch OE to " <<
                freqInfoFIG0_21->other_ensemble;
        }

        if (fig0 == nullptr) {
            if (remaining < 2 + required_size) {
                break;
            }
            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;

            // Database start or continuation flag, EN 300 401 Clause 5.2.2.1 part b)
            fig0->CN = (fi_frequency_index == 0 ? 0 : 1);
            fig0->OE = freqInfoFIG0_21->other_ensemble ? 1 : 0;
            fig0->PD = false;
            fig0->Extension = 21;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        etiLog.level(FIG0_21_TRACE) << "FIG0_21::loop " << freqInfoFIG0_21->uid << " " <<
            std::distance(ensemble->frequency_information.begin(), freqInfoFIG0_21) <<
            " freq entry " << fi_frequency_index;

        auto *fig0_21_header = (FIGtype0_21_header*)buf;
        fig0_21_header->rfaHigh = 0;
        fig0_21_header->rfaLow = 0;
        fig0_21_header->length_fi = sizeof(FIGtype0_21_fi_list_header);

        fig0->Length += sizeof(FIGtype0_21_header);
        buf          += sizeof(FIGtype0_21_header);
        remaining    -= sizeof(FIGtype0_21_header);

        auto *fi_list_header = (FIGtype0_21_fi_list_header*)buf;
        fig0->Length += sizeof(FIGtype0_21_fi_list_header);
        buf          += sizeof(FIGtype0_21_fi_list_header);
        remaining    -= sizeof(FIGtype0_21_fi_list_header);

        fi_list_header->continuity = freqInfoFIG0_21->continuity;
        fi_list_header->length_freq_list = 0;
        fi_list_header->range_modulation = static_cast<uint8_t>(freqInfoFIG0_21->rm);

        bool continue_loop = true;

        switch (freqInfoFIG0_21->rm) {
            case RangeModulation::dab_ensemble:
                fi_list_header->setId(freqInfoFIG0_21->fi_dab.eid);

                for (size_t num_inserted = 0, i = fi_frequency_index;
                        num_inserted < 2 and
                        i < freqInfoFIG0_21->fi_dab.frequencies.size();
                        num_inserted++, i++) {
                    if (remaining < 3) {
                        continue_loop = false;
                        break;
                    }

                    const auto& freq = freqInfoFIG0_21->fi_dab.frequencies.at(i);

                    auto *field = (FIGtype0_21_fi_dab_entry*)buf;
                    field->control_field = static_cast<uint8_t>(freq.control_field);
                    field->setFreq(static_cast<uint32_t>(
                                freq.frequency * 1000.0f / 16.0f));

                    fig0_21_header->length_fi += 3;
                    fi_list_header->addToLength(3);
                    fig0->Length += 3;
                    buf += 3;
                    remaining -= 3;
                    etiLog.level(FIG0_21_TRACE) << "FIG0_21::freq " <<
                        fi_frequency_index << " " << freq.frequency << " rem " << remaining;
                    fi_frequency_index++;
                }
                break;
            case RangeModulation::fm_with_rds:
                fi_list_header->setId(freqInfoFIG0_21->fi_fm.pi_code);

                for (size_t num_inserted = 0, i = fi_frequency_index;
                        num_inserted < 7 and
                        i < freqInfoFIG0_21->fi_fm.frequencies.size();
                        num_inserted++, i++) {
                    if (remaining < 1) {
                        continue_loop = false;
                        break;
                    }

                    const auto& freq = freqInfoFIG0_21->fi_fm.frequencies.at(i);
                    // RealFreq = 87.5 MHz + (F * 100kHz)
                    // => F = (RealFreq - 87.5 MHz) / 100kHz
                    // Do the whole calculation in kHz:
                    *buf = (freq * 1000.0f - 87500.0f) / 100.0f;

                    fig0_21_header->length_fi += 1;
                    fi_list_header->addToLength(1);
                    fig0->Length += 1;
                    buf += 1;
                    remaining -= 1;
                    fi_frequency_index++;
                }
                break;
            case RangeModulation::drm:
                fi_list_header->setId((freqInfoFIG0_21->fi_drm.drm_service_id) & 0xFFFF);

                if (remaining < 3) {
                    throw logic_error("Incorrect DRM FI size calculation");
                }

                // Id field 2
                *buf = (freqInfoFIG0_21->fi_drm.drm_service_id >> 16) & 0xFF;
                fig0_21_header->length_fi += 1;

                fi_list_header->addToLength(1);
                fig0->Length += 1;
                buf += 1;
                remaining -= 1;

                for (size_t num_inserted = 0, i = fi_frequency_index;
                        num_inserted < 3 and
                        i < freqInfoFIG0_21->fi_drm.frequencies.size();
                        num_inserted++, i++) {
                    if (remaining < 2) {
                        continue_loop = false;
                        break;
                    }

                    const auto& freq = freqInfoFIG0_21->fi_drm.frequencies.at(i);
                    uint16_t freq_field = static_cast<uint16_t>(freq * 1000.0f);
                    buf[0] = freq_field >> 8;
                    buf[1] = freq_field & 0xFF;

                    fi_list_header->addToLength(2);
                    fig0_21_header->length_fi += 2;
                    fig0->Length += 2;
                    buf += 2;
                    remaining -= 2;
                    fi_frequency_index++;
                }
                break;
            case RangeModulation::amss:
                fi_list_header->setId((freqInfoFIG0_21->fi_amss.amss_service_id) & 0xFFFF);

                if (remaining < 3) {
                    throw logic_error("Incorrect AMSS FI size calculation");
                }

                // Id field 2
                *buf = (freqInfoFIG0_21->fi_amss.amss_service_id >> 16) & 0xFF;
                fig0_21_header->length_fi += 1;

                fi_list_header->addToLength(1);
                fig0->Length += 1;
                buf += 1;
                remaining -= 1;

                for (size_t num_inserted = 0, i = fi_frequency_index;
                        num_inserted < 3 and
                        i < freqInfoFIG0_21->fi_amss.frequencies.size();
                        num_inserted++, i++) {
                    if (remaining < 2) {
                        continue_loop = false;
                        break;
                    }

                    const auto& freq = freqInfoFIG0_21->fi_amss.frequencies.at(i);
                    uint16_t freq_field = static_cast<uint16_t>(freq * 1000.0f);
                    buf[0] = freq_field >> 8;
                    buf[1] = freq_field & 0xFF;

                    fi_list_header->addToLength(2);
                    fig0_21_header->length_fi += 2;
                    fig0->Length += 2;
                    buf += 2;
                    remaining -= 2;
                    fi_frequency_index++;
                }
                break;
        } // switch (RM)

        etiLog.level(FIG0_21_TRACE) << "FIG0_21::end " <<
            (continue_loop ? "same " : "next ") << " len=" <<
            static_cast<size_t>(fig0->Length) << " rem=" << remaining <<
            " ********************************";

        if (not continue_loop) {
            // There is no more space left. The freqInfoFIG0_21 iterator
            // and fi_frequency_index save the position how to continue
            // next time we are called.
            break;
        }
    } // for over FI

    if (freqInfoFIG0_21 == ensemble->frequency_information.end()) {
        fs.complete_fig_transmitted = true;

        freqInfoFIG0_21 = ensemble->frequency_information.begin();
        fi_frequency_index = 0;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}

