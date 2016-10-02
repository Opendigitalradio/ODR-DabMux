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

#include "fig/FIG0_10.h"
#include "utils.h"

namespace FIC {

FIG0_10::FIG0_10(FIGRuntimeInformation *rti) :
    m_rti(rti)
{
}

FillStatus FIG0_10::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    auto ensemble = m_rti->ensemble;
    size_t remaining = max_size;

    if (remaining < 8) {
        fs.num_bytes_written = 0;
        return fs;
    }

    //Time and country identifier
    auto fig0_10 = (FIGtype0_10_LongForm*)buf;

    fig0_10->FIGtypeNumber = 0;
    fig0_10->Length = 7;
    fig0_10->CN = 0;
    fig0_10->OE = 0;
    fig0_10->PD = 0;
    fig0_10->Extension = 10;
    buf += 2;
    remaining -= 2;

    tm* timeData;
    time_t dab_time_seconds = 0;
    uint32_t dab_time_millis = 0;
    get_dab_time(&dab_time_seconds, &dab_time_millis);
    timeData = gmtime(&dab_time_seconds);

    fig0_10->RFU = 0;
    fig0_10->setMJD(gregorian2mjd(timeData->tm_year + 1900,
                timeData->tm_mon + 1,
                timeData->tm_mday));
    fig0_10->LSI = 0;
    fig0_10->ConfInd = 1;
    fig0_10->UTC = 1;
    fig0_10->setHours(timeData->tm_hour);
    fig0_10->Minutes = timeData->tm_min;
    fig0_10->Seconds = timeData->tm_sec;
    fig0_10->setMilliseconds(dab_time_millis);
    buf += 6;
    remaining -= 6;

    fs.num_bytes_written = max_size - remaining;
    fs.complete_fig_transmitted = true;
    return fs;
}

}
