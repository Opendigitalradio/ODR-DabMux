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

#include "fig/FIGCarousel.h"
#include <iostream>

/**************** FIGCarouselElement ****************/
void FIGCarouselElement::reduce_deadline()
{
    deadline -= rate_increment_ms(fig->repetition_rate());

    if (deadline < 0) {
        std::cerr << "FIG " << fig->name() <<
            "has negative scheduling deadline" << std::endl;
    }
}


/**************** FIGCarousel *****************/

FIGCarousel::FIGCarousel(boost::shared_ptr<dabEnsemble> ensemble) :
    m_fig0_0(&m_rti),
    m_fig0_1(&m_rti),
    m_fig0_2(&m_rti),
    m_fig0_3(&m_rti),
    m_fig0_17(&m_rti)
{
    m_rti.ensemble = ensemble;
    m_rti.currentFrame = 0;
    m_rti.factumAnalyzer = false;

    m_figs_available[std::make_pair(0, 0)] = &m_fig0_0;
    m_figs_available[std::make_pair(0, 1)] = &m_fig0_1;
    m_figs_available[std::make_pair(0, 2)] = &m_fig0_2;
    m_figs_available[std::make_pair(0, 3)] = &m_fig0_3;
    m_figs_available[std::make_pair(0, 17)] = &m_fig0_17;

    const int fib0 = 0;
    allocate_fig_to_fib(0, 0, fib0);
    allocate_fig_to_fib(0, 1, fib0);
    allocate_fig_to_fib(0, 2, fib0);
    allocate_fig_to_fib(0, 3, fib0);
    allocate_fig_to_fib(0, 17, fib0);
}

void FIGCarousel::set_currentFrame(unsigned long currentFrame)
{
    m_rti.currentFrame = currentFrame;
}

void FIGCarousel::allocate_fig_to_fib(int figtype, int extension, int fib)
{
    if (fib < 0 or fib >= 3) {
        throw std::out_of_range("Invalid FIB");
    }

    auto fig = m_figs_available.find(std::make_pair(figtype, extension));

    if (fig != m_figs_available.end()) {
        FIGCarouselElement el;
        el.fig = fig->second;
        el.deadline = 0;
        m_fibs[fib].push_back(el);
    }
    else {
        std::stringstream ss;
        ss << "No FIG " << figtype << "/" << extension << " available";
        throw std::runtime_error(ss.str());
    }
}

void FIGCarousel::fib0(int framephase) {
    std::list<FIGCarouselElement> figs = m_fibs[0];

    std::vector<FIGCarouselElement> sorted_figs;

    /* Decrement all deadlines according to the desired repetition rate */
    for (auto& fig_el : figs) {
        fig_el.reduce_deadline();

        sorted_figs.push_back(fig_el);
    }

    /* Sort the FIGs in the FIB according to their deadline */
    std::sort(sorted_figs.begin(), sorted_figs.end(),
            []( const FIGCarouselElement& left,
                const FIGCarouselElement& right) {
            return left.deadline < right.deadline;
            });

    /* Data structure to carry FIB */
    const size_t fib_size = 30;
    uint8_t fib_data[fib_size];
    uint8_t *fib_data_current = fib_data;
    size_t available_size = fib_size;

    /* Take special care for FIG0/0 */
    auto fig0_0 = find_if(sorted_figs.begin(), sorted_figs.end(),
            [](const FIGCarouselElement& f) {
            return f.fig->repetition_rate() == FIG_rate::FIG0_0;
            });

    if (fig0_0 != sorted_figs.end()) {
        sorted_figs.erase(fig0_0);

        if (framephase == 0) { // TODO check for all TM
            size_t written = fig0_0->fig->fill(fib_data_current, available_size);

            if (written > 0) {
                available_size -= written;
                fib_data_current += written;
            }
            else {
                throw std::runtime_error("Failed to write FIG0/0");
            }
        }
    }

    /* Fill the FIB with the FIGs, taking the earliest deadline first */
    while (available_size > 0 and not sorted_figs.empty()) {
        auto fig_el = sorted_figs.begin();
        size_t written = fig_el->fig->fill(fib_data_current, available_size);

        if (written > 0) {
            available_size -= written;
            fib_data_current += written;
        }

        sorted_figs.erase(fig_el);
    }
}

