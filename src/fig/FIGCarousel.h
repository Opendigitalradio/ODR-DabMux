/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

   Implementation of the FIG carousel to schedule the FIGs into the
   FIBs.
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

#ifndef __FIG_CAROUSEL_H_
#define __FIG_CAROUSEL_H_

#include "fig/FIG.h"
#include "fig/FIG0.h"
#include "fig/FIG1.h"
#include <list>
#include <map>
#include <memory>
#include "MuxElements.h"

namespace FIC {

struct FIGCarouselElement {
    IFIG* fig;
    int   deadline; // unit: ms

    void reduce_deadline(void);

    void increase_deadline(void);
};

enum class FIBAllocation {
    FIB0,
    FIB1,
    FIB2,
    FIB3,
    FIB_ANY
};

class FIGCarousel {
    public:
        FIGCarousel(std::shared_ptr<dabEnsemble> ensemble);

        void update(unsigned long currentFrame);

        void allocate_fig_to_fib(int figtype, int extension, FIBAllocation fib);

        /* Write all FIBs to the buffer, including correct padding and crc.
         * Returns number of bytes written.
         *
         * The buffer buf must be large enough to accomodate the FIBs, i.e.
         * 32 bytes per FIB.
         */
        size_t write_fibs(
                uint8_t *buf,
                int framephase,
                bool fib3_present);

    private:
        size_t carousel(int fib, uint8_t *buf, size_t bufsize, int framephase);

        void load_and_allocate(IFIG& fig, FIBAllocation fib);

        FIGRuntimeInformation m_rti;
        std::map<std::pair<int, int>, IFIG*> m_figs_available;

        // Some FIGs can be mapped to a specific FIB or to FIB_ANY
        std::map<FIBAllocation, std::list<FIGCarouselElement> > m_fibs;

        // See in ctor for allocation to FIBs
        FIG0_0 m_fig0_0;
        FIG0_1 m_fig0_1;
        FIG0_2 m_fig0_2;
        FIG0_3 m_fig0_3;
        FIG0_5 m_fig0_5;
        FIG0_6 m_fig0_6;
        FIG0_17 m_fig0_17;
        FIG0_8 m_fig0_8;
        FIG1_0 m_fig1_0;
        FIG0_13 m_fig0_13;
        FIG0_10 m_fig0_10;
        FIG0_9 m_fig0_9;
        FIG1_1 m_fig1_1;
        FIG1_4 m_fig1_4;
        FIG1_5 m_fig1_5;
        FIG0_18 m_fig0_18;
        FIG0_19 m_fig0_19;
};

} // namespace FIC

#endif // __FIG_CAROUSEL_H_

