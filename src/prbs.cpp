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

#include "prbs.h"

#include <string.h>
#include <limits.h>


/*
 * Generate a parity check for a 32-bit word.
 */
static uint32_t parity_check(uint32_t prbs_accum)
{
    uint32_t mask = 1;
    uint32_t parity = 0;
    for (int i = 0; i < 32; ++i) {
        parity ^= ((prbs_accum & mask) != 0);
        mask <<= 1;
    }
    return parity;
}

void PrbsGenerator::setup(uint32_t polynomial)
{
    this->polynomial = polynomial;
    this->accum = 0;
    gen_prbs_table();
    gen_weight_table();
}

uint8_t PrbsGenerator::step()
{
    accum = update_prbs();
    return accum & 0xff;
}

void PrbsGenerator::rewind()
{
    while (accum < polynomial) {
        accum <<= 1;
        accum |= 1;
    }
}

void PrbsGenerator::gen_prbs_table()
{
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 256; ++j) {
            uint32_t prbs_accum = ((uint32_t)j << (i * 8));
            for (int k = 0; k < 8; ++k) {
                prbs_accum = (prbs_accum << 1)
                    ^ parity_check(prbs_accum & polynomial);
            }

            prbs_table[i][j] = (prbs_accum & 0xff);
        }
    }
}

uint32_t PrbsGenerator::update_prbs()
{
    uint8_t acc_lsb = 0;
    for (int i = 0; i < 4; ++i ) {
        acc_lsb ^= prbs_table [i] [ (accum >> (i * 8) ) & 0xff ];
    }
    return (accum << 8) ^ ((uint32_t)acc_lsb);
}

void PrbsGenerator::gen_weight_table()
{
    for (int i = 0; i < 256; ++i) {
        uint8_t mask = 1;
        uint8_t ones_count = 0;

        for (int j = 0; j < 8; ++j) {
            ones_count += ((i & mask) != 0);
            mask = mask << 1;
        }
        weight[i] = ones_count;
    }
}

size_t PrbsGenerator::error_count(
        uint8_t *rx_data,
        size_t rx_data_length)
{
    uint32_t error_count = 0;
    uint32_t prbs_accum = 0;

    /* seed the PRBS accumulator */
    for (int i = 0; i < 4; ++i) {
        prbs_accum = (prbs_accum << 8) ^ (rx_data[i] ^ polarity_mask);
    }

    /* check the received data */
    for (size_t i = 0; i < rx_data_length; ++i) {
        uint8_t error_pattern =
            (prbs_accum >> 24) ^ (rx_data[i] ^ polarity_mask);

        if (error_pattern != 0) {
            error_count += weight[error_pattern];
        }
        prbs_accum = update_prbs();
    }
    return error_count;
}

void PrbsGenerator::gen_sequence(
        uint8_t *tx_data,
        size_t tx_data_length,
        uint32_t polynomial)
{
    uint32_t prbs_accum = 0;

    while (prbs_accum < polynomial) {
        prbs_accum <<= 1;
        prbs_accum |= 1;
    }

    for (size_t i = 0; i < tx_data_length; i++) {
        prbs_accum = update_prbs();
        tx_data[i] = (uint8_t)(prbs_accum & 0xff);
    }
}
