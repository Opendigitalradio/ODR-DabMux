/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016 Matthias P. Braendli
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

#include <vector>
#include <string>
#include <stdint.h>
#include "input/inputs.h"
#include "ManagementServer.h"

namespace Inputs {

class FileBase : public InputBase {
    public:
        virtual int open(const std::string& name);
        virtual int readFrame(uint8_t* buffer, size_t size) = 0;
        virtual int setBitrate(int bitrate) = 0;
        virtual int close();

        /* Rewind the file
         * Returns -1 on failure, 0 on success
         */
        virtual int rewind();
    protected:
        // We use unix open() instead of fopen() because
        // we want to do non-blocking I/O
        int m_fd = -1;
};

class MPEGFile : public FileBase {
    public:
        virtual int readFrame(uint8_t* buffer, size_t size);
        virtual int setBitrate(int bitrate);

    private:
        bool m_parity = false;
};

class DABPlusFile : public FileBase {
    public:
        virtual int readFrame(uint8_t* buffer, size_t size);
        virtual int setBitrate(int bitrate);
};


};
