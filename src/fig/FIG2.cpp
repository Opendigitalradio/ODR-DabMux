/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of FIG2
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

#include <algorithm>
#include <iomanip>
#include "fig/FIG2.h"
#include "lib/charset/charset.h"

namespace FIC {

using namespace std;

void FIG2_Segments::clear()
{
    segments.clear();
    current_segment_it = segments.end();
}

void FIG2_Segments::load(const string& label)
{
    /* ETSI EN 300 401 Clause 5.2.2.3.3:
     * "The label may be carried in one or two FIGs"
     */

    if (label.size() > 32) {
        throw runtime_error("FIG2 label too long: " + to_string(label.size()));
    }

    if (label != label_on_last_load) {
        toggle = not toggle;
        label_on_last_load = label;
    }

    segments.clear();
    vector<uint8_t> label_bytes(label.begin(), label.end());

    for (size_t i = 0; i < label_bytes.size(); i+=16) {
        size_t len = distance(label_bytes.begin() + i, label_bytes.end());

        len = min(len, (size_t)16);

        segments.emplace_back(label_bytes.begin() + i, label_bytes.begin() + i + len);
    }

    current_segment_it = segments.begin();
}

size_t FIG2_Segments::segment_count() const {
    if (segments.empty()) {
        throw runtime_error("Empty FIG2 has not segments");
    }
    return segments.size() - 1;
}

std::vector<uint8_t> FIG2_Segments::advance_segment() {
    if (current_segment_it == segments.end()) {
        return {};
    }
    else {
        return *(current_segment_it++);
    }
}

size_t FIG2_Segments::current_segment_length() const {
    if (current_segment_it == segments.end()) {
        return 0;
    }
    else {
        return current_segment_it->size();
    }
}

size_t FIG2_Segments::current_segment_index() const {
    vv::const_iterator cur = current_segment_it;
    return distance(segments.begin(), cur);
}

bool FIG2_Segments::ready() const {
    return not segments.empty();
}

bool FIG2_Segments::complete() const {
    return not segments.empty() and current_segment_it == segments.end();
}

int FIG2_Segments::toggle_flag() const {
    return toggle ? 1 : 0;
}

// Ensemble label
FillStatus FIG2_0::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    ssize_t remaining = max_size;

    if (not ensemble->label.has_fig2_label()) {
        fs.complete_fig_transmitted = true;
        fs.num_bytes_written = 0;
        return fs;
    }

    if (not m_segments.ready()) {
        m_segments.load(ensemble->label.fig2_label());

        if (not m_segments.ready()) {
            throw logic_error("Non-empty label but segments not ready()");
        }
    }

    const ssize_t required_bytes = (m_segments.current_segment_index() == 0) ?
        sizeof(FIGtype2) + 2 + sizeof(FIG2_Extended_Label) + m_segments.current_segment_length() :
        sizeof(FIGtype2) + 2 + m_segments.current_segment_length();

    if (remaining < required_bytes) {
        fs.num_bytes_written = 0;
        return fs;
    }

    auto fig2 = (FIGtype2*)buf;

    fig2->Length = required_bytes - 1;
    fig2->FIGtypeNumber = 2;

    fig2->Extension = 0;
    fig2->Rfu = 0;
    fig2->SegmentIndex = m_segments.current_segment_index();
    fig2->ToggleFlag = m_segments.toggle_flag();

    buf += sizeof(FIGtype2);
    remaining -= sizeof(FIGtype2);

    // Identifier field
    buf[0] = ensemble->id >> 8;
    buf[1] = ensemble->id & 0xFF;
    buf += 2;
    remaining -= 2;

    if (m_segments.current_segment_index() == 0) {
        // EN 300 401 5.2.2.3.3 "The second segment shall be carried in a following
        // FIG type 2 data field ...", i.e. do not insert the header anymore.
        auto ext = (FIG2_Extended_Label*)buf;
        ext->Rfa = 0;
        ext->SegmentCount = m_segments.segment_count();
        ext->EncodingFlag = 0; // UTF-8
        ext->CharacterFlag = htons(0xFF00); // Short label always truncation

        buf += sizeof(FIG2_Extended_Label);
        remaining -= sizeof(FIG2_Extended_Label);
    }

    const auto character_field = m_segments.advance_segment();
    copy(character_field.begin(), character_field.end(), buf);
    buf += character_field.size();
    remaining -= character_field.size();

    if (m_segments.complete()) {
        fs.complete_fig_transmitted = true;
        m_segments.clear();
    }
    fs.num_bytes_written = max_size - remaining;
    return fs;
}

// Programme service label and data service label
FillStatus FIG2_1_and_5::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    ssize_t remaining = max_size;

    if (not m_initialised) {
        service = m_rti->ensemble->services.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    // Rotate through the subchannels until there is no more space
    while (service != ensemble->services.end()) {
        const bool is_programme = (*service)->getType(ensemble) == subchannel_type_t::DABAudio or
                ((*service)->getType(ensemble) == subchannel_type_t::DABPlusAudio);

        if (not (m_programme xor is_programme) and (*service)->label.has_fig2_label()) {

            auto& segments = segment_per_service[(*service)->id];

            if (not segments.ready()) {
                segments.load((*service)->label.fig2_label());

                if (not segments.ready()) {
                    throw logic_error("Non-empty label but segments not ready()");
                }
            }

            const size_t id_length = (is_programme ? 2 : 4);

            const ssize_t required_bytes = sizeof(FIGtype2) + id_length + segments.current_segment_length() +
                ((segments.current_segment_index() == 0) ? sizeof(FIG2_Extended_Label) : 0);

            if (remaining < required_bytes) {
                break;
            }

            auto fig2 = (FIGtype2*)buf;

            fig2->Length = required_bytes - 1;
            fig2->FIGtypeNumber = 2;

            fig2->Extension = figextension();
            fig2->Rfu = 0;
            fig2->SegmentIndex = segments.current_segment_index();
            fig2->ToggleFlag = segments.toggle_flag();

            buf += sizeof(FIGtype2);
            remaining -= sizeof(FIGtype2);

            // Identifier field
            if (is_programme) {
                buf[0] = (*service)->id >> 8;
                buf[1] = (*service)->id & 0xFF;
            }
            else {
                buf[0] = ((*service)->id >> 24) & 0xFF;
                buf[1] = ((*service)->id >> 16) & 0xFF;
                buf[2] = ((*service)->id >> 8) & 0xFF;
                buf[3] = (*service)->id & 0xFF;
            }
            buf += id_length;
            remaining -= id_length;

            if (segments.current_segment_index() == 0) {
                auto ext = (FIG2_Extended_Label*)buf;
                ext->Rfa = 0;
                ext->SegmentCount = segments.segment_count();
                ext->EncodingFlag = 0; // UTF-8
                ext->CharacterFlag = htons(0xFF00); // Short label always truncation

                buf += sizeof(FIG2_Extended_Label);
                remaining -= sizeof(FIG2_Extended_Label);
            }

            const auto character_field = segments.advance_segment();
            copy(character_field.begin(), character_field.end(), buf);
            buf += character_field.size();
            remaining -= character_field.size();

            if (segments.complete()) {
                segments.clear();
                ++service;
            }
        }
        else {
            ++service;
        }
    }

    if (service == ensemble->services.end()) {
        service = ensemble->services.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

// Component label
FillStatus FIG2_4::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    ssize_t remaining = max_size;

    if (not m_initialised) {
        component = m_rti->ensemble->components.end();
        m_initialised = true;
    }

    auto ensemble = m_rti->ensemble;

    while (component != ensemble->components.end()) {
        if ((*component)->label.has_fig2_label()) {
            auto service = getService(*component, ensemble->services);

            auto& segments = segment_per_component[{(*component)->serviceId, (*component)->SCIdS}];

            if (not segments.ready()) {
                segments.load((*component)->label.fig2_label());

                if (not segments.ready()) {
                    throw logic_error("Non-empty label but segments not ready()");
                }
            }

            const bool is_programme = (*service)->getType(ensemble) == subchannel_type_t::DABAudio or
                    ((*service)->getType(ensemble) == subchannel_type_t::DABPlusAudio);

            const size_t id_length = is_programme ?
                sizeof(FIGtype2_4_Programme_Identifier) :
                sizeof(FIGtype2_4_Data_Identifier);

            const ssize_t required_bytes = sizeof(FIGtype2) + id_length + segments.current_segment_length() +
                ((segments.current_segment_index() == 0) ? sizeof(FIG2_Extended_Label) : 0);

            if (remaining < required_bytes) {
                break;
            }

            auto fig2 = (FIGtype2*)buf;

            fig2->Length = required_bytes - 1;
            fig2->FIGtypeNumber = 2;

            fig2->Extension = 4;
            fig2->Rfu = 0;
            fig2->SegmentIndex = segments.current_segment_index();
            fig2->ToggleFlag = segments.toggle_flag();

            buf += sizeof(FIGtype2);
            remaining -= sizeof(FIGtype2);

            // Identifier field
            if (is_programme) {
                auto fig2_4 = (FIGtype2_4_Programme_Identifier*)buf;

                fig2_4->SCIdS = (*component)->SCIdS;
                fig2_4->rfa = 0;
                fig2_4->PD = 0;
                fig2_4->SId = htons((*service)->id);

                buf += sizeof(FIGtype2_4_Programme_Identifier);
                remaining -= sizeof(FIGtype2_4_Programme_Identifier);
            }
            else {
                auto fig2_4 = (FIGtype2_4_Data_Identifier*)buf;

                fig2_4->SCIdS = (*component)->SCIdS;
                fig2_4->rfa = 0;
                fig2_4->PD = 1;
                fig2_4->SId = htonl((*service)->id);

                buf += sizeof(FIGtype2_4_Data_Identifier);
                remaining -= sizeof(FIGtype2_4_Data_Identifier);
            }

            if (segments.current_segment_index() == 0) {
                auto ext = (FIG2_Extended_Label*)buf;
                ext->Rfa = 0;
                ext->SegmentCount = segments.segment_count();
                ext->EncodingFlag = 0; // UTF-8
                ext->CharacterFlag = htons(0xFF00); // Short label always truncation

                buf += sizeof(FIG2_Extended_Label);
                remaining -= sizeof(FIG2_Extended_Label);
            }

            const auto character_field = segments.advance_segment();
            copy(character_field.begin(), character_field.end(), buf);
            buf += character_field.size();
            remaining -= character_field.size();

            if (segments.complete()) {
                segments.clear();
                ++component;
            }
        }
        else {
            ++component;
        }
    }

    if (component == ensemble->components.end()) {
        component = ensemble->components.begin();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

} // namespace FIC

