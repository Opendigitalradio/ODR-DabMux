/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#include "dabInputPrbs.h"

#include <string.h>
#include <limits.h>
#include <stdlib.h>


#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_PRBS


struct dabInputOperations dabInputPrbsOperations = {
    dabInputPrbsInit,
    dabInputPrbsOpen,
    NULL,
    dabInputPrbsRead,
    NULL,
    NULL,
    dabInputPrbsReadFrame,
    dabInputPrbsBitrate,
    dabInputPrbsClose,
    dabInputPrbsClean,
    dabInputPrbsRewind,
};


int dabInputPrbsInit(void** args)
{
    prbs_data* data = new prbs_data;

    memset(data, 0, sizeof(*data));

    *args = data;
    return 0;
}


int dabInputPrbsOpen(void* args, const char* name)
{
    prbs_data* data = (prbs_data*)args;

    if (*name != ':') {
        etiLog.log(error,
                "Sorry, PRBS address format is prbs://:polynomial.\n");
        errno = EINVAL;
        return -1;
    }

    long polynomial = strtol(++name, (char **)NULL, 10);
    if ((polynomial == LONG_MIN) || (polynomial == LONG_MAX)) {
        etiLog.log(error, "can't convert polynomial number %s\n", name);
        errno = EINVAL;
        return -1;
    }
    if (polynomial == 0) {
        etiLog.log(error, "you must specify a polynomial number\n");
        errno = EINVAL;
        return -1;
    }
    
    data->polynomial = polynomial;
    data->accum = 0;
    gen_prbs_table(data);
    gen_weight_table(data);
    dabInputPrbsRewind(args);

    return 0;
}


int dabInputPrbsRead(void* args, void* buffer, int size)
{
    prbs_data* data = (prbs_data*)args;
	unsigned char* cbuffer = reinterpret_cast<unsigned char*>(buffer);

    for(int i = 0; i < size; ++i) {
        data->accum = update_prbs(args);
        *(cbuffer++) = (unsigned char)(data->accum & 0xff);
    }

    return size;
}


int dabInputPrbsReadFrame(dabInputOperations* ops, void* args,
        void* buffer, int size)
{
    return ops->read(args, buffer, size);
}


int dabInputPrbsBitrate(dabInputOperations* ops, void* args, int bitrate)
{
    return bitrate;
}


int dabInputPrbsClose(void* args)
{
    return 0;
}


int dabInputPrbsClean(void** args)
{
    delete (prbs_data*)(*args);
    return 0;
}


int dabInputPrbsRewind(void* args)
{
    prbs_data* data = (prbs_data*)args;

    while (data->accum < data->polynomial) {
        data->accum <<= 1;
        data->accum |= 1;
    }

    return 0;
}


#   endif
#endif
