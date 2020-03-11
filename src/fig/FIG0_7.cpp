/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2020
   Matthias P. Braendli, matthias.braendli@mpb.li
   Mathias Kuntze, mathias@kuntze.email
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
#include "fig/FIG0_7.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_7 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint8_t ReconfigCounter_high:2;
    uint8_t ServiceCount:6;
    uint8_t ReconfigCounter_low:8;
} PACKED;

//=========== FIG 0/7 ===========

FillStatus FIG0_7::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    auto ensemble = m_rti->ensemble;

    if (ensemble->reconfig_counter < 0) {
        // FIG 0/7 is disabled
        fs.complete_fig_transmitted = true;
        fs.num_bytes_written = 0;
        return fs;
    }

    FIGtype0_7 *fig0_7;
    fig0_7 = (FIGtype0_7 *)buf;

    fig0_7->FIGtypeNumber = 0;
    fig0_7->Length = 3;
    fig0_7->CN = 0;
    fig0_7->OE = 0;
    fig0_7->PD = 0;
    fig0_7->Extension = 7;

    fig0_7->ServiceCount = ensemble->services.size();
    fig0_7->ReconfigCounter_high = (ensemble->reconfig_counter % 1024) / 256;
    fig0_7->ReconfigCounter_low = (ensemble->reconfig_counter % 1024) % 256;

    fs.complete_fig_transmitted = true;
    fs.num_bytes_written = 4;
    return fs;
}

}

