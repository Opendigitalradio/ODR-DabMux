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
#include "fig/FIG0_10.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_10_LongForm {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;

    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;

    uint8_t MJD_high:7;
    uint8_t RFU:1;

    uint8_t MJD_med;

    uint8_t Hours_high:3;
    uint8_t UTC:1;
    uint8_t ConfInd:1;
    uint8_t LSI:1;
    uint8_t MJD_low:2;

    uint8_t Minutes:6;
    uint8_t Hours_low:2;

    uint8_t Milliseconds_high:2;
    uint8_t Seconds:6;

    uint8_t Milliseconds_low;

    void setMJD(uint32_t date) {
        MJD_high = (date >> 10) & 0x7f;
        MJD_med = (date >> 2) & 0xff;
        MJD_low = date & 0x03;
    }
    void setHours(uint16_t hours) {
        Hours_high = (hours >> 2) & 0x07;
        Hours_low = hours & 0x03;
    }

    void setMilliseconds(uint16_t ms) {
        Milliseconds_high = (ms >> 8) & 0x03;
        Milliseconds_low = ms & 0xFF;
    }
} PACKED;

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
