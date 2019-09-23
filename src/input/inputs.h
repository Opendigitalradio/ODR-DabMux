/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
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
#   include "config.h"
#endif
#include "Log.h"
#include "RemoteControl.h"
#include <string>

namespace Inputs {

enum class BufferManagement {
    // Use a buffer in the input that doesn't consider timestamps
    Prebuffering,

    // Buffer incoming data until a given timestamp is reached
    Timestamped,
};


/* New input object base */
class InputBase {
    public:
        /* Throws runtime_error or invalid_argument on failure */
        virtual void open(const std::string& name) = 0;

        /* read a frame from the input. Buffer management is either not necessary
         * (e.g. File input) or done with pre-buffering (network-based inputs).
         *
         * This ignores timestamps. All inputs support this.
         *
         * Returns number of data bytes written to the buffer. May clear the buffer
         * if no data bytes available, in which case it will return 0.
         *
         * Returns negative on error.
         */
        virtual size_t readFrame(uint8_t *buffer, size_t size) = 0;

        /* read a frame from the input, taking into account timestamp. The timestamp of the data
         * returned is not more recent than the timestamp specified in seconds and tsta.
         *
         * seconds is in UNIX epoch, utco is the TAI-UTC offset, tsta is in the format used by ETI.
         *
         * Returns number of data bytes written to the buffer. May clear the buffer
         * if no data bytes available, in which case it will return 0.
         *
         * Returns negative on error.
         *
         * Calling this function on inputs that do not support timestamps returns 0. This allows
         * changing the buffer management at runtime without risking an crash due to an exception.
         */
        virtual size_t readFrame(uint8_t *buffer, size_t size, std::time_t seconds, int utco, uint32_t tsta) = 0;

        /* Returns the effectively used bitrate, or throws invalid_argument on invalid bitrate */
        virtual int setBitrate(int bitrate) = 0;
        virtual void close() = 0;

        virtual ~InputBase() {}

        void setBufferManagement(BufferManagement bm) { m_bufferManagement = bm; }
        BufferManagement getBufferManagement() const { return m_bufferManagement; }

    protected:
        InputBase() {}

        std::atomic<BufferManagement> m_bufferManagement = ATOMIC_VAR_INIT(BufferManagement::Prebuffering);
};

};

