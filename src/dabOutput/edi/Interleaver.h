/*
   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   Interleaving of PFT fragments to increase robustness against
   burst packet loss.

   This is possible because EDI has to assume that fragments may reach
   the receiver out of order.

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

#include "config.h"
#include <vector>
#include <deque>
#include <stdexcept>
#include <stdint.h>
#include "Log.h"
#include "PFT.h"

namespace edi {

class Interleaver {
    public:
        using fragment_vec = std::vector<PFTFragment>;

        /* Configure the interleaver to use latency_frames number of AF
         * packets for interleaving. Total delay through the interleaver
         * will be latency_frames * 24ms
         */
        void SetLatency(size_t latency_frames);

        /* Move the fragments for an AF Packet into the interleaver and
         * return interleaved fragments to be transmitted.
         */
        fragment_vec Interleave(fragment_vec &fragments);

    private:
        size_t m_latency = 0;
        size_t m_fragment_count = 0;
        size_t m_interleave_offset = 0;
        size_t m_stride = 0;
        std::deque<fragment_vec> m_buffer;
};

}

