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

#include "ReedSolomon.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <stdio.h>          // For galois.h ...
#include <string.h>         // For memcpy
#include <cstdint>

extern "C" {
#include "fec/fec.h"
}
#include <assert.h>

#define SYMSIZE     8


ReedSolomon::ReedSolomon(int N, int K, bool reverse, int gfpoly, int firstRoot, int primElem)
{
    setReverse(reverse);

    m_N = N;
    m_K = K;

    const int symsize = SYMSIZE;
    const int nroots = N - K; // For EDI PFT, this must be 48
    const int pad = ((1 << symsize) - 1) - N; // is 255-N

    rsData = init_rs_char(symsize, gfpoly, firstRoot, primElem, nroots, pad);

    if (rsData == nullptr) {
        std::stringstream ss;
        ss << "Invalid Reed-Solomon parameters! " <<
            "N=" << N << " ; K=" << K << " ; pad=" << pad;
        throw std::invalid_argument(ss.str());
    }
}


ReedSolomon::~ReedSolomon()
{
    free_rs_char(rsData);
}


void ReedSolomon::setReverse(bool state)
{
    reverse = state;
}


int ReedSolomon::encode(void* data, void* fec, size_t size)
{
    uint8_t* input = reinterpret_cast<uint8_t*>(data);
    uint8_t* output = reinterpret_cast<uint8_t*>(fec);
    int ret = 0;

    if (reverse) {
        std::vector<uint8_t> buffer(m_N);

        memcpy(&buffer[0], input, m_K);
        memcpy(&buffer[m_K], output, m_N - m_K);

        ret = decode_rs_char(rsData, &buffer[0], nullptr, 0);
        if ((ret != 0) && (ret != -1)) {
            memcpy(input, &buffer[0], m_K);
            memcpy(output, &buffer[m_K], m_N - m_K);
        }
    }
    else {
        encode_rs_char(rsData, input, output);
    }

    return ret;
}


int ReedSolomon::encode(void* data, size_t size)
{
    uint8_t* input = reinterpret_cast<uint8_t*>(data);
    int ret = 0;

    if (reverse) {
        ret = decode_rs_char(rsData, input, nullptr, 0);
    }
    else {
        encode_rs_char(rsData, input, &input[m_K]);
    }

    return ret;
}
