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
#include <list>
#include <map>
#include <boost/shared_ptr.hpp>
#include "MuxElements.h"

struct FIGCarouselElement {
    IFIG* fig;
    int   deadline;

    void reduce_deadline(void);
};

class FIGCarousel {
    public:
        FIGCarousel(boost::shared_ptr<dabEnsemble> ensemble);

        void set_currentFrame(unsigned long currentFrame);

        void allocate_fig_to_fib(int figtype, int extension, int fib);

        void fib0(int framephase);

    private:
        FIGRuntimeInformation m_rti;
        std::map<std::pair<int, int>, IFIG*> m_figs_available;

        // Each FIB contains a list of carousel elements
        std::map<int, std::list<FIGCarouselElement> > m_fibs;

        FIG0_0 m_fig0_0;
        FIG0_1 m_fig0_1;
        FIG0_2 m_fig0_2;
        FIG0_3 m_fig0_3;
};

#endif // __FIG_CAROUSEL_H_

