/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2025
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

#include "ManagementServer.h"
#include "crc.h"
#include "Log.h"
#include "fig/FIGCarousel.h"
#include <boost/format.hpp>
#include <iostream>
#include <memory>
#include <deque>

#define CAROUSELDEBUG 0

namespace FIC {

static int rate_increment_ms(FIG_rate rate)
{
    switch (rate) {
        /* All these values are multiples of 24, so that it is easier to reason
         * about the behaviour when considering ETI frames of 24ms duration
         *
         * In large ensembles it's not always possible to respect the reptition rates, so
         * the values are a bit larger than what the spec says.
         * However, we observed that some receivers wouldn't always show labels (rate B),
         * and that's why we reduced B rate to slightly below 1s.
         * */
        case FIG_rate::FIG0_0:    return 96;        // Is a special case
        case FIG_rate::A:         return 240;
        case FIG_rate::A_B:       return 480;
        case FIG_rate::B:         return 960;
        case FIG_rate::C:         return 24000;
        case FIG_rate::D:         return 30000;
        case FIG_rate::E:         return 120000;
    }
    throw std::logic_error("Invalid FIG_rate");
}

// Next to the rates given in the spec, apply a bit of correction.
// Values lower than 1 increase the rate.
static double fig_correction_factor(IFIG *fig)
{
    switch (fig->figtype()) {
        case 0: switch (fig->figextension()) {
                    case 1:
                    case 2:
                        return 0.8;
                    case 8:
                    case 13:
                        return 1.1;
                    case 5:
                    case 9:
                    case 10:
                    case 17:
                    case 18:
                    case 20:
                        return 1.2;
                    case 14:
                    case 19:
                        return 1.8;
                    default:
                        return 1;
                }
        case 1: switch (fig->figextension()) {
                    case 1:
                        return 0.6;
                    default:
                        return 1;
                }
        case 2: return 1;
        default: return 1;
    }
}

/**************** FIGCarouselElement ****************/
FIGCarouselElement::FIGCarouselElement(IFIG *fig,
        double correction_factor,
        double with_fig_correction_factor) :
    m_fig(fig),
    m_correction_factor(correction_factor),
    m_with_fig_correction_factor(with_fig_correction_factor)
{
    increase_deadline();
}

void FIGCarouselElement::reduce_deadline()
{
    m_deadline -= 24; // ms
}

void FIGCarouselElement::increase_deadline()
{
    m_deadline = m_correction_factor * rate_increment_ms(m_fig->repetition_rate());

    if (m_with_fig_correction_factor) {
        m_deadline *= fig_correction_factor(m_fig);
    }
}

bool FIGCarouselElement::check_deadline()
{
    const auto new_rate = m_fig->repetition_rate();
    const bool rate_changed = (m_last_rate != new_rate);

    if (rate_changed) {
        const auto new_deadline = m_correction_factor * rate_increment_ms(new_rate);
        if (m_deadline > new_deadline) {
            m_deadline = new_deadline;
        }
        m_last_rate = new_rate;
    }

    return rate_changed;
}


/**************** FIGCarousel *****************/

FIGCarousel::FIGCarousel(
        std::shared_ptr<dabEnsemble> ensemble,
        FIGRuntimeInformation::get_time_func_t getTimeFunc,
        bool with_fig_correction_factor) :
    with_fig_correction_factor(with_fig_correction_factor),
    m_rti(ensemble, getTimeFunc),
    m_fig0_0(&m_rti),
    m_fig0_1(&m_rti),
    m_fig0_2(&m_rti),
    m_fig0_3(&m_rti),
    m_fig0_5(&m_rti),
    m_fig0_6(&m_rti),
    m_fig0_7(&m_rti),
    m_fig0_17(&m_rti),
    m_fig0_8(&m_rti),
    m_fig1_0(&m_rti),
    m_fig0_13(&m_rti),
    m_fig0_14(&m_rti),
    m_fig0_10(&m_rti),
    m_fig0_9(&m_rti),
    m_fig1_1(&m_rti),
    m_fig1_4(&m_rti),
    m_fig1_5(&m_rti),
    m_fig0_18(&m_rti),
    m_fig0_19(&m_rti),
    m_fig0_20(&m_rti),
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
    load_and_allocate(m_fig0_7, FIBAllocation::FIB0);
    load_and_allocate(m_fig0_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_2, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_3, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_6, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_8, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_13, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_14, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig0_17, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_0, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_10, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_9, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig1_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_4, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig1_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_18, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_19, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_20, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_21, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig0_24, FIBAllocation::FIB_ANY);

    load_and_allocate(m_fig2_0, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_1, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_5, FIBAllocation::FIB_ANY);
    load_and_allocate(m_fig2_4, FIBAllocation::FIB_ANY);
}

void FIGCarousel::load_and_allocate(IFIG& fig, FIBAllocation fib)
{
    m_fibs[fib].emplace_back(&fig, correction_factor, with_fig_correction_factor);
}

size_t FIGCarousel::write_fibs(
        uint8_t *buf,
        uint64_t current_frame,
        bool fib3_present)
{
    m_rti.currentFrame = current_frame;

    ManagementServer& mgmt_server = get_mgmt_server();

    /* Decrement all deadlines of all figs */
    for (auto& fib_fig : m_fibs) {
        auto& fig = fib_fig.second;
        for (auto& fig_el : fig) {
            fig_el.reduce_deadline();

            if (fig_el.deadline() < 0) {
#if CAROUSELDEBUG
                etiLog.level(warn) << " FIG" << fig_el.fig()->name() << " LATE";
#endif
                mgmt_server.fig_deadline_missed(std::to_string(fig_el.fig()->figtype()) + "_" + std::to_string(fig_el.fig()->figextension()));
            }
        }
    }

    const int fibCount = fib3_present ? 4 : 3;

    for (int fib = 0; fib < fibCount; fib++) {
        memset(buf, 0x00, 30);
        size_t figSize = carousel(fib, buf, 30, current_frame);

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
        *(buf++) = (CRCtmp >> 8) & 0x00FF;
        *(buf++) = CRCtmp & 0x00FF;
    }

    return 32 * fibCount;
}

double FIGCarousel::get_rate_correction() const
{
    return correction_factor;
}

void FIGCarousel::set_rate_correction(double factor)
{
    // Lower bound: 0.5 is quite arbitrary, as most applications need a positive factor.
    // Upper bound: 4 times lower repetition than the standard is already quite a lot,
    // I suspect values between 1 and 2 will be most common.
    if (factor < 0.5 or factor > 4) {
        throw std::runtime_error(std::string{"rate correction factor "}
                + std::to_string(factor) + "must be between 0.5 and 4.0");
    }
    correction_factor = factor;
}

size_t FIGCarousel::carousel(
        int fib,
        uint8_t *buf,
        const size_t bufsize,
        uint64_t current_frame)
{
    const int framephase = current_frame % 4;

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
            etiLog.level(debug) <<
                "FRAME " << current_frame <<
                " FIG" << fig->fig()->figtype() << "/" <<
                fig->fig()->figextension() << " deadline changed";
#endif
        }
    }

    /* Sort the FIGs in the FIB according to their deadline */
    std::sort(sorted_figs.begin(), sorted_figs.end(),
            []( const FIGCarouselElement* left,
                const FIGCarouselElement* right) {
            return left->deadline() < right->deadline();
            });

#if CAROUSELDEBUG
    {
        std::stringstream ss;
        ss << "FRAME " << current_frame
            << " sorted FIGs ";

        for (auto& f : sorted_figs) {
            ss << f->fig()->figtype() << "/" <<
                f->fig()->figextension() << "(" <<
                f->deadline() << ") ";
        }
        etiLog.level(debug) << ss.str();
    }
#endif

    /* Data structure to carry FIB */
    size_t available_size = bufsize;

    /* Take special care for FIG0/0 which must be the first FIG of the FIB */
    auto fig0_0 = find_if(sorted_figs.begin(), sorted_figs.end(),
        [](const FIGCarouselElement *f) {
            return (f->fig()->figtype() == 0 && f->fig()->figextension() == 0);
        });

    if (fig0_0 != sorted_figs.end()) {
        if (framephase == 0) { // TODO check for all TM
            FillStatus status = (*fig0_0)->fig()->fill(pbuf, available_size);
            size_t written = status.num_bytes_written;

            if (written > 0) {
                available_size -= written;
                pbuf += written;

#if CAROUSELDEBUG
                etiLog.level(debug) <<
                    "FRAME " << current_frame <<
                    " *** FIG0/0(special) wrote\t" << written << " bytes";
#endif
            }
            else {
                throw std::logic_error("Failed to write FIG0/0");
            }

            if (status.complete_fig_transmitted) {
                (*fig0_0)->increase_deadline();
            }
            else {
                throw std::logic_error("FIG0/0 did not complete!");
            }

            /* FIG0/7 must directly follow FIG 0/0 */
            auto fig0_7 = find_if(sorted_figs.begin(), sorted_figs.end(),
                [](const FIGCarouselElement *f) {
                    return (f->fig()->figtype() == 0 && f->fig()->figextension() == 7);
                });

            if (fig0_7 != sorted_figs.end()) {
                FillStatus status0_7 = (*fig0_7)->fig()->fill(pbuf, available_size);
                size_t written = status0_7.num_bytes_written;

                if (written > 0) {
                    available_size -= written;
                    pbuf += written;

#if CAROUSELDEBUG
                    etiLog.level(debug) <<
                        "FRAME " << current_frame <<
                        " ****** FIG0/7(special) wrote\t" << written << " bytes";
#endif
                }

                if (status0_7.complete_fig_transmitted) {
                    (*fig0_7)->increase_deadline();
                }
            }
        }

        // never transmit FIG 0/0 in any other spot
        sorted_figs.erase(fig0_0);
    }

    // never transmit FIG 0/7 except right after FIG 0/0.
    {
        auto fig0_7 = find_if(sorted_figs.begin(), sorted_figs.end(),
                [](const FIGCarouselElement *f) {
                return (f->fig()->figtype() == 0 && f->fig()->figextension() == 7);
                });
        if (fig0_7 != sorted_figs.end()) {
            sorted_figs.erase(fig0_7);
        }
    }

    /* Fill the FIB with the FIGs, taking the earliest deadline first */
    while (available_size > 0 and not sorted_figs.empty()) {
        auto fig_el = sorted_figs[0];
        FillStatus status = fig_el->fig()->fill(pbuf, available_size);
        size_t written = status.num_bytes_written;

        // If exactly two bytes were written, it's because the FIG did
        // only write the header, but no content.
        // Writing only one byte is not allowed
        if (written == 1 or written == 2) {
            std::stringstream ss;
            ss << "Assertion error: FIG" << fig_el->fig()->figtype() << "/" <<
                fig_el->fig()->figextension() <<
                " did not write enough data: (" << written << ")";
            throw std::logic_error(ss.str());
        }
        else if (written > available_size) {
            std::stringstream ss;
            ss << "Assertion error: FIG" << fig_el->fig()->figtype() << "/" <<
                fig_el->fig()->figextension() <<
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
            etiLog.level(debug) <<
                " FRAME " << current_frame <<
                " ** FIB" << fib <<
                " FIG" << fig_el->fig()->figtype() << "/" <<
                fig_el->fig()->figextension() <<
                " wrote\t" << written << " bytes" <<
                (status.complete_fig_transmitted ? ", complete" : ", incomplete") <<
                ", margin " << fig_el->deadline();
        }
#endif

        if (status.complete_fig_transmitted) {
            fig_el->increase_deadline();
        }

        sorted_figs.pop_front();
    }

#if 0
    std::cerr << "FIB ";
    for (size_t i = 0; i < bufsize; i++) {
        std::cerr << boost::format("%02x ") % (unsigned int)buf[i];
    }
    std::cerr << std::endl;
#endif

    return bufsize - available_size;
}

} // namespace FIC

