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

#include "dabInputTest.h"

#include <string.h>
#ifdef _WIN32
#else
#   include <arpa/inet.h>
#endif


#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_TEST


struct dabInputTestData {
    unsigned long counter;
};


struct dabInputOperations dabInputTestOperations = {
    dabInputTestInit,
    dabInputTestOpen,
    NULL,
    NULL,
    NULL,
    NULL,
    dabInputTestRead,
    dabInputTestSetbitrate,
    dabInputTestClose,
    dabInputTestClean,
    NULL
};


int dabInputTestInit(void** args)
{
    dabInputTestData* input = new dabInputTestData;
    memset(input, 0, sizeof(*input));
    input->counter = 0;
    *args = input;
    return 0;
}


int dabInputTestOpen(void* args, const char* inputName)
{
    return 0;
}


int dabInputTestRead(dabInputOperations* ops, void* args, void* buffer, int size)
{
    dabInputTestData* input = (dabInputTestData*)args;
    char* data = (char*)buffer;

    *((long*)buffer) = htonl(input->counter++);
    for (int i = sizeof(input->counter); i < size; ++i) {
        data[i] = i;
    }
    return size;
}


int dabInputTestSetbitrate(dabInputOperations* ops, void* args, int bitrate)
{
    return bitrate;
}


int dabInputTestClose(void* args)
{
    return 0;
}


int dabInputTestClean(void** args)
{
    dabInputTestData* input = (dabInputTestData*)(*args);
    delete input;
    return 0;
}


#   endif
#endif
