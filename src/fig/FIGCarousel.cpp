/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017
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

#include "crc.h"
#include "Log.h"
#include "fig/FIGCarousel.h"
#include <boost/format.hpp>
#include <iostream>
#include <memory>
#include <deque>

#define CAROUSELDEBUG 0

namespace FIC {

/**************** FIGCarouselElement ****************/
void FIGCarouselElement::reduce_deadline()
{
    deadline -= 24; //ms

    if (deadline < 0) {
        etiLog.level(debug) <<
            "Could not respect repetition rate for FIG " <<
            fig->name() << " (" << deadline << "ms late)";
    }
}

void FIGCarouselElement::increase_deadline()
{
    deadline = rate_increment_ms(fig->repetition_rate());
}

bool FIGCarouselElement::check_deadline()
{
    const auto new_rate = fig->repetition_rate();
    const bool rate_changed = (m_last_rate != new_rate);

    if (rate_changed) {
        const auto new_deadline = rate_increment_ms(new_rate);
        if (deadline > new_deadline) {
            deadline = new_deadline;
        }
        m_last_rate = new_rate;
    }

    return rate_changed;
}


/**************** FIGCarousel *****************/

FIGCarousel::FIGCarousel(std::shared_ptr<dabEnsemble> ensemble) :
    m_rti(ensemble),
    m_fig0_0(&m_rti),
    m_fig0_1(&m_rti),
    m_fig0_2(&m_rti),
    m_fig0_3(&m_rti),
    m_fig0_5(&m_rti),
    m_fig0_6(&m_rti),
    m_fig0_17(&m_rti),
    m_fig0_8(&m_rti),
    m_fig1_0(&m_rti),
    m_fig0_13(&m_rti),
    m_fig0_10(&m_rti),
    m_fig0_9(&m_rti),
    m_fig1_1(&m_rti),
    m_fig1_4(&m_rti),
    m_fig1_5(&m_rti),
    m_fig0_18(&m_rti),
    m_fig0_19(&m_rti),
    m_fig0_21(&m_rti),
    m_fig0_24(&m_rti),
    m_fig2_0(&m_rti),
    m_fig2_1(&m_rti, true),
    m_fig2_5(&m_rti, false),
    m_fig2_4(&m_rti)
{
    /* Complete MCI except FIG0/8 should be in FIB0.
     * EN 300 401 V1.4.1 Clause 6.1
     *
     * It seems that this has become a weak requirement
     * with time, because current receivers can cope with
     * FIGs in any FIB. During elaboration of the standard,
     * receiver manufacturers were concerned about the complexity,
     * and pushed for support for receivers that only could
     * decode FIB0.
     *
     * V2.1.1 of the spec drops this requirement. Only FIG0/0 and
     * FIG 0/7 have a defined location in the FIC.
     */
    load_and_allocate(m_fig0_0, FIBAllocation::FIB0);
    load_and_allocate(m_fig0_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_2, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_3, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_6, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_8, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_13, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig0_17, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_0, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_10, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_9, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig1_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_4, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_18, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_19, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_21, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_24, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig2_0, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_4, FIBAllocation::FIB_ANY);
}

void FIGCarousel::load_and_allocate(IFIG& fig, FIBAllocation fib)
{
    FIGCarouselElement el;
    el.fig = &fig;
    el.deadline = 0;
    el.increase_deadline();
    m_fibs[fib].push_back(el);
}

void FIGCarousel::update(unsigned long currentFrame)
{
    m_rti.currentFrame = currentFrame;
}

void dumpfib(const uint8_t *buf, size_t bufsize) {
    std::cerr << "FIB ";
    for (size_t i = 0; i < bufsize; i++) {
        std::cerr << boost::format("%02x ") % (unsigned int)buf[i];
    }
    std::cerr << std::endl;
}

size_t FIGCarousel::write_fibs(
        uint8_t *buf,
        int framephase,
        bool fib3_present)
{
    /* Decrement all deadlines of all figs */
    for (auto& fib_fig : m_fibs) {
        auto& fig = fib_fig.second;
        for (auto& fig_el : fig) {
            fig_el.reduce_deadline();
        }
    }

    const int fibCount = fib3_present ? 4 : 3;

    for (int fib = 0; fib < fibCount; fib++) {
        memset(buf, 0x00, 30);
        size_t figSize = carousel(fib, buf, 30, framephase);

        if (figSize < 30) {
            buf[figSize] = 0xff; // end marker
        }
        else if (figSize > 30) {
            std::stringstream ss;
            ss << "FIB" << fib << " overload (" << figSize << "> 30)";
            throw std::runtime_error(ss.str());
        }

        uint16_t CRCtmp = 0xffff;
        CRCtmp = crc16(CRCtmp, buf, 30);
        CRCtmp ^= 0xffff;

        buf += 30;
        *(buf++) = ((char *) &CRCtmp)[1];
        *(buf++) = ((char *) &CRCtmp)[0];
    }

    return 32 * fibCount;
}

size_t FIGCarousel::carousel(
        int fib,
        uint8_t *buf,
        const size_t bufsize,
        int framephase)
{
    uint8_t *pbuf = buf;

    FIBAllocation fibix;

    switch (fib) {
        case 0:
            fibix = FIBAllocation::FIB0;
            break;
        case 1:
            fibix = FIBAllocation::FIB1;
            break;
        case 2:
            fibix = FIBAllocation::FIB2;
            break;
        default:
            throw std::invalid_argument("FIGCarousel::carousel called with invalid fib");
    }

    // Create our list of FIGs to consider for this FIB
    std::deque<FIGCarouselElement*> sorted_figs;
    for (auto& fig : m_fibs[fibix]) {
        sorted_figs.push_back(&fig);
    }
    for (auto& fig : m_fibs[FIBAllocation::FIB_ANY]) {
        sorted_figs.push_back(&fig);
    }

    /* Some FIGs might have changed rate since we last
     * set the deadline */
    for (auto& fig : sorted_figs) {
        if (fig->check_deadline()) {
#if CAROUSELDEBUG
            std::cerr << " FIG" << fig->fig->figtype() << "/" <<
                fig->fig->figextension() << " deadline changed" <<
                std::endl;
#endif
        }
    }

    /* Sort the FIGs in the FIB according to their deadline */
    std::sort(sorted_figs.begin(), sorted_figs.end(),
            []( const FIGCarouselElement* left,
                const FIGCarouselElement* right) {
            return left->deadline < right->deadline;
            });

#if CAROUSELDEBUG
    std::cerr << " ************** FIGs" << std::endl;
    for (auto& f : sorted_figs) {
        std::cerr << " FIG" << f->fig->figtype() << "/" <<
            f->fig->figextension() << " deadline " <<
            f->deadline << std::endl;
    }
#endif

    /* Data structure to carry FIB */
    size_t available_size = bufsize;

    /* Take special care for FIG0/0 */
    auto fig0_0 = find_if(sorted_figs.begin(), sorted_figs.end(),
            [](const FIGCarouselElement* f) {
            return f->fig->repetition_rate() == FIG_rate::FIG0_0;
            });

    if (fig0_0 != sorted_figs.end()) {
        if (framephase == 0) { // TODO check for all TM
            FillStatus status = (*fig0_0)->fig->fill(pbuf, available_size);
            size_t written = status.num_bytes_written;

            if (written > 0) {
                available_size -= written;
                pbuf += written;

#if CAROUSELDEBUG
                if (written) {
                    std::cerr << " ****** FIG0/0(special) wrote\t" << written << " bytes"
                        << std::endl;
                }

                if (    (*fig0_0)->fig->figtype() != 0 or
                        (*fig0_0)->fig->figextension() != 0 or
                        written != 6) {

                    std::stringstream ss;
                    ss << "Assertion error: FIG 0/0 is actually " <<
                        (*fig0_0)->fig->figtype()
                        << "/" << (*fig0_0)->fig->figextension() <<
                        " and wrote " << written << " bytes";

                    throw std::runtime_error(ss.str());
                }
#endif
            }
            else {
                throw std::runtime_error("Failed to write FIG0/0");
            }

            if (status.complete_fig_transmitted) {
                (*fig0_0)->increase_deadline();
            }
        }

        sorted_figs.erase(fig0_0);
    }


    /* Fill the FIB with the FIGs, taking the earliest deadline first */
    while (available_size > 0 and not sorted_figs.empty()) {
        auto fig_el = sorted_figs[0];
        FillStatus status = fig_el->fig->fill(pbuf, available_size);
        size_t written = status.num_bytes_written;

        // If exactly two bytes were written, it's because the FIG did
        // only write the header, but no content.
        // Writing only one byte is not allowed
        if (written == 1 or written == 2) {
            std::stringstream ss;
            ss << "Assertion error: FIG" << fig_el->fig->figtype() << "/" <<
                fig_el->fig->figextension() <<
                " did not write enough data: (" << written << ")";
            throw std::logic_error(ss.str());
        }
        else if (written > available_size) {
            std::stringstream ss;
            ss << "Assertion error: FIG" << fig_el->fig->figtype() << "/" <<
                fig_el->fig->figextension() <<
                " wrote " << written << " bytes, but only " <<
                available_size << " available!";
            throw std::logic_error(ss.str());
        }

        if (written > 2) {
            available_size -= written;
            pbuf += written;
        }
#if CAROUSELDEBUG
        if (written) {
            std::cerr <<
                " ** FIB" << fib <<
                " FIG" << fig_el->fig->figtype() << "/" <<
                fig_el->fig->figextension() <<
                " wrote\t" << written << " bytes" <<
                (status.complete_fig_transmitted ? ", complete" : ", incomplete") <<
                std::endl;
        }
#endif

        if (status.complete_fig_transmitted) {
            fig_el->increase_deadline();
        }

        sorted_figs.pop_front();
    }

    //dumpfib(buf, bufsize);

    return bufsize - available_size;
}

} // namespace FIC

