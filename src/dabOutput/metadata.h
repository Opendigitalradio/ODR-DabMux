/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   The metadata support for the outputs.
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
#include "Log.h"

/* Some outputs support additional metadata to be carried
 * next to the main ETI stream. This metadata always has the
 * following format:
 *   Field:  | id | len      | value               |
 *   Length:   1    2          depending on id
 *
 * Multi-byte values are transmitted in network byte order.
 */

enum class output_metadata_id_e {
    // TAI-UTC offset, value is int16_t.
    utc_offset = 1,

    /* EDI Time is the number of SI seconds since 2000-01-01 T 00:00:00 UTC.
     * value is an uint32_t */
    edi_time = 2,
};

struct OutputMetadata {
    virtual output_metadata_id_e getId(void) const = 0;
    virtual size_t getLength(void) const = 0;

    /* Write the value in the metadata format to the buffer.
     * Returns number of bytes written. The buffer buf needs
     * to be large enough to contain the value;
     */
    virtual size_t write(uint8_t *buf) = 0;
};

struct OutputMetadataUTCO : public OutputMetadata {
    explicit OutputMetadataUTCO(int16_t utco) : utco(utco) {}
    output_metadata_id_e getId(void) const { return output_metadata_id_e::utc_offset; }
    virtual size_t getLength(void) const { return 5; }
    virtual size_t write(uint8_t *buf);

    int16_t utco;
};

struct OutputMetadataEDITime : public OutputMetadata {
    explicit OutputMetadataEDITime(uint32_t seconds) : seconds(seconds) {}
    output_metadata_id_e getId(void) const { return output_metadata_id_e::edi_time; }
    virtual size_t getLength(void) const { return 7; }
    virtual size_t write(uint8_t *buf);

    uint32_t seconds;

};

