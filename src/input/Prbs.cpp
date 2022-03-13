/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   Pseudo-Random Bit Sequence generator for test purposes.
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

#include "input/Prbs.h"

#include <stdexcept>
#include <sstream>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include "utils.h"

using namespace std;

namespace Inputs {

// ETS 300 799 Clause G.2.1
// Preferred polynomial is G(x) = x^20 + x^17 + 1
const uint32_t PRBS_DEFAULT_POLY = (1 << 20) | (1 << 17) | (1 << 0);

void Prbs::open(const string& name)
{
    if (name.substr(0, 7) != "prbs://") {
        throw logic_error("Invalid PRBS name");
    }

    const string& url_polynomial = name.substr(7);

    if (url_polynomial.empty()) {
        m_prbs.setup(PRBS_DEFAULT_POLY);
    }
    else {
        if (url_polynomial[0] != ':') {
            throw invalid_argument(
                    "Invalid PRBS address format. "
                    "Must be prbs://:polynomial.");
        }

        const string poly_str = url_polynomial.substr(1);

        long polynomial = hexparse(poly_str);

        if (polynomial == 0) {
            throw invalid_argument("No polynomial given for PRBS input");
        }

        m_prbs.setup(polynomial);
    }
    rewind();
}

size_t Prbs::readFrame(uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = m_prbs.step();
    }

    return size;
}

size_t Prbs::readFrame(uint8_t *buffer, size_t size, std::time_t seconds, int utco, uint32_t tsta)
{
    memset(buffer, 0, size);
    return 0;
}

int Prbs::setBitrate(int bitrate)
{
    if (bitrate <= 0) {
        throw invalid_argument("Invalid bitrate " + to_string(bitrate));
    }
    return bitrate;
}

void Prbs::close()
{
}

int Prbs::rewind()
{
    m_prbs.rewind();
    return 0;
}

};
