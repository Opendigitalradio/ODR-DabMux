/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

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

#ifndef __FIG_H_
#define __FIG_H_

#include <memory>
#include "MuxElements.h"

namespace FIC {

#define PACKED __attribute__ ((packed))

class FIGRuntimeInformation {
    public:
        FIGRuntimeInformation(std::shared_ptr<dabEnsemble> e) :
            currentFrame(0),
            ensemble(e),
            factumAnalyzer(false) {}

        time_t date;
        unsigned long currentFrame;
        std::shared_ptr<dabEnsemble> ensemble;
        bool factumAnalyzer;
};

// Recommended FIG rates according to ETSI TR 101 496-2 Table 3.6.1
enum class FIG_rate {
    FIG0_0, /* Special repetition rate for FIG0/0, EN 300 401 Clause 6.4
        In any 96 ms period, the FIG 0/0 should be transmitted in a fixed time
        position. In transmission mode I, this should be the first FIB (of the three)
        associated with the first CIF (of the four) in the transmission frame (see
        clause 5.1). In transmission modes II and III, this should be the first FIB of
        every fourth transmission frame. In transmission mode IV, this should be the
        first FIB (of the three) associated with the first CIF (of the two) in every
        alternate transmission frame (see clause 5.1). */
    A,   // at least 10 times per second
    A_B, // between 10 times and once per second
    B,   // once per second
    C,   // once every 10 seconds
    D,   // less than once every 10 seconds
    E,   // all in two minutes
};

/* Helper function to calculate the deadline for the next transmission, in milliseconds */
int rate_increment_ms(FIG_rate rate);

/* The fill function of each FIG shall return a status telling
 * the carousel how many bytes have been written, and if the complete
 * set of information from that FIG was transmitted.
 */
struct FillStatus
{
    FillStatus() :
        num_bytes_written(0),
        complete_fig_transmitted(false) {}
    size_t num_bytes_written;
    bool   complete_fig_transmitted;
};

class IFIG
{
    public:
        virtual FillStatus fill(uint8_t *buf, size_t max_size) = 0;

        virtual FIG_rate repetition_rate(void) = 0;

        virtual const int figtype(void) const = 0;
        virtual const int figextension(void) const = 0;

        virtual const std::string name(void) const
        {
            std::stringstream ss;
            ss << figtype() << "/" << figextension();
            return ss.str();
        }

};

} // namespace FIC

#endif // __FIG_H_

