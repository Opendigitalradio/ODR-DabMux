/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in Right
   of Canada (Communications Research Center Canada)
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

#include "Interleaver.h"

#include <string.h>

#ifdef _WIN32
#   define bzero(a, b) memset((a), 0, (b))
#endif // _WIN32


Interleaver::Interleaver(unsigned short I, unsigned short M, bool reverse)
{
    this->I = I;
    this->M = M;
    this->N = I * M;
    this->memSize = (this->N * I) - 1;
    this->mem = new char[memSize];
    setReverse(reverse);
}


Interleaver::~Interleaver()
{
    delete []mem;
}


void Interleaver::setReverse(bool state)
{
    reverse = state;
    reset();
}


void Interleaver::reset()
{
    j = 0;
    index = 0;
    bzero(mem, memSize);
}


void Interleaver::encode(void* data, unsigned long size)
{
    encode(data, data, size);
}


void Interleaver::encode(const void* inData, void* outData, unsigned long size)
{
    const char* input = reinterpret_cast<const char *>(inData);
    char* output = reinterpret_cast<char *>(outData);
    unsigned long i;

    if (reverse) {
        for (i = 0; i < size; ++i) {
            mem[(index + ((I - 1 - j) * N)) % memSize] = *input;
            *output = mem[index];
            ++input;
            ++output;
            if (++j == I) {
                j = 0;
            }
            if (++index == memSize) {
                index = 0;
            }
        }
    } else {
        for (i = 0; i < size; ++i) {
            if (j) {
                mem[(index + (j * N)) % memSize] = *input;
                *output = mem[index];
            } else {
                *output = *input;
            }
            ++input;
            ++output;
            if (++j == I) {
                j = 0;
            }
            if (++index == memSize) {
                index = 0;
            }
        }
    }
}


unsigned long Interleaver::sync(void* data, unsigned long size, char padding)
{
    char* input = reinterpret_cast<char *>(data);
    unsigned long index;

    if (reverse) {
        for (index = 0; index < size; ++index) {
            mem[(index + ((I - 1 - j) * N)) % memSize] = padding;
            *input = mem[index];
            ++input;
            if (++index == memSize) {
                index = 0;
            }
            if (++j == I) {
                j = 0;
                break;
            }
        }
    } else {
        for (index = 0; index < size; ++index) {
            mem[(index + (j * N)) % memSize] = padding;
            *input = mem[index];
            ++input;
            if (++index == memSize) {
                index = 0;
            }
            if (++j == I) {
                j = 0;
                break;
            }
        }
    }

    return index;
}
