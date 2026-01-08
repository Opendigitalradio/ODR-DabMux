/*
   Copyright (C) 2026
   Samuel Hunt, Maxxwave Ltd. sam@maxxwave.co.uk

   Implementation of a priority-based FIG carousel scheduler.
   This scheduler uses weighted priority classes with round-robin
   within each class to provide fair bandwidth allocation and
   prevent starvation.
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

#include "fig/FIG.h"
#include "fig/FIG0.h"
#include "fig/FIG0_20.h"
#include "fig/FIG1.h"
#include "fig/FIG2.h"
#include "fig/FIGSchedulerType.h"
#include "MuxElements.h"
#include "Log.h"

#include <list>
#include <vector>
#include <array>
#include <map>
#include <unordered_set>
#include <memory>
#include <string>

namespace FIC {

// Number of priority levels (0-9)
constexpr int NUM_PRIORITIES = 10;

// Priority 0 is special (always sent every frame)
constexpr int PRIORITY_CRITICAL = 0;

// Debug flag for carousel tracing - set to 1 to enable verbose logging
#define PRIORITY_CAROUSEL_DEBUG 0

/*
 * FIGEntryPriority - Holds a FIG and its scheduling state
 *
 * Each FIG has:
 * - must_send: Set when a new cycle is due, cleared when cycle completes
 * - deadline_ms: Independent countdown for monitoring repetition rate compliance
 * - rate_ms: The required repetition rate from the FIG's specification
 */
struct FIGEntryPriority {
    IFIG* fig = nullptr;
    
    // Scheduling state
    bool must_send = false;         // Cycle is due, not yet complete
    
    // Deadline monitoring (independent of scheduling)
    int deadline_ms = 0;            // Countdown timer
    int rate_ms = 0;                // Reset value (from FIG_rate)
    bool deadline_violated = false; // Set if deadline expires before cycle completes
    
    // For future dynamic priority adjustment
    int assigned_priority = 0;      // Current priority assignment
    int base_priority = 0;          // Original priority assignment
    
    std::string name() const {
        if (fig) {
            return fig->name();
        }
        return "unknown";
    }
    
    void init_deadline() {
        rate_ms = rate_increment_ms(fig->repetition_rate());
        // FIG 0/7 has special timing - only sent at framephase 0
        // Give it an extra frame margin to avoid false violation warnings
        if (fig->figtype() == 0 && fig->figextension() == 7) {
            deadline_ms = rate_ms + 24; // Extra frame margin
        } else {
            deadline_ms = rate_ms;
        }
        must_send = true;  // Start with cycle due
    }
    
    void tick_deadline(int elapsed_ms) {
        deadline_ms -= elapsed_ms;
        if (deadline_ms <= 0 && must_send && !deadline_violated) {
            // Deadline expired but cycle not complete (only flag once)
            deadline_violated = true;
        }
    }
    
    void on_cycle_complete() {
        // FIG 0/7 needs extra margin for framephase timing
        if (fig->figtype() == 0 && fig->figextension() == 7) {
            deadline_ms = rate_ms + 24;
        } else {
            deadline_ms = rate_ms;
        }
        must_send = false;
        // Note: deadline_violated is NOT cleared here
        // It will be logged and cleared by the monitoring system
    }
    
    void start_new_cycle() {
        must_send = true;
    }
};

/*
 * PriorityLevel - A priority class containing multiple FIGs
 *
 * Each priority has:
 * - poll_counter: Decrements on each FIG send, determines when this priority is due
 * - poll_reset_value: Value to reset counter to (2^priority for priorities 1+)
 * - carousel: Round-robin list of FIGs, front = least recently sent
 */
struct PriorityLevel {
    int priority = 0;
    int poll_counter = 0;
    int poll_reset_value = 1;
    std::list<FIGEntryPriority*> carousel;
    
    // Find first FIG with must_send set that fits in available space
    FIGEntryPriority* find_must_send();
    
    // Find first FIG that can send (has data)
    FIGEntryPriority* find_can_send();
    
    // Move entry to back of carousel (after sending)
    void move_to_back(FIGEntryPriority* entry);
    
    // Check if any FIG in this priority has must_send
    bool has_must_send() const;
    
    // Check if any FIG in this priority can send
    bool has_can_send() const;
};

/*
 * FIGCarouselPriority - Priority-based FIG scheduler
 *
 * Scheduling algorithm:
 * 1. Priority 0 (0/0, 0/7) always sends first every frame
 * 2. Other priorities are polled based on weighted counters
 * 3. Within each priority, FIGs rotate via round-robin carousel
 * 4. must_send FIGs are prioritised over can_send (opportunistic)
 * 5. Lower priorities can get early turns if higher has nothing due
 *
 * Counter mechanism:
 * - All counters decrement when ANY FIG is sent
 * - When a priority sends, its counter resets to poll_reset_value
 * - Priority with counter=0 (or lowest weighted score) is selected
 *
 * Priority stack:
 * - Tracks which priority sent most recently
 * - Used for tie-breaking when multiple priorities are due
 */
class FIGCarouselPriority {
public:
    FIGCarouselPriority(
            std::shared_ptr<dabEnsemble> ensemble,
            FIGRuntimeInformation::get_time_func_t getTimeFunc);
    
    // Write all FIBs to buffer, returns bytes written
    size_t write_fibs(uint8_t* buf, uint64_t current_frame, bool fib3_present);
    
private:
    // Fill a single FIB with FIG data
    size_t fill_fib(uint8_t* buf, size_t max_size, int framephase);
    
    // Send priority 0 FIGs (0/0, 0/7)
    size_t send_priority_zero(uint8_t* buf, size_t max_size, int framephase);
    
    // Select which priority to poll next
    int select_priority();
    
    // Called after a FIG is sent from a priority
    void on_fig_sent(int priority);
    
    // Decrement all poll counters (called on each FIG send)
    void decrement_all_counters();
    
    // Tick all deadline monitors (called each frame)
    void tick_all_deadlines(int elapsed_ms);
    
    // Check and log any deadline violations
    void check_and_log_deadlines(uint64_t current_frame);
    
    // Try to send a FIG, returns bytes written
    size_t try_send_fig(FIGEntryPriority* entry, uint8_t* buf, size_t max_size);
    
    // Assign FIGs to priority levels (hardcoded assignments)
    void assign_figs_to_priorities();
    
    // Add a FIG to a priority level
    void add_fig_to_priority(IFIG& fig, int priority);
    
    // Runtime information shared with FIGs
    FIGRuntimeInformation m_rti;
    
    // Priority levels array
    std::array<PriorityLevel, NUM_PRIORITIES> m_priorities;
    
    // Priority stack: front = least recently sent from
    std::list<int> m_priority_stack;
    
    // All FIG entries (owns the FIGEntryPriority objects)
    std::vector<std::unique_ptr<FIGEntryPriority>> m_all_entries;
    
    // Track missed deadlines for periodic logging
    std::unordered_set<std::string> m_missed_deadlines;
    
    // FIG instances
    FIG0_0 m_fig0_0;
    FIG0_1 m_fig0_1;
    FIG0_2 m_fig0_2;
    FIG0_3 m_fig0_3;
    FIG0_5 m_fig0_5;
    FIG0_6 m_fig0_6;
    FIG0_7 m_fig0_7;
    FIG0_8 m_fig0_8;
    FIG0_9 m_fig0_9;
    FIG0_10 m_fig0_10;
    FIG0_13 m_fig0_13;
    FIG0_14 m_fig0_14;
    FIG0_17 m_fig0_17;
    FIG0_18 m_fig0_18;
    FIG0_19 m_fig0_19;
    FIG0_20 m_fig0_20;
    FIG0_21 m_fig0_21;
    FIG0_24 m_fig0_24;
    FIG1_0 m_fig1_0;
    FIG1_1 m_fig1_1;
    FIG1_4 m_fig1_4;
    FIG1_5 m_fig1_5;
    FIG2_0 m_fig2_0;
    FIG2_1_and_5 m_fig2_1;
    FIG2_1_and_5 m_fig2_5;
    FIG2_4 m_fig2_4;
};

} // namespace FIC