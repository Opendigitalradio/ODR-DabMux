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

#ifndef _INTERLEAVER
#define _INTERLEAVER

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <vector>

class Interleaver
{
public:
    Interleaver(unsigned short I, unsigned short M, bool reverse = false);

    void setReverse(bool state);
    void encode(void* data, unsigned long size);
    void encode(const void* inData, void* outData, unsigned long size);
    unsigned long sync(void* data, unsigned long size, char padding = 0);
    void reset();
    void flush(char padding = 0);

private:
    unsigned short I;
    unsigned short M;
    unsigned long N;
    unsigned long j;
    unsigned long index;
    unsigned long memSize;
    std::vector<char> mem;
    bool reverse;
};


#endif // _INTERLEAVER
