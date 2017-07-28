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

#include "dabOutput/metadata.h"
#include <cstring>
#include <arpa/inet.h>

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

template <typename T>
size_t write_meta(output_metadata_id_e md, uint8_t *buf, T value)
{
    buf[0] = static_cast<uint8_t>(md);

    const int16_t len_value = sizeof(T);

    const uint16_t data_length = htons(len_value);
    memcpy(buf + 1, &data_length, sizeof(data_length));

    if (len_value == 1) {
        buf[3] = value;
    }
    else if (len_value == 2) {
        const uint16_t value = htons(value);
        memcpy(buf + 3, &value, sizeof(value));
    }
    else if (len_value == 4) {
        const uint32_t value = htons(value);
        memcpy(buf + 3, &value, sizeof(value));
    }

    return 3 + len_value;
}

size_t OutputMetadataUTCO::write(uint8_t *buf)
{
    return write_meta(getId(), buf, utco);
}

size_t OutputMetadataEDITime::write(uint8_t *buf)
{
    return write_meta(getId(), buf, seconds);
}
