/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in Right
   of Canada (Communications Research Center Canada)
   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#include "prbs.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>


/*
 * Generate a parity check for a 32-bit word.
 */
unsigned long parity_check(unsigned long prbs_accum)
{
    unsigned long mask=1UL, parity=0UL;
    int i;
    for (i = 0;  i < 32;  ++i) {
        parity ^= ((prbs_accum & mask) != 0UL);
        mask <<= 1;
    }
    return parity;
}

/*
 * Generate a table of matrix products to update a 32-bit PRBS generator.
 */
void gen_prbs_table(void* args)
{
    prbs_data* data = (prbs_data*)args;
    int i;
    for (i = 0;  i < 4;  ++i) {
        int j;
        for (j = 0;  j < 256;  ++j) {
            unsigned long prbs_accum = ((unsigned long)j << (i * 8));
            int k;
            for (k = 0;  k < 8;  ++k) {
                prbs_accum = (prbs_accum << 1)
                                ^ parity_check(prbs_accum & data->polynomial);
            }
            data->prbs_table[i][j] = (prbs_accum & 0xff);
        }
    }
}

/*
 * Update a 32-bit PRBS generator eight bits at a time.
 */
unsigned long update_prbs(void* args)
{
    prbs_data* data = (prbs_data*)args;
    unsigned char acc_lsb = 0;
    int i;
    for (i = 0;  i < 4;  ++i ) {
        acc_lsb ^= data->prbs_table [i] [ (data->accum >> (i * 8) ) & 0xff ];
    }
    return (data->accum << 8) ^ ((unsigned long)acc_lsb);
}

/*
 * Generate the weight table.
 */
void gen_weight_table(void* args)
{
    prbs_data* data = (prbs_data*)args; 
    int i;
    for (i = 0;  i < 256;  ++i) {
        unsigned char mask=1U, ones_count = 0U;
        int j;
        for (j = 0;  j < 8;  ++j) {
            ones_count += ((i & mask) != 0U);
            mask = mask << 1;
        }
        data->weight[i] = ones_count;
    }
}

/*
 * Count the number of errors in a block of received data.
 */
unsigned long error_count(void* args, unsigned char *rx_data,
        int rx_data_length)
{
    prbs_data* data = (prbs_data*)args;
    unsigned long error_count = 0U;
    unsigned long prbs_accum = 0U;
    int i;

    /* seed the PRBS accumulator */
    for (i = 0;  i < 4;  ++i) {
        prbs_accum = (prbs_accum << 8) ^ (rx_data[i] ^ data->polarity_mask);
    }

    /* check the received data */
    for (i = 0;  i < rx_data_length;  ++i) {
        unsigned char error_pattern = (unsigned char)
                          ((prbs_accum >> 24)
                          ^ (rx_data[i] ^ data->polarity_mask));
        if (error_pattern != 0U) {
            error_count += data->weight[error_pattern];
        }
        prbs_accum = update_prbs(data);
    }
    return error_count;
}

void gen_sequence(void* args, unsigned char *tx_data, int tx_data_length,
        unsigned long polynomial)
{
    prbs_data* data = (prbs_data*)args;
    unsigned long prbs_accum = 0U;

    while (prbs_accum < polynomial) {
        prbs_accum <<= 1;
        prbs_accum |= 1;
    }

    while (tx_data_length-- > 0) {
        prbs_accum = update_prbs(data);
        *(tx_data++) = (unsigned char)(prbs_accum & 0xff);
    }
}
