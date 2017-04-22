/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
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
#include "fig/FIG0_9.h"
#include "utils.h"

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

FIG0_9::FIG0_9(FIGRuntimeInformation *rti) :
    m_rti(rti) {}

FillStatus FIG0_9::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 5) {
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

    fig0_9->ext = 0;
    fig0_9->rfa1 = 0; // Had a different meaning in EN 300 401 V1.4.1

    if (ensemble->lto_auto) {
        time_t now = time(NULL);
        struct tm* ltime = localtime(&now);
        time_t now2 = timegm(ltime);
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

    /* No extended field, no support for services with different ECC */

    fs.num_bytes_written = max_size - remaining;
    fs.complete_fig_transmitted = true;
    return fs;
}

}
