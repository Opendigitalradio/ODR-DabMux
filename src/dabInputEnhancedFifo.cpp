/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#include "dabInputEnhancedFifo.h"
#include "dabInputPacketFile.h"
#include "dabInputFifo.h"


#ifdef HAVE_FORMAT_PACKET
#   ifdef HAVE_FORMAT_EPM
#       ifdef HAVE_INPUT_FIFO


struct dabInputOperations dabInputEnhancedFifoOperations = {
    dabInputEnhancedFifoInit,
    dabInputFifoOpen,
    dabInputFifoSetbuf,
    dabInputFifoRead,
    dabInputFifoLock,
    dabInputFifoUnlock,
    dabInputPacketFileRead,
    dabInputSetbitrate,
    dabInputFifoClose,
    dabInputFifoClean,
    dabInputFifoRewind
};


int dabInputEnhancedFifoInit(void** args)
{
    dabInputFifoInit(args);
    dabInputFifoData* data = (dabInputFifoData*)*args;

    data->packetData = new unsigned char[96];
    data->enhancedPacketData = new unsigned char*[12];
    for (int i = 0; i < 12; ++i) {
        data->enhancedPacketData[i] = new unsigned char[204];
    }

    return 0;
}


#       endif
#   endif
#endif
