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

#ifndef DAB_INPUT_DABPLUS_FILE_H
#define DAB_INPUT_DABPLUS_FILE_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdint.h>
#include <unistd.h>


#ifdef HAVE_FORMAT_DABPLUS
#   ifdef HAVE_INPUT_FILE


extern struct dabInputOperations dabInputDabplusFileOperations;


struct dabInputDabplusFileData {
    int file;
    uint8_t* buffer;
    size_t bufferSize;
    size_t bufferIndex;
};


int dabInputDabplusFileInit(void** args);
int dabInputDabplusFileOpen(void* args, const char* filename);
int dabInputDabplusFileRead(void* args, void* buffer, int size);
int dabInputDabplusFileReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size);
int dabInputDabplusFileClose(void* args);
int dabInputDabplusFileClean(void** args);
int dabInputDabplusFileRewind(void* args);


#   endif
#endif


#endif // DAB_INPUT_DABPLUS_FILE_H
