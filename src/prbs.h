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

#ifndef _PRBS
#define _PRBS

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif


#ifdef __cplusplus
extern "C" { // }
#endif

typedef struct {
    // table of matrix products used to update a 32-bit PRBS generator
    unsigned long prbs_table [4] [256];
    // table of weights for 8-bit bytes
    unsigned char weight[256];
    // PRBS polynomial generator
    unsigned long polynomial;
    // PRBS generator polarity mask
    unsigned char polarity_mask;
    // PRBS accumulator
    unsigned long accum;
} prbs_data;


unsigned long parity_check(unsigned long prbs_accum);
void gen_prbs_table(void* args);
unsigned long update_prbs(void* args);
void gen_weight_table(void* args);
unsigned long error_count(void* args, unsigned char *rx_data,
        int rx_data_length);
void gen_sequence(void* args, unsigned char *tx_data, int tx_data_length,
        unsigned long polynomial);

#ifdef __cplusplus
}
#endif

#endif // _PRBS
