/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#include "dabInput.h"

#include <string.h>

TcpLog etiLog;


bool dabInputOperations::operator==(const dabInputOperations& ops)
{
    return memcmp(this, &ops, sizeof(*this)) == 0;
}


int dabInputSetbuf(void* args, int size)
{
    return 0;
}


int dabInputSetbitrate(dabInputOperations* ops, void* args, int bitrate)
{
    if (bitrate <= 0) {
        etiLog.print(TcpLog::ERR, "Invalid bitrate (%i)\n", bitrate);
        return -1;
    }
    if (ops->setbuf(args, bitrate * 3) == 0) {
        return bitrate;
    }
    return -1;
}
