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

#pragma once

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

class PrbsGenerator {
    public:
        void setup(uint32_t polynomial);

        uint8_t step(void);

        void rewind(void);

    private:
        /* Generate a table of matrix products to update a 32-bit PRBS
         * generator. */
        void gen_prbs_table(void);

        /* Update a 32-bit PRBS generator eight bits at a time. */
        uint32_t update_prbs(void);

        /* Generate the weight table. */
        void gen_weight_table(void);

        /* Count the number of errors in a block of received data. */
        size_t error_count(
                uint8_t *rx_data,
                size_t rx_data_length);

        void gen_sequence(
                uint8_t *tx_data,
                size_t tx_data_length,
                uint32_t polynomial);

        // table of matrix products used to update a 32-bit PRBS generator
        uint32_t prbs_table [4] [256];
        // table of weights for 8-bit bytes
        uint8_t weight[256];
        // PRBS polynomial generator
        uint32_t polynomial;
        // PRBS generator polarity mask
        uint8_t polarity_mask;
        // PRBS accumulator
        uint32_t accum;
};

