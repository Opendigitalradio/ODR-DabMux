/*
   Copyright (C) 2026
   Samuel Hunt, Maxxwave Ltd. sam@maxxwave.co.uk

   Implementation of a priority-based FIG carousel scheduler.
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

#include "fig/FIGCarouselPriority.h"
#include "fig/FIG0_20.h"
#include "crc.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cstring>

namespace FIC {

/**************** PriorityLevel ****************/

FIGEntryPriority* PriorityLevel::find_must_send()
{
    for (auto* entry : carousel) {
        if (entry->must_send) {
            return entry;
        }
    }
    return nullptr;
}

FIGEntryPriority* PriorityLevel::find_can_send()
{
    // Return front of carousel if non-empty
    // Any FIG can potentially send - it will return 0 bytes if nothing to send
    if (!carousel.empty()) {
        return carousel.front();
    }
    return nullptr;
}

void PriorityLevel::move_to_back(FIGEntryPriority* entry)
{
    carousel.remove(entry);
    carousel.push_back(entry);
}

bool PriorityLevel::has_must_send() const
{
    for (const auto* entry : carousel) {
        if (entry->must_send) {
            return true;
        }
    }
    return false;
}

bool PriorityLevel::has_can_send() const
{
    return !carousel.empty();
}

/**************** FIGCarouselPriority ****************/

FIGCarouselPriority::FIGCarouselPriority(
        std::shared_ptr<dabEnsemble> ensemble,
        FIGRuntimeInformation::get_time_func_t getTimeFunc) :
    m_rti(ensemble, getTimeFunc),
    m_fig0_0(&m_rti),
    m_fig0_1(&m_rti),
    m_fig0_2(&m_rti),
    m_fig0_3(&m_rti),
    m_fig0_5(&m_rti),
    m_fig0_6(&m_rti),
    m_fig0_7(&m_rti),
    m_fig0_8(&m_rti),
    m_fig0_9(&m_rti),
    m_fig0_10(&m_rti),
    m_fig0_13(&m_rti),
    m_fig0_14(&m_rti),
    m_fig0_17(&m_rti),
    m_fig0_18(&m_rti),
    m_fig0_19(&m_rti),
    m_fig0_20(&m_rti),
    m_fig0_21(&m_rti),
    m_fig0_24(&m_rti),
    m_fig1_0(&m_rti),
    m_fig1_1(&m_rti),
    m_fig1_4(&m_rti),
    m_fig1_5(&m_rti),
    m_fig2_0(&m_rti),
    m_fig2_1(&m_rti, true),
    m_fig2_5(&m_rti, false),
    m_fig2_4(&m_rti)
{
    // Initialize priority levels
    for (int i = 0; i < NUM_PRIORITIES; i++) {
        m_priorities[i].priority = i;
        // Priority 0 is handled specially (0/0 and 0/7)
        // Other priorities have reset values tuned to meet rate requirements:
        // - Priority 1 (Rate A, 100ms): Must send frequently
        // - Lower priorities can wait longer
        switch (i) {
            case 0: m_priorities[i].poll_reset_value = 1; break;  // Special
            case 1: m_priorities[i].poll_reset_value = 1; break;  // Rate A - every opportunity
            case 2: m_priorities[i].poll_reset_value = 2; break;  // Rate A
            case 3: m_priorities[i].poll_reset_value = 4; break;  // Rate B (1 sec)
            case 4: m_priorities[i].poll_reset_value = 8; break;  // Rate B
            case 5: m_priorities[i].poll_reset_value = 16; break; // Rate B
            case 6: m_priorities[i].poll_reset_value = 32; break; // Rate B
            case 7: m_priorities[i].poll_reset_value = 64; break; // Rate C (10 sec)
            case 8: m_priorities[i].poll_reset_value = 128; break; // Rate E
            default: m_priorities[i].poll_reset_value = 256; break;
        }
        m_priorities[i].poll_counter = m_priorities[i].poll_reset_value;
    }
    
    // Initialize priority stack (all priorities except 0)
    for (int i = 1; i < NUM_PRIORITIES; i++) {
        m_priority_stack.push_back(i);
    }
    
    // Assign FIGs to priorities
    assign_figs_to_priorities();
}

void FIGCarouselPriority::assign_figs_to_priorities()
{
    /*
     * Priority assignments based on WorldDAB "Guidance on FIC generation 
     * for high service count ensembles":
     * 
     * Priority 0 (every frame): 0/0, 0/7 - Fixed insertion (first FIB of every frame)
     * Priority 1 (reset 1):     0/1, 0/2 - Core MCI (fixed share, ~50% of FIC)
     * Priority 2 (reset 2):     1/1, 1/0 - Service labels + ensemble label (fixed share)
     *                                      Critical for scan completion - receivers timeout!
     * Priority 3 (reset 4):     0/9      - ECC (timing critical, once/sec)
     * Priority 4 (reset 8):     0/3, 0/8, 0/13 - Packet mode, component details
     * Priority 5 (reset 16):    0/5, 0/10, 0/17, 0/18, 0/20 - Scaled insertion
     *                           1/4, 1/5, 2/0, 2/1, 2/4, 2/5 - Other labels (match label cycle)
     *                           PTY (0/17) is NOT essential for scan, just nice-to-have
     * Priority 6 (reset 32):    0/14, 0/19 - Announcements, FEC
     * Priority 7 (reset 64):    0/6 - Service linking (short form)
     * Priority 8 (reset 128):   0/21, 0/24 - Frequency info, OE (best effort, 2 mins)
     * Priority 9 (reserved):    (future/dynamic use)
     * 
     * Key insight from WorldDAB: For >20 services, service labels (1/1) are 
     * critical for scan completion. Receivers abort scan if labels timeout.
     * PTY and other metadata should be "scaled insertion" - match label cycle,
     * not compete with it.
     */
    
    // Priority 0: Fixed insertion (every frame at framephase 0)
    add_fig_to_priority(m_fig0_0, 0);
    add_fig_to_priority(m_fig0_7, 0);
    
    // Priority 1: Core MCI (fixed share - ~50% of FIC capacity)
    add_fig_to_priority(m_fig0_1, 1);
    add_fig_to_priority(m_fig0_2, 1);
    
    // Priority 2: Service labels (fixed share - critical for scan completion!)
    // WorldDAB: "labels should get 2 FIGs per 96ms frame"
    // Ensemble label should match service label rate
    add_fig_to_priority(m_fig1_1, 2);  // Programme service labels - PROMOTED
    add_fig_to_priority(m_fig1_0, 2);  // Ensemble label - PROMOTED
    
    // Priority 3: ECC (timing critical - once per second)
    add_fig_to_priority(m_fig0_9, 3);
    
    // Priority 4: Component details
    add_fig_to_priority(m_fig0_3, 4);  // Packet mode
    add_fig_to_priority(m_fig0_8, 4);  // Service component global def
    add_fig_to_priority(m_fig0_13, 4); // User applications
    
    // Priority 5: Scaled insertion (should match label cycle time)
    // These are important but NOT critical for scan completion
    add_fig_to_priority(m_fig0_5, 5);   // Language
    add_fig_to_priority(m_fig0_10, 5);  // Date and time
    add_fig_to_priority(m_fig0_17, 5);  // PTY - DEMOTED (not essential for scan)
    add_fig_to_priority(m_fig0_18, 5);  // Announcement support
    add_fig_to_priority(m_fig0_20, 5);  // SCI - Service Component Information
    // Other labels (component, data service) - less critical than service labels
    add_fig_to_priority(m_fig1_4, 5);   // Component labels
    add_fig_to_priority(m_fig1_5, 5);   // Data service labels
    add_fig_to_priority(m_fig2_0, 5);   // Extended ensemble label
    add_fig_to_priority(m_fig2_1, 5);   // Extended service labels
    add_fig_to_priority(m_fig2_4, 5);   // Extended component labels
    add_fig_to_priority(m_fig2_5, 5);   // Extended data service labels
    
    // Priority 6: Announcements and FEC
    add_fig_to_priority(m_fig0_14, 6);  // FEC subchannel organisation
    add_fig_to_priority(m_fig0_19, 6);  // Announcement switching
    
    // Priority 7: Service linking
    add_fig_to_priority(m_fig0_6, 7);   // Service linking (short form)
    
    // Priority 8: Frequency/OE (best effort - within 2 minutes)
    add_fig_to_priority(m_fig0_21, 8);  // Frequency information
    add_fig_to_priority(m_fig0_24, 8);  // OE services
    
    // Priority 9: Reserved for dynamic adjustment
}

void FIGCarouselPriority::add_fig_to_priority(IFIG& fig, int priority)
{
    std::unique_ptr<FIGEntryPriority> entry(new FIGEntryPriority());
    entry->fig = &fig;
    entry->assigned_priority = priority;
    entry->base_priority = priority;
    entry->init_deadline();
    
    FIGEntryPriority* entry_ptr = entry.get();
    m_all_entries.push_back(std::move(entry));
    
    m_priorities[priority].carousel.push_back(entry_ptr);
}

void FIGCarouselPriority::tick_all_deadlines(int elapsed_ms)
{
    for (auto& entry : m_all_entries) {
        bool start_new = entry->tick_deadline(elapsed_ms);
        
        // Check if a new cycle should start
        if (start_new) {
            entry->start_new_cycle();
        }
    }
}

void FIGCarouselPriority::check_and_log_deadlines(uint64_t current_frame)
{
    // Log every ~6 seconds (250 frames at 24ms)
    if ((current_frame % 250) != 0) {
        return;
    }
    
    std::stringstream ss_slow;      // 1x-2x nominal (acceptable per WorldDAB)
    std::stringstream ss_warning;   // 2x-3x nominal (approaching limit)
    std::stringstream ss_critical;  // >3x nominal (unacceptable)
    bool any_slow = false;
    bool any_warning = false;
    bool any_critical = false;
    
    for (auto& entry : m_all_entries) {
        if (entry->deadline_violated && entry->cycle_count > 0) {
            // Check how severe the violation is based on average cycle time
            uint64_t avg = entry->avg_cycle_time_ms();
            uint64_t required = entry->rate_ms;
            
            if (avg > required * 3) {
                ss_critical << " " << entry->name();
                any_critical = true;
            } else if (avg > required * 2) {
                ss_warning << " " << entry->name();
                any_warning = true;
            } else if (avg > required) {
                ss_slow << " " << entry->name();
                any_slow = true;
            }
            entry->deadline_violated = false;
        }
    }
    
    // Log critical issues first (these violate WorldDAB guidance)
    if (any_critical) {
        etiLog.level(warn) << "FIGCarouselPriority: UNACCEPTABLE repetition rates (>3x nominal) for FIGs:" << ss_critical.str();
    }
    
    // Log warnings (approaching limit)
    if (any_warning) {
        etiLog.level(info) << "FIGCarouselPriority: Slow repetition rates (2x-3x nominal) for FIGs:" << ss_warning.str();
    }
    
    // Log slow but acceptable (only if no worse issues, to avoid log spam)
    if (any_slow && !any_warning && !any_critical) {
        etiLog.level(info) << "FIGCarouselPriority: Repetition rates slightly exceeded for FIGs:" << ss_slow.str();
    }
}

void FIGCarouselPriority::log_rate_statistics(uint64_t current_frame)
{
#if PRIORITY_RATE_STATS_DEBUG
    // Log every ~10 seconds (approx 417 frames)
    m_stats_log_counter++;
    if (m_stats_log_counter < 417) {
        return;
    }
    m_stats_log_counter = 0;
    
    etiLog.level(info) << "=== FIG Repetition Rate Statistics ===";
    etiLog.level(info) << "FIG          Required   Avg      Min      Max      Cycles";
    etiLog.level(info) << "------------ -------- -------- -------- -------- --------";
    
    for (auto& entry : m_all_entries) {
        if (entry->cycle_count > 0) {
            std::stringstream ss;
            ss << std::left << std::setw(12) << entry->name()
               << " " << std::right << std::setw(7) << entry->rate_ms << "ms"
               << " " << std::setw(7) << entry->avg_cycle_time_ms() << "ms"
               << " " << std::setw(7) << entry->min_cycle_time_ms << "ms"
               << " " << std::setw(7) << entry->max_cycle_time_ms << "ms"
               << " " << std::setw(8) << entry->cycle_count;
            
            // WorldDAB guidance: worst case is 3x nominal rate
            // - Within spec: no marker
            // - SLOW (1x-2x): exceeds nominal but well within WorldDAB limits
            // - [!] (2x-3x): approaching WorldDAB worst-case limit
            // - UNACCEPTABLE (>3x): violates WorldDAB guidance
            uint64_t avg = entry->avg_cycle_time_ms();
            uint64_t required = entry->rate_ms;
            if (avg > required * 3) {
                ss << " UNACCEPTABLE";  // >3x nominal - violates WorldDAB
            } else if (avg > required * 2) {
                ss << " [!]";           // 2x-3x nominal - approaching limit
            } else if (avg > required) {
                ss << " [SLOW]";        // 1x-2x nominal - exceeds but acceptable
            }
            
            etiLog.level(info) << ss.str();
        }
        
        // Reset stats for next interval
        entry->reset_stats();
    }
    etiLog.level(info) << "======================================";
#else
    (void)current_frame;
#endif
}

size_t FIGCarouselPriority::write_fibs(uint8_t* buf, uint64_t current_frame, bool fib3_present)
{
    m_rti.currentFrame = current_frame;
    
    // Tick all deadline monitors (24ms per frame)
    tick_all_deadlines(24);
    
    // Periodically log missed deadlines
    check_and_log_deadlines(current_frame);
    
    // Periodically log rate statistics (if enabled)
    log_rate_statistics(current_frame);
    
    const int fibCount = fib3_present ? 4 : 3;
    const int framephase = current_frame % 4;
    
    for (int fib = 0; fib < fibCount; fib++) {
        memset(buf, 0x00, 30);
        size_t figSize = fill_fib(buf, 30, fib, framephase);
        
        if (figSize < 30) {
            buf[figSize] = 0xff; // End marker
        }
        else if (figSize > 30) {
            std::stringstream ss;
            ss << "FIB" << fib << " overload (" << figSize << " > 30)";
            throw std::runtime_error(ss.str());
        }
        
        // Calculate and append CRC
        uint16_t crc = 0xffff;
        crc = crc16(crc, buf, 30);
        crc ^= 0xffff;
        
        buf += 30;
        *(buf++) = (crc >> 8) & 0xFF;
        *(buf++) = crc & 0xFF;
    }
    
    return 32 * fibCount;
}

size_t FIGCarouselPriority::fill_fib(uint8_t* buf, size_t max_size, int fib_index, int framephase)
{
    size_t written = 0;
    
#if PRIORITY_CAROUSEL_DEBUG
    std::stringstream fib_log;
    fib_log << "FIB" << fib_index << " fp" << framephase << ": ";
#endif
    
    // Step 1: Priority 0 - FIG 0/0 and 0/7 (only in FIB 0 at framephase 0)
    written += send_priority_zero(buf, max_size, fib_index, framephase);
    
    size_t remaining = max_size - written;
    uint8_t* pbuf = buf + written;
    
    // Track which FIGs we've tried this pass to avoid infinite loops
    std::unordered_set<FIGEntryPriority*> tried_this_fib;
    
    // Step 2: Must-send pass - clear all urgent FIGs using cascading priority
    // We iterate through priorities and try each must_send FIG.
    // A FIG might return 0 bytes if it doesn't fit, so we try all of them.
    
    bool made_progress = true;
    while (remaining > 2 && made_progress) {
        made_progress = false;
        
        // Try each priority, starting from the selected one
        int start_prio = select_priority();
        if (start_prio < 0) {
            start_prio = 1;
        }
        
        // Look for must_send FIGs in priority order
        for (int prio_offset = 0; prio_offset < NUM_PRIORITIES - 1 && remaining > 2; prio_offset++) {
            int prio = start_prio + prio_offset;
            if (prio >= NUM_PRIORITIES) {
                prio = 1 + (prio - NUM_PRIORITIES); // Wrap around, skip 0
            }
            
            if (!m_priorities[prio].has_must_send()) {
                continue;
            }
            
            // Try all must_send FIGs in this priority
            size_t carousel_size = m_priorities[prio].carousel.size();
            for (size_t fig_attempt = 0; fig_attempt < carousel_size && remaining > 2; fig_attempt++) {
                FIGEntryPriority* entry = m_priorities[prio].find_must_send();
                if (!entry) {
                    break; // No more must_send in this priority
                }
                
                // Skip if we've already tried this FIG this FIB
                if (tried_this_fib.count(entry)) {
                    m_priorities[prio].move_to_back(entry);
                    continue;
                }
                tried_this_fib.insert(entry);
                
                size_t bytes = try_send_fig(entry, pbuf, remaining);
                if (bytes > 0) {
#if PRIORITY_CAROUSEL_DEBUG
                    fib_log << entry->name() << ":" << bytes << "B ";
#endif
                    written += bytes;
                    remaining -= bytes;
                    pbuf += bytes;
                    m_priorities[prio].move_to_back(entry);
                    on_fig_sent(prio);
                    made_progress = true;
                } else {
                    // FIG couldn't write (doesn't fit), move to back and try next
                    m_priorities[prio].move_to_back(entry);
                }
            }
        }
    }
    
    // Step 3: Can-send pass - fill remaining space opportunistically
    // By now, there should be no must_send FIGs (cleared in step 2)
    // Any FIG can send to fill the FIB and improve repetition rates
    //
    // TWO-PASS APPROACH to avoid over-serving FIGs that are ahead of schedule:
    // Pass 1: Only try FIGs that are past 50% of their deadline (actually need bandwidth)
    // Pass 2: If still space, try any FIG that can_send (avoid wasting FIB space)
    //
    // This ensures bandwidth goes to FIGs that need it (like 1/1 at ~1200ms vs 960ms target)
    // rather than FIGs already well ahead of schedule (like 0/9 at ~500ms vs 960ms target)
    //
    // NOTE: We do NOT update the priority counters here. The counter system
    // is for bandwidth allocation in must_send. In can_send, we just fill space.
    
    tried_this_fib.clear();
    
    // Pass 1: FIGs that are past 50% of their deadline (need bandwidth more urgently)
    for (int prio = 1; prio < NUM_PRIORITIES && remaining > 2; prio++) {
        if (m_priorities[prio].carousel.empty()) {
            continue;
        }
        
        size_t carousel_size = m_priorities[prio].carousel.size();
        for (size_t fig_attempt = 0; fig_attempt < carousel_size && remaining > 2; fig_attempt++) {
            FIGEntryPriority* entry = m_priorities[prio].find_can_send();
            if (!entry) {
                break;
            }
            
            // Skip if we've already tried this FIG this FIB
            if (tried_this_fib.count(entry)) {
                m_priorities[prio].move_to_back(entry);
                continue;
            }
            
            // Pass 1: Only try FIGs past 50% of their deadline
            if (!entry->past_deadline_percent(50)) {
                tried_this_fib.insert(entry);
                m_priorities[prio].move_to_back(entry);
                continue;
            }
            
            tried_this_fib.insert(entry);
            
            size_t bytes = try_send_fig(entry, pbuf, remaining);
            if (bytes > 0) {
#if PRIORITY_CAROUSEL_DEBUG
                fib_log << entry->name() << ":" << bytes << "B(urg) ";
#endif
                written += bytes;
                remaining -= bytes;
                pbuf += bytes;
            }
            m_priorities[prio].move_to_back(entry);
        }
    }
    
    // Pass 2: Any remaining FIGs that can send (to fill unused space)
    // Reset tried set - we want to try everything again, but only those we skipped
    std::unordered_set<FIGEntryPriority*> tried_pass1 = tried_this_fib;
    
    for (int prio = 1; prio < NUM_PRIORITIES && remaining > 2; prio++) {
        if (m_priorities[prio].carousel.empty()) {
            continue;
        }
        
        size_t carousel_size = m_priorities[prio].carousel.size();
        for (size_t fig_attempt = 0; fig_attempt < carousel_size && remaining > 2; fig_attempt++) {
            FIGEntryPriority* entry = m_priorities[prio].find_can_send();
            if (!entry) {
                break;
            }
            
            // Skip if already sent in pass 1
            if (tried_pass1.count(entry) && tried_this_fib.count(entry)) {
                // Check if it was actually sent (not just tried and skipped)
                // If it was skipped due to not being urgent, we can try it now
                if (entry->past_deadline_percent(50)) {
                    // Was tried and is urgent - was actually attempted, skip
                    m_priorities[prio].move_to_back(entry);
                    continue;
                }
            }
            
            tried_this_fib.insert(entry);
            
            size_t bytes = try_send_fig(entry, pbuf, remaining);
            if (bytes > 0) {
#if PRIORITY_CAROUSEL_DEBUG
                fib_log << entry->name() << ":" << bytes << "B(fill) ";
#endif
                written += bytes;
                remaining -= bytes;
                pbuf += bytes;
            }
            m_priorities[prio].move_to_back(entry);
        }
    }
    
#if PRIORITY_CAROUSEL_DEBUG
    fib_log << "= " << written << "/" << max_size;
    etiLog.level(info) << fib_log.str();
#endif
    
    return written;
}

size_t FIGCarouselPriority::send_priority_zero(uint8_t* buf, size_t max_size, int fib_index, int framephase)
{
    size_t written = 0;
    
    // EN 300 401 Clause 6.4.1: FIG 0/0 shall only be transmitted in the first
    // FIB of the first CIF of each FIC (once per 96ms in Mode I)
    // It must be the first FIG in that FIB
    
    // Only send in FIB 0 at framephase 0
    if (fib_index != 0 || framephase != 0) {
        return 0;
    }
    
#if PRIORITY_CAROUSEL_DEBUG
    etiLog.level(info) << "send_priority_zero: FIB0, framephase=0, sending 0/0 and 0/7";
#endif
    
    // Find and send FIG 0/0
    for (auto* entry : m_priorities[0].carousel) {
        if (entry->fig->figtype() == 0 && entry->fig->figextension() == 0) {
            FillStatus status = entry->fig->fill(buf + written, max_size - written);
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(info) << "  FIG 0/0: wrote " << status.num_bytes_written 
                               << " bytes, complete=" << status.complete_fig_transmitted
                               << ", deadline was " << entry->deadline_ms << "ms";
#endif
            if (status.num_bytes_written > 0) {
                written += status.num_bytes_written;
            }
            // Mark cycle complete if the FIG says so
            if (status.complete_fig_transmitted) {
                entry->on_cycle_complete(status.num_bytes_written > 0);
            }
            if (status.num_bytes_written == 0) {
                throw std::logic_error("Failed to write FIG 0/0");
            }
            break;
        }
    }
    
    // FIG 0/7 must directly follow FIG 0/0
    for (auto* entry : m_priorities[0].carousel) {
        if (entry->fig->figtype() == 0 && entry->fig->figextension() == 7) {
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(info) << "  FIG 0/7: deadline=" << entry->deadline_ms 
                               << "ms, must_send=" << entry->must_send
                               << ", violated=" << entry->deadline_violated;
#endif
            FillStatus status = entry->fig->fill(buf + written, max_size - written);
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(info) << "  FIG 0/7: wrote " << status.num_bytes_written 
                               << " bytes, complete=" << status.complete_fig_transmitted;
#endif
            if (status.num_bytes_written > 0) {
                written += status.num_bytes_written;
            }
            // If complete, reset the cycle - pass whether data was sent
            if (status.complete_fig_transmitted) {
                entry->on_cycle_complete(status.num_bytes_written > 0);
#if PRIORITY_CAROUSEL_DEBUG
                etiLog.level(info) << "  FIG 0/7: cycle complete, new deadline=" << entry->deadline_ms;
#endif
            }
            break;
        }
    }
    
    return written;
}

int FIGCarouselPriority::select_priority()
{
    if (m_priority_stack.empty()) {
        return -1;
    }
    
    // First: find any priority with counter == 0, highest in stack (front)
    for (int prio : m_priority_stack) {
        if (m_priorities[prio].poll_counter == 0) {
            return prio;
        }
    }
    
    // None at 0: find lowest weighted score
    // Score = poll_counter * stack_position
    // Lower score wins; ties broken by stack position (higher = wins)
    
    int best_prio = -1;
    int best_score = std::numeric_limits<int>::max();
    int position = 1;
    
    for (int prio : m_priority_stack) {
        int score = m_priorities[prio].poll_counter * position;
        if (score < best_score) {
            best_score = score;
            best_prio = prio;
        }
        position++;
    }
    
    return best_prio;
}

int FIGCarouselPriority::cascade_find_priority(int start, bool must_send_only)
{
    // Cascade from start priority down to 8, then wrap to 1 and continue
    // until we reach start again (full circle)
    
    // First pass: start -> NUM_PRIORITIES-1
    for (int p = start; p < NUM_PRIORITIES; p++) {
        if (must_send_only) {
            if (m_priorities[p].has_must_send()) {
                return p;
            }
        } else {
            if (m_priorities[p].has_can_send()) {
                return p;
            }
        }
    }
    
    // Second pass: 1 -> start-1 (wrap around, skip priority 0)
    for (int p = 1; p < start; p++) {
        if (must_send_only) {
            if (m_priorities[p].has_must_send()) {
                return p;
            }
        } else {
            if (m_priorities[p].has_can_send()) {
                return p;
            }
        }
    }
    
    return -1; // Nothing found in any priority
}

void FIGCarouselPriority::on_fig_sent(int priority)
{
    // Decrement all counters
    decrement_all_counters();
    
    // Reset the priority that sent
    m_priorities[priority].poll_counter = m_priorities[priority].poll_reset_value;
    
    // Move to bottom of priority stack
    m_priority_stack.remove(priority);
    m_priority_stack.push_back(priority);
}

void FIGCarouselPriority::decrement_all_counters()
{
    for (int i = 1; i < NUM_PRIORITIES; i++) { // Skip priority 0
        if (m_priorities[i].poll_counter > 0) {
            m_priorities[i].poll_counter--;
        }
        // Clamp at 0 (don't go negative)
    }
}

size_t FIGCarouselPriority::try_send_fig(FIGEntryPriority* entry, uint8_t* buf, size_t max_size)
{
    FillStatus status = entry->fig->fill(buf, max_size);
    size_t written = status.num_bytes_written;
    
    // Validation: FIG should write at least 3 bytes or nothing
    if (written == 1 || written == 2) {
        std::stringstream ss;
        ss << "Assertion error: FIG " << entry->name()
           << " wrote only " << written << " bytes (minimum is 3)";
        throw std::logic_error(ss.str());
    }
    
    if (written > max_size) {
        std::stringstream ss;
        ss << "Assertion error: FIG " << entry->name()
           << " wrote " << written << " bytes but only "
           << max_size << " available";
        throw std::logic_error(ss.str());
    }
    
    // Handle cycle completion
    // Pass whether data was actually sent - only record stats if we transmitted something
    if (status.complete_fig_transmitted) {
        entry->on_cycle_complete(written > 0);
    }
    
    return written;
}

} // namespace FIC
