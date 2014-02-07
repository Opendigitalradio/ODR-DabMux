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

#ifndef DAB_INPUT_RAW_FIFO_H
#define DAB_INPUT_RAW_FIFO_H


#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "dabInput.h"

#include <stdint.h>


#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_FILE


extern struct dabInputOperations dabInputRawFifoOperations;

int dabInputRawFifoInit(void** args);
int dabInputRawFifoOpen(void* args, const char* filename);
int dabInputRawFifoSetbuf(void* args, int size);
int dabInputRawFifoRead(void* args, void* buffer, int size);
int dabInputRawFifoReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size);
int dabInputRawFifoClose(void* args);
int dabInputRawFifoClean(void** args);
int dabInputRawFifoRewind(void* args);


struct dabInputRawFifoData {
    int file;
    uint8_t* buffer;
    size_t bufferSize;
    size_t bufferOffset;
};


#   endif
#endif

#endif // DAB_INPUT_RAW_FIFO_H
