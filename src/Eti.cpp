/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in Right
   of Canada (Communications Research Center Canada)

   Includes modifications
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
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

#ifdef _WIN32
#   pragma warning ( disable : 4103 )
#   include "Eti.h"
#   pragma warning ( default : 4103 )
#else
#   include "Eti.h"
#   include <time.h>
#endif


//definitions des structures des champs du ETI(NI, G703)


unsigned short eti_FC::getFrameLength()
{
    return (unsigned short)((FL_high << 8) | FL_low);
}


void eti_FC::setFrameLength(uint16_t length)
{
    FL_high = (length >> 8) & 0x07;
    FL_low = length & 0xff;
}


void eti_STC::setSTL(uint16_t length)
{
    STL_high = length >> 8;
    STL_low = length & 0xff;
}


uint16_t eti_STC::getSTL()
{
    return (uint16_t)((STL_high << 8) + STL_low);
}


void eti_STC::setStartAddress(uint16_t address)
{
    startAddress_high = address >> 8;
    startAddress_low = address & 0xff;
}


uint16_t eti_STC::getStartAddress()
{
    return (uint16_t)((startAddress_high << 8) + startAddress_low);
}

/* Helper functions for eti_MNSC_TIME_x which fill the time-relevant
 * fields for the MNSC
 */
void eti_MNSC_TIME_1::setFromTime(struct tm *time_tm)
{
    second_unit = time_tm->tm_sec % 10;
    second_tens = time_tm->tm_sec / 10;
    
    minute_unit = time_tm->tm_min % 10;
    minute_tens = time_tm->tm_min / 10;
}

void eti_MNSC_TIME_2::setFromTime(struct tm *time_tm)
{
    hour_unit = time_tm->tm_hour % 10;
    hour_tens = time_tm->tm_hour / 10;
    
    day_unit = time_tm->tm_mday % 10;
    day_tens = time_tm->tm_mday / 10;
}

void eti_MNSC_TIME_3::setFromTime(struct tm *time_tm)
{
    month_unit = (time_tm->tm_mon + 1) % 10;
    month_tens = (time_tm->tm_mon + 1) / 10;
    
    // They didn't see the y2k bug coming, did they ?
    year_unit = (time_tm->tm_year - 100) % 10;
    year_tens = (time_tm->tm_year - 100) / 10;
}
