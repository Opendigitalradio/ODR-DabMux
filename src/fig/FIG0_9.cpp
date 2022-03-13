/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2021 Matthias P. Braendli
    http://www.opendigitalradio.org
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
#include "fig/FIG0_9.h"
#include "utils.h"
#include <map>

using namespace std;

namespace FIC {

struct FIGtype0_9 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint8_t ensembleLto:6;
    uint8_t rfa1:1;
    uint8_t ext:1;
    uint8_t ensembleEcc;
    uint8_t tableId;
} PACKED;

struct FIGtype0_9_Subfield {
    uint8_t Rfa2:6;
    uint8_t NumServices:2;

    uint8_t ecc;
    // Followed by list of NumServices uint16_t SIDs
} PACKED;

FIG0_9::FIG0_9(FIGRuntimeInformation *rti) :
    m_rti(rti) {}

FillStatus FIG0_9::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (m_extended_fields.empty()) {
        map<uint8_t, list<uint16_t> > ecc_to_services;
        for (const auto& srv : ensemble->services) {
            if (srv->ecc != 0 and srv->ecc != ensemble->ecc and srv->isProgramme(ensemble)) {
                if (srv->id > 0xFFFF) {
                    throw std::logic_error("Service ID for programme service > 0xFFFF");
                }
                ecc_to_services[srv->ecc].push_back(srv->id);
            }
        }

        // Reorganise the data so that it fits into the
        // extended field. Max 3 SIds per sub-field.
        for (auto& ecc_sids_pair : ecc_to_services) {
            FIG0_9_Extended_Field ef;
            ef.ecc = ecc_sids_pair.first;

            size_t i = 0;
            for (auto& sid : ecc_sids_pair.second) {
                if (i == 3) {
                    m_extended_fields.push_back(ef);
                    ef.sids.clear();
                    i = 0;
                }

                ef.sids.push_back(sid);
                i++;
            }

            if (not ef.sids.empty()) {
                m_extended_fields.push_back(ef);
            }
        }

        // Max length of extended field is 25 bytes
        size_t subfield_required = 0;
        for (const auto& ef : m_extended_fields) {
            subfield_required += ef.required_bytes();
        }

        if (subfield_required > 25) {
            etiLog.level(error) << "Cannot transmit FIG 0/9: too many services with different ECC";
        }

        for (const auto& ef : m_extended_fields) {
            stringstream ss;
            ss << "FIG0_9 Ext ECC 0x" << hex << (int)ef.ecc << dec << ":";

            for (auto& sid : ef.sids) {
                ss << " " << hex << (int)sid << dec;
            }
            ss << ".";
            etiLog.level(debug) << ss.str();
        }
    }

    // Transmitting a FIG0/9 without any extended field was the CEI in EN 300 401 v1.
    // It went away in v2, and I interpret this that it is impossible to transmit
    // more than 11 services with a different ECC.
    size_t required = 5;
    for (const auto& ef : m_extended_fields) {
        required += ef.required_bytes();
    }

    if (remaining < required) {
        fs.num_bytes_written = 0;
        return fs;
    }

    auto fig0_9 = (FIGtype0_9*)buf;
    fig0_9->FIGtypeNumber = 0;
    fig0_9->Length = 4;
    fig0_9->CN = 0;
    fig0_9->OE = 0;
    fig0_9->PD = 0;
    fig0_9->Extension = 9;

    fig0_9->ext = m_extended_fields.empty() ? 0 : 1;
    fig0_9->rfa1 = 0; // Had a different meaning in EN 300 401 V1.4.1

    if (ensemble->lto_auto) {
        time_t now = time(NULL);
        struct tm ltime;
        localtime_r(&now, &ltime);
        time_t now2 = timegm(&ltime);
        ensemble->lto = (now2 - now) / 1800;
    }

    if (ensemble->lto >= 0) {
        fig0_9->ensembleLto = ensemble->lto;
    }
    else {
        /* Convert to 1-complement representation */
        fig0_9->ensembleLto = (-ensemble->lto) | (1<<5);
    }

    fig0_9->ensembleEcc = ensemble->ecc;
    fig0_9->tableId = ensemble->international_table;
    buf += 5;
    remaining -= 5;

    for (const auto& ef : m_extended_fields) {
        if (ef.required_bytes() > remaining) {
            throw logic_error("Wrong FIG0/9 length calculation");
        }

        if (ef.sids.size() > 3) {
            throw logic_error("Wrong FIG0/9 subfield generation");
        }

        auto subfield = (FIGtype0_9_Subfield*)buf;
        subfield->NumServices = ef.sids.size();
        subfield->Rfa2 = 0;
        subfield->ecc = ef.ecc;
        buf += 2;
        fig0_9->Length += 2;
        remaining -= 2;

        for (uint16_t sid : ef.sids) {
            uint16_t *sid_field = (uint16_t*)buf;
            *sid_field = htons(sid);
            buf += 2;
            fig0_9->Length += 2;
            remaining -= 2;
        }
    }

    fs.num_bytes_written = max_size - remaining;
    fs.complete_fig_transmitted = true;
    return fs;
}

}
