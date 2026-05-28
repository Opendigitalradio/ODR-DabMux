/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2024
   Matthias P. Braendli, matthias.braendli@mpb.li

   Phase-lock extensions:
   Copyright (C) 2026
   Samuel Hunt, Maxxwave Ltd. sam@maxxwave.co.uk

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

#include <memory>
#include "MuxElements.h"

namespace FIC {

#define PACKED __attribute__ ((packed))

class FIGRuntimeInformation {
    public:

        using dab_time_t = std::pair<uint32_t /* milliseconds */, time_t>;
        using get_time_func_t = std::function<dab_time_t()>;

        FIGRuntimeInformation(
                std::shared_ptr<dabEnsemble>& e,
                get_time_func_t getTimeFunc) :
            getTimeFunc(getTimeFunc),
            currentFrame(0),
            ensemble(e),
            factumAnalyzer(false) {}

        get_time_func_t getTimeFunc;
        unsigned long currentFrame;
        std::shared_ptr<dabEnsemble> ensemble;
        bool factumAnalyzer;
        
        /* Phase-lock mechanism for FIG 0/2 and FIG 1/1 synchronisation.
         *
         * Problem: Receivers use FIG 0/2 to learn service organisation, then
         * wait for FIG 1/1 labels for each service. If a receiver completes
         * its label scan before seeing complete 0/2 data, services may be
         * missing from the service list.
         *
         * Solution: One-way lock - FIG 1/1 cannot send a label until FIG 0/2
         * has announced that service. FIG 0/2 runs freely at its natural rate
         * (potentially completing multiple cycles per label cycle).
         *
         * Counters are reset when FIG 1/1 starts a new cycle, synchronising
         * the start of both cycles.
         *
         * This ensures that within any label cycle window:
         * - Every service announced by 0/2 can have its label sent
         * - 0/2 completes at least once (usually multiple times)
         * - Receivers see complete service data before labels finish
         */
        
        // Number of programme services announced by FIG 0/2 since last 1/1 cycle start
        size_t fig0_2_services_announced = 0;
        
        // Number of programme service labels sent by FIG 1/1 this cycle  
        size_t fig1_1_labels_sent = 0;
        
        // Called by FIG 0/2 when it sends a programme service entry
        void on_fig0_2_service_sent() {
            fig0_2_services_announced++;
        }
        
        // Called by FIG 1/1 to check if it can send a label
        // Returns true if 0/2 has announced more services than 1/1 has labelled
        bool can_fig1_1_send() const {
            return fig1_1_labels_sent < fig0_2_services_announced;
        }
        
        // Called by FIG 1/1 when it sends a service label
        void on_fig1_1_label_sent() {
            fig1_1_labels_sent++;
        }
        
        // Called by FIG 1/1 when it starts a new cycle
        // Resets both counters to synchronise the cycles
        void on_fig1_1_cycle_start() {
            fig0_2_services_announced = 0;
            fig1_1_labels_sent = 0;
        }
};

/* Recommended FIG rates according to ETSI TR 101 496-2 Table 3.6.1
 * Keep in mind that this specification was not updated since DAB+ came out.
 * The rates that are recommended there cannot be satisfied with a well filled
 * DAB+ multiplex, and this is the reason the rates have to be reduced compared
 * to this recommendation.
 */
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

        virtual FIG_rate repetition_rate() const = 0;

        virtual int figtype() const = 0;
        virtual int figextension() const = 0;

        virtual const std::string name() const;
};

} // namespace FIC
