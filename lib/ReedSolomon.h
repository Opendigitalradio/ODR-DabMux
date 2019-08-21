/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in Right
   of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#pragma once

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

class ReedSolomon
{
public:
    ReedSolomon(int N, int K,
            bool reverse = false,
            int gfpoly = 0x11d, int firstRoot = 0, int primElem = 1);
    ReedSolomon(const ReedSolomon& other) = delete;
    ReedSolomon operator=(const ReedSolomon& other) = delete;
    ~ReedSolomon();

    void setReverse(bool state);
    int encode(void* data, void* fec, size_t size);
    int encode(void* data, size_t size);

private:
    int m_N;
    int m_K;

    void* rsData;
    bool reverse;
};

