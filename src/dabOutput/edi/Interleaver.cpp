/*
   Copyright (C) 2017
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

#include "Interleaver.h"

namespace edi {

void Interleaver::SetLatency(size_t latency_frames)
{
    m_latency = latency_frames;
}

Interleaver::fragment_vec Interleaver::Interleave(fragment_vec &fragments)
{
    m_fragment_count = fragments.size();

    // Create vectors containing Fcount*latency fragments in total
    // and store them into the deque
    if (m_buffer.empty()) {
        m_buffer.emplace_back();
    }

    auto& last_buffer = m_buffer.back();

    for (auto& fragment : fragments) {
        const bool last_buffer_is_complete =
                (last_buffer.size() >= m_fragment_count * m_latency);

        if (last_buffer_is_complete) {
            m_buffer.emplace_back();
            last_buffer = m_buffer.back();
        }

        last_buffer.push_back(std::move(fragment));
    }

    fragments.clear();

    while ( not m_buffer.empty() and
            (m_buffer.front().size() >= m_fragment_count * m_latency)) {

        auto& first_buffer = m_buffer.front();

        assert(first_buffer.size() == m_fragment_count * m_latency);

        /* Assume we have 5 fragments per AF frame, and latency of 3.
         * This will give the following strides:
         *    0        1     2
         * +-------+-------+---+
         * | 0   1 | 2   3 | 4 |
         * |       |   +---+   |
         * | 5   6 | 7 | 8   9 |
         * |   +---+   |       |
         * |10 |11  12 |13  14 |
         * +---+-------+-------+
         *
         * ix will be 0, 5, 10, 1, 6 in the first loop
         */

        for (size_t i = 0; i < m_fragment_count; i++) {
            const size_t ix = m_interleave_offset + m_fragment_count * m_stride;
            m_interleaved_fragments.push_back(first_buffer.at(ix));

            m_stride += 1;
            if (m_stride >= m_latency) {
                m_interleave_offset++;
                m_stride = 0;
            }
        }

        if (m_interleave_offset >= m_fragment_count) {
            m_interleave_offset = 0;
            m_stride = 0;
            m_buffer.pop_front();
        }
    }

    std::vector<PFTFragment> interleaved_frags;

    const size_t n = std::min(m_fragment_count, m_interleaved_fragments.size());
    std::move(m_interleaved_fragments.begin(),
              m_interleaved_fragments.begin() + n,
              std::back_inserter(interleaved_frags));
    m_interleaved_fragments.erase(
              m_interleaved_fragments.begin(),
              m_interleaved_fragments.begin() + n);

    return interleaved_frags;
}

}


