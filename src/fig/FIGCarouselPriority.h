/*
   Copyright (C) 2024
   Matthias P. Braendli, matthias.braendli@mpb.li

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
// Note: Uses 'info' level so output is visible without -v flag
#define PRIORITY_CAROUSEL_DEBUG 0

// Debug flag for repetition rate statistics - logs actual vs required rates
#define PRIORITY_RATE_STATS_DEBUG 0

/*
 * FIGEntryPriority - Holds a FIG and its scheduling state
 *
 * Deadline/Cycle Model:
 * ---------------------
 * - deadline_ms: Countdown timer, reset ONLY when cycle completes
 * - must_send: Flag indicating a cycle is in progress and not yet complete
 * - rate_ms: The required repetition rate from the FIG's specification
 *
 * Lifecycle:
 * 1. Counter starts at rate_ms, must_send = true (cycle in progress)
 * 2. Counter ticks down every 24ms
 * 3. When FIG completes its cycle (complete_fig_transmitted = true):
 *    - must_send = false
 *    - deadline_ms = rate_ms (reset for next cycle)
 * 4. When deadline_ms reaches 0:
 *    - If must_send == true: VIOLATION (cycle didn't complete in time)
 *    - If must_send == false: Start new cycle (must_send = true)
 *      Note: DON'T reset deadline_ms here - it continues counting to track lateness
 *
 * In a lightly loaded mux:
 * - can_send completes cycles well before deadline
 * - deadline_ms gets reset early, must_send is briefly true then false
 * - When deadline_ms hits 0, must_send is false, so we just start new cycle
 *
 * In a heavily loaded mux:
 * - must_send stays true longer as cycle takes time to complete
 * - If deadline_ms hits 0 while must_send is true, violation is logged
 *
 * Repetition rate tracking (for debug/verification):
 * - last_cycle_complete_ms: Timestamp when last cycle completed
 * - cycle_count: Number of completed cycles
 * - total_cycle_time_ms: Sum of all cycle times (for averaging)
 * - min_cycle_time_ms / max_cycle_time_ms: Range tracking
 */
struct FIGEntryPriority {
    IFIG* fig = nullptr;
    
    // Scheduling state
    bool must_send = false;         // Cycle is in progress, not yet complete
    
    // Deadline monitoring
    int deadline_ms = 0;            // Countdown timer - ONLY reset on cycle complete
    int rate_ms = 0;                // Required repetition rate (from FIG_rate)
    bool deadline_violated = false; // Set if deadline expires before cycle completes
    
    // For future dynamic priority adjustment
    int assigned_priority = 0;      // Current priority assignment
    int base_priority = 0;          // Original priority assignment
    
    // Repetition rate statistics (for verification)
    uint64_t last_cycle_complete_ms = 0;  // Timestamp of last completion
    uint64_t cycle_count = 0;             // Number of completed cycles
    uint64_t total_cycle_time_ms = 0;     // For calculating average
    uint64_t min_cycle_time_ms = UINT64_MAX;
    uint64_t max_cycle_time_ms = 0;
    uint64_t current_time_ms = 0;         // Updated each frame
    
    std::string name() const {
        if (fig) {
            return fig->name();
        }
        return "unknown";
    }
    
    // Returns true if this FIG has used more than the given percentage of its deadline
    // Used to prioritize FIGs that actually need bandwidth over those running ahead
    bool past_deadline_percent(int percent) const {
        // deadline_ms counts down from rate_ms
        // If deadline_ms < rate_ms * (100 - percent) / 100, we're past that percent
        // e.g., for 50%: if deadline_ms < rate_ms/2, we're past 50%
        int threshold = rate_ms * (100 - percent) / 100;
        return deadline_ms < threshold;
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
    
    // Called every frame (24ms). Returns true if a new cycle should start.
    bool tick_deadline(int elapsed_ms) {
        current_time_ms += elapsed_ms;
        deadline_ms -= elapsed_ms;
        
        if (deadline_ms <= 0) {
            if (must_send) {
                // Deadline expired but cycle not complete - VIOLATION
                if (!deadline_violated) {
                    deadline_violated = true;
                }
                // Don't start new cycle - current one isn't done yet
                // Let deadline go negative to track how late we are
                return false;
            } else {
                // Deadline expired and cycle is complete - start new cycle
                // Note: We do NOT reset deadline_ms here. It was already reset
                // when the cycle completed. If we're here, it means the cycle
                // completed exactly on time or the counter wrapped.
                // Actually, if must_send is false and deadline <= 0, the cycle
                // completed and deadline was reset, then counted down again.
                // So we should start a new cycle.
                return true;
            }
        }
        return false;
    }
    
    void on_cycle_complete(bool data_was_sent = true) {
        // Track repetition rate statistics only if data was actually sent
        // FIGs that return 0 bytes (nothing to send) shouldn't count as "cycles"
        // as they don't consume bandwidth and skew the statistics
        if (data_was_sent && last_cycle_complete_ms > 0) {
            uint64_t cycle_time = current_time_ms - last_cycle_complete_ms;
            // Only count if meaningful time has passed (avoid artifacts from
            // multiple completions in same frame due to timing granularity)
            if (cycle_time > 0) {
                cycle_count++;
                total_cycle_time_ms += cycle_time;
                if (cycle_time < min_cycle_time_ms) min_cycle_time_ms = cycle_time;
                if (cycle_time > max_cycle_time_ms) max_cycle_time_ms = cycle_time;
            }
        }
        if (data_was_sent) {
            last_cycle_complete_ms = current_time_ms;
        }
        
        // Reset deadline for next cycle
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
        // Note: Do NOT reset deadline_ms here!
        // The deadline was reset when the previous cycle completed.
        // If we reset here, we lose track of timing.
    }
    
    // Get average cycle time in ms (0 if no data)
    uint64_t avg_cycle_time_ms() const {
        return (cycle_count > 0) ? (total_cycle_time_ms / cycle_count) : 0;
    }
    
    // Reset statistics (call periodically to get recent averages)
    void reset_stats() {
        cycle_count = 0;
        total_cycle_time_ms = 0;
        min_cycle_time_ms = UINT64_MAX;
        max_cycle_time_ms = 0;
        // Don't reset last_cycle_complete_ms - need it for next cycle measurement
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
    
    // Find first FIG with must_send set
    FIGEntryPriority* find_must_send();
    
    // Find first FIG in carousel (for can_send - any FIG can potentially send)
    FIGEntryPriority* find_can_send();
    
    // Move entry to back of carousel (only call after successful send!)
    void move_to_back(FIGEntryPriority* entry);
    
    // Check if any FIG in this priority has must_send
    bool has_must_send() const;
    
    // Check if carousel is non-empty
    bool has_can_send() const;
};

/*
 * FIGCarouselPriority - Priority-based FIG scheduler
 *
 * Scheduling algorithm:
 * 
 * 1. Priority 0 (FIG 0/0, 0/7) handled specially:
 *    - Only sent in FIB 0 at framephase 0 (once per 96ms)
 *    - FIG 0/0 must be first FIG in FIB per EN 300 401 clause 6.4.1
 *
 * 2. Must-send pass (clear urgent FIGs):
 *    - Select priority via weighted counter system
 *    - If selected priority has must_send FIG, try to send it
 *    - If not, CASCADE: try lower priorities, then wrap to 1 and continue
 *    - Only move FIG to back of carousel if it actually wrote bytes
 *    - Continue until no must_send FIGs remain or FIB full
 *
 * 3. Can-send pass (fill remaining space opportunistically):
 *    - Same cascading logic as must-send
 *    - Any FIG can send even if not "due" - improves receiver acquisition
 *    - Continue until FIB full or all FIGs tried
 *
 * Counter mechanism:
 * - All counters decrement when ANY FIG successfully sends
 * - When a priority sends, its counter resets to poll_reset_value
 * - Priority with counter=0 (or lowest weighted score) is selected
 *
 * Carousel ordering:
 * - Front of carousel = least recently sent
 * - FIGs only move to back when they actually write bytes
 * - FIGs that write 0 bytes stay at front (get another chance)
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
    // fib_index: 0-3, which FIB we're filling (needed for FIG 0/0 placement)
    size_t fill_fib(uint8_t* buf, size_t max_size, int fib_index, int framephase);
    
    // Send priority 0 FIGs (0/0, 0/7) - only in FIB 0 at framephase 0
    size_t send_priority_zero(uint8_t* buf, size_t max_size, int fib_index, int framephase);
    
    // Select which priority to poll next based on weighted counters
    int select_priority();
    
    // Cascade through priorities starting from 'start', wrapping around
    // Returns priority that has a FIG matching the predicate, or -1 if none
    // For must_send: predicate checks must_send flag
    // For can_send: predicate checks carousel non-empty
    int cascade_find_priority(int start, bool must_send_only);
    
    // Called after a FIG successfully sends from a priority
    void on_fig_sent(int priority);
    
    // Decrement all poll counters (called on each successful FIG send)
    void decrement_all_counters();
    
    // Tick all deadline monitors (called each frame)
    void tick_all_deadlines(int elapsed_ms);
    
    // Check and log any deadline violations
    void check_and_log_deadlines(uint64_t current_frame);
    
    // Log repetition rate statistics (when PRIORITY_RATE_STATS_DEBUG enabled)
    void log_rate_statistics(uint64_t current_frame);
    
    // Try to send a FIG, returns bytes written
    // Does NOT move FIG in carousel - caller must do that only if bytes > 0
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
    
    // Frame counter for rate statistics logging interval
    uint64_t m_stats_log_counter = 0;
    
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
