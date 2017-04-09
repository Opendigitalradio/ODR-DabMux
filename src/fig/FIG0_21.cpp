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

#include "fig/FIG0_21.h"
#include "utils.h"

namespace FIC {

FIG0_21::FIG0_21(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_21::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    if (not m_initialised) {
        freqListEntryFIG0_21 = ensemble->frequency_information.begin();
        m_initialised = true;
    }

    FIGtype0* fig0 = nullptr;

    for (; freqListEntryFIG0_21 != ensemble->frequency_information.end();
            ++freqListEntryFIG0_21) {

        size_t num_list_entries_that_fit = 0;
        size_t required_size = sizeof(struct FIGtype0_21_header);

        size_t list_entry_size = sizeof(FIGtype0_21_fi_list_header);
        switch ((*freqListEntryFIG0_21)->rm) {
            case RangeModulation::dab_ensemble:
                list_entry_size += (*freqListEntryFIG0_21)->fi_dab.frequencies.size() * 3;
                break;
            case RangeModulation::fm_with_rds:
                list_entry_size += (*freqListEntryFIG0_21)->fi_fm.frequencies.size() * 3;
                break;
            case RangeModulation::amss:
                list_entry_size += 1; // Id field 2
                list_entry_size += (*freqListEntryFIG0_21)->fi_amss.frequencies.size() * 3;
                break;
            case RangeModulation::drm:
                list_entry_size += 1; // Id field 2
                list_entry_size += (*freqListEntryFIG0_21)->fi_drm.frequencies.size() * 3;
                break;
        }
        required_size += list_entry_size;

        etiLog.level(debug) << "FIG0/21 " << num_list_entries_that_fit << " entries"
            "will fit";


        if (fig0 == nullptr) {
            if (remaining < 2 + required_size) {
                break;
            }
            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN =
                (freqListEntryFIG0_21 == ensemble->frequency_information.begin() ? 0 : 1);
            fig0->OE = 0;
            fig0->PD = false;
            fig0->Extension = 21;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        auto *fig0_21_header = (FIGtype0_21_header*)buf;
        switch ((*freqListEntryFIG0_21)->rm) {
            case RangeModulation::dab_ensemble:
                fig0_21_header->length_fi = (*freqListEntryFIG0_21)->fi_dab.frequencies.size();
                break;
            case RangeModulation::fm_with_rds:
                fig0_21_header->length_fi = (*freqListEntryFIG0_21)->fi_fm.frequencies.size();
                break;
            case RangeModulation::drm:
                fig0_21_header->length_fi = (*freqListEntryFIG0_21)->fi_drm.frequencies.size();
                break;
            case RangeModulation::amss:
                fig0_21_header->length_fi = (*freqListEntryFIG0_21)->fi_amss.frequencies.size();
                break;
        }
        fig0_21_header->rfa = 0;

#error "Why do we have lists of FI lists? This is confusing"

        switch ((*freqListEntryFIG0_21)->rm) {
            case RangeModulation::dab_ensemble:
                for (const auto& freq : (*freqListEntryFIG0_21)->fi_dab.frequencies) {

                }
                break;
            case RangeModulation::fm_with_rds:
                break;
            case RangeModulation::drm:
                break;
            case RangeModulation::amss:
                break;
        }

        fig0->Length += sizeof(struct FIGtype0_21_header);
        buf += sizeof(struct FIGtype0_21_header);
        remaining -= sizeof(struct FIGtype0_21_header);
    }

    if (freqListEntryFIG0_21 == ensemble->frequency_information.end()) {
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

}

