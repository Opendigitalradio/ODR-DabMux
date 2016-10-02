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

#include "fig/FIG0_0.h"
#include "utils.h"

namespace FIC {

//=========== FIG 0/0 ===========

FillStatus FIG0_0::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;

    if (max_size < 6) {
        fs.num_bytes_written = 0;
        return fs;
    }

    FIGtype0_0 *fig0_0;
    fig0_0 = (FIGtype0_0 *)buf;

    fig0_0->FIGtypeNumber = 0;
    fig0_0->Length = 5;
    fig0_0->CN = 0;
    fig0_0->OE = 0;
    fig0_0->PD = 0;
    fig0_0->Extension = 0;

    fig0_0->EId = htons(m_rti->ensemble->id);
    fig0_0->Change = 0;
    fig0_0->Al = 0;
    fig0_0->CIFcnt_hight = (m_rti->currentFrame / 250) % 20;
    fig0_0->CIFcnt_low = (m_rti->currentFrame % 250);

    fs.complete_fig_transmitted = true;
    fs.num_bytes_written = 6;
    return fs;
}

}

