/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of FIG1
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

#include "fig/FIG1.h"
#include "DabMultiplexer.h"

namespace FIC {

//=========== FIG 1/0 ===========

FillStatus FIG1_0::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 22) {
        fs.num_bytes_written = 0;
        return fs;
    }

    auto fig1_0 = (FIGtype1_0*)buf;

    fig1_0->Length = 21;
    fig1_0->FIGtypeNumber = 1;
    fig1_0->Extension = 0;
    fig1_0->OE = 0;
    fig1_0->Charset = 0;
    fig1_0->EId = htons(ensemble->id);
    buf += 4;

    ensemble->label.writeLabel(buf);
    buf += 16;

    buf[0] = ensemble->label.flag() >> 8;
    buf[1] = ensemble->label.flag() & 0xFF;
    buf += 2;

    remaining -= 22;

    fs.complete_fig_transmitted = true;
    fs.num_bytes_written = max_size - remaining;
    return fs;
}

} // namespace FIC

