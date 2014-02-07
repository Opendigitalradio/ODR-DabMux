/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#ifndef DAB_INPUT_FILE_H
#define DAB_INPUT_FILE_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif


struct dabInputFileData {
    int file;
    bool parity;
    unsigned packetLength;
    unsigned char* packetData;
    unsigned char** enhancedPacketData;
    unsigned enhancedPacketLength;
    unsigned enhancedPacketWaiting;
};


int dabInputFileInit(void** args);
int dabInputFileOpen(void* args, const char* filename);
int dabInputFileRead(void* args, void* buffer, int size);
int dabInputFileClose(void* args);
int dabInputFileClean(void** args);
int dabInputFileRewind(void* args);


#endif // DAB_INPUT_FILE_H
