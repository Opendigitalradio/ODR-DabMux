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

#pragma once

#include <string>

#include "input/inputs.h"
#include "PrbsGenerator.h"

namespace Inputs {

class Prbs : public InputBase {
    public:
        virtual void open(const std::string& name);
        virtual size_t readFrame(uint8_t *buffer, size_t size);
        virtual size_t readFrame(uint8_t *buffer, size_t size, std::time_t seconds, int utco, uint32_t tsta);
        virtual int setBitrate(int bitrate);
        virtual void close();

    private:
        virtual int rewind();

        PrbsGenerator m_prbs;
};

};

