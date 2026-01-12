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
    for (auto* entry : carousel) {
        // A FIG can send if it has data (even if must_send is false)
        // We always have something to send - the FIG will return 0 bytes if nothing
        return entry;
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
     * Priority assignments:
     * 
     * Priority 0 (every frame): 0/0, 0/7 - Critical MCI
     * Priority 1 (reset 2):     0/1, 0/2 - Core MCI (subchannel/service organisation)
     * Priority 2 (reset 4):     0/3      - Service component in packet mode
     * Priority 3 (reset 8):     1/1      - Programme service labels
     * Priority 4 (reset 16):    0/8, 0/13 - Service component global definition, user apps
     * Priority 5 (reset 32):    0/5, 0/9, 0/10, 0/17, 0/18 - Metadata
     * Priority 6 (reset 64):    1/0, 1/4, 1/5, 2/0, 2/1, 2/4, 2/5 - Labels
     * Priority 7 (reset 128):   0/6, 0/14, 0/19 - Service linking, FEC, announcements
     * Priority 8 (reset 256):   0/21, 0/24 - Frequency info, OE services
     * Priority 9 (reset 512):   (reserved for future/dynamic use)
     */
    
    // Priority 0: Critical (every frame)
    add_fig_to_priority(m_fig0_0, 0);
    add_fig_to_priority(m_fig0_7, 0);
    
    // Priority 1: Core MCI
    add_fig_to_priority(m_fig0_1, 1);
    add_fig_to_priority(m_fig0_2, 1);
    
    // Priority 2: Packet mode
    add_fig_to_priority(m_fig0_3, 2);
    
    // Priority 3: Service labels
    add_fig_to_priority(m_fig1_1, 3);
    
    // Priority 4: Component details
    add_fig_to_priority(m_fig0_8, 4);
    add_fig_to_priority(m_fig0_13, 4);
    
    // Priority 5: Metadata
    add_fig_to_priority(m_fig0_5, 5);
    add_fig_to_priority(m_fig0_9, 5);
    add_fig_to_priority(m_fig0_10, 5);
    add_fig_to_priority(m_fig0_17, 5);
    add_fig_to_priority(m_fig0_18, 5);
    add_fig_to_priority(m_fig0_20, 5);  // SCI - Service Component Information
    
    // Priority 6: Labels (ensemble, component, data, extended)
    add_fig_to_priority(m_fig1_0, 6);
    add_fig_to_priority(m_fig1_4, 6);
    add_fig_to_priority(m_fig1_5, 6);
    add_fig_to_priority(m_fig2_0, 6);
    add_fig_to_priority(m_fig2_1, 6);
    add_fig_to_priority(m_fig2_4, 6);
    add_fig_to_priority(m_fig2_5, 6);
    
    // Priority 7: Linking and announcements
    add_fig_to_priority(m_fig0_6, 7);
    add_fig_to_priority(m_fig0_14, 7);
    add_fig_to_priority(m_fig0_19, 7);
    
    // Priority 8: Frequency/OE
    add_fig_to_priority(m_fig0_21, 8);
    add_fig_to_priority(m_fig0_24, 8);
    
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
        entry->tick_deadline(elapsed_ms);
        
        // Check if a new cycle should start
        if (!entry->must_send && entry->deadline_ms <= 0) {
            entry->start_new_cycle();
            entry->deadline_ms = entry->rate_ms; // Reset for next cycle
        }
    }
}

void FIGCarouselPriority::check_and_log_deadlines(uint64_t current_frame)
{
    if ((current_frame % 250) != 0) {
        return;
    }
    
    std::stringstream ss;
    bool any_violated = false;
    
    for (auto& entry : m_all_entries) {
        if (entry->deadline_violated) {
            ss << " " << entry->name();
            entry->deadline_violated = false;
            any_violated = true;
        }
    }
    
    if (any_violated) {
        etiLog.level(info) << "FIGCarouselPriority: Could not respect repetition rates for FIGs:" << ss.str();
    }
}

size_t FIGCarouselPriority::write_fibs(uint8_t* buf, uint64_t current_frame, bool fib3_present)
{
    m_rti.currentFrame = current_frame;
    
    const int fibCount = fib3_present ? 4 : 3;
    const int framephase = current_frame % 4;
    
#if PRIORITY_CAROUSEL_DEBUG
    if ((current_frame % 50) == 0) { // Log every 50 frames (~1.2 sec)
        std::stringstream ss;
        ss << "Frame " << current_frame << " (phase " << framephase << ") FIG deadlines: ";
        for (auto& entry : m_all_entries) {
            if (entry->must_send || entry->deadline_ms < 50) {
                ss << entry->name() << "(" << entry->deadline_ms << "ms"
                   << (entry->must_send ? ",MUST" : "") 
                   << (entry->deadline_violated ? ",VIOL" : "") << ") ";
            }
        }
        etiLog.level(debug) << ss.str();
    }
#endif
    
    for (int fib = 0; fib < fibCount; fib++) {
        memset(buf, 0x00, 30);
        size_t figSize = fill_fib(buf, 30, framephase);
        
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
    
    // Tick all deadline monitors AFTER sending (24ms per frame)
    tick_all_deadlines(24);
    
    // Periodically log missed deadlines
    check_and_log_deadlines(current_frame);
    
    return 32 * fibCount;
}

size_t FIGCarouselPriority::fill_fib(uint8_t* buf, size_t max_size, int framephase)
{
    size_t written = 0;
    
#if PRIORITY_CAROUSEL_DEBUG
    std::stringstream fib_log;
    fib_log << "FIB fill (phase " << framephase << "): ";
#endif
    
    // Step 1: Priority 0 always first (FIG 0/0, 0/7)
    size_t p0_written = send_priority_zero(buf, max_size, framephase);
    written += p0_written;
    
#if PRIORITY_CAROUSEL_DEBUG
    if (p0_written > 0) {
        fib_log << "P0:" << p0_written << "B ";
    }
#endif
    
    size_t remaining = max_size - written;
    uint8_t* pbuf = buf + written;
    
    // Step 2: Must-send pass - send FIGs that are due
    int attempts = 0;
    const int max_attempts = NUM_PRIORITIES * 2; // Prevent infinite loop
    
    while (remaining > 2 && attempts < max_attempts) {
        attempts++;
        
        // Find any priority with must_send FIGs
        FIGEntryPriority* entry = nullptr;
        int send_prio = -1;
        
        // First try the selected priority
        int prio = select_priority();
        if (prio >= 0) {
            entry = m_priorities[prio].find_must_send();
            if (entry) {
                send_prio = prio;
            }
        }
        
        // If selected priority has nothing, search all priorities
        if (!entry) {
            for (int p = 1; p < NUM_PRIORITIES; p++) {
                entry = m_priorities[p].find_must_send();
                if (entry) {
                    send_prio = p;
                    break;
                }
            }
        }
        
        if (!entry) break; // No more must_send anywhere
        
        size_t bytes = try_send_fig(entry, pbuf, remaining);
        if (bytes > 0) {
#if PRIORITY_CAROUSEL_DEBUG
            fib_log << entry->name() << ":" << bytes << "B ";
#endif
            written += bytes;
            remaining -= bytes;
            pbuf += bytes;
            m_priorities[send_prio].move_to_back(entry);
            on_fig_sent(send_prio);
        } else {
            // FIG couldn't write (no space or no data), move to back to try others
            m_priorities[send_prio].move_to_back(entry);
        }
    }
    
    // Step 3: Can-send pass (fill remaining space opportunistically)
    attempts = 0;
    while (remaining > 2 && attempts < max_attempts) {
        attempts++;
        
        int prio = select_priority();
        if (prio < 0) break;
        
        FIGEntryPriority* entry = m_priorities[prio].find_can_send();
        
        if (!entry) {
            // Try other priorities
            for (int p = 1; p < NUM_PRIORITIES; p++) {
                if (!m_priorities[p].carousel.empty()) {
                    entry = m_priorities[p].carousel.front();
                    prio = p;
                    break;
                }
            }
        }
        
        if (!entry) break;
        
        size_t bytes = try_send_fig(entry, pbuf, remaining);
        if (bytes > 0) {
#if PRIORITY_CAROUSEL_DEBUG
            fib_log << entry->name() << ":" << bytes << "B(opt) ";
#endif
            written += bytes;
            remaining -= bytes;
            pbuf += bytes;
            m_priorities[prio].move_to_back(entry);
            on_fig_sent(prio);
        } else {
            // FIG wrote nothing, move on
            m_priorities[prio].move_to_back(entry);
            break; // No more space or nothing to send
        }
    }
    
#if PRIORITY_CAROUSEL_DEBUG
    fib_log << "= " << written << "/" << max_size;
    etiLog.level(debug) << fib_log.str();
#endif
    
    return written;
}

size_t FIGCarouselPriority::send_priority_zero(uint8_t* buf, size_t max_size, int framephase)
{
    size_t written = 0;
    
    // FIG 0/0 and 0/7 only in framephase 0 (every 96ms in mode I)
    if (framephase != 0) {
        return 0;
    }
    
#if PRIORITY_CAROUSEL_DEBUG
    etiLog.level(debug) << "send_priority_zero: framephase=0, sending 0/0 and 0/7";
#endif
    
    // Find and send FIG 0/0
    for (auto* entry : m_priorities[0].carousel) {
        if (entry->fig->figtype() == 0 && entry->fig->figextension() == 0) {
            FillStatus status = entry->fig->fill(buf + written, max_size - written);
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(debug) << "  FIG 0/0: wrote " << status.num_bytes_written 
                               << " bytes, complete=" << status.complete_fig_transmitted
                               << ", deadline was " << entry->deadline_ms << "ms";
#endif
            if (status.num_bytes_written > 0) {
                written += status.num_bytes_written;
                if (status.complete_fig_transmitted) {
                    entry->on_cycle_complete();
                }
            }
            else {
                throw std::logic_error("Failed to write FIG 0/0");
            }
            break;
        }
    }
    
    // FIG 0/7 must directly follow FIG 0/0
    for (auto* entry : m_priorities[0].carousel) {
        if (entry->fig->figtype() == 0 && entry->fig->figextension() == 7) {
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(debug) << "  FIG 0/7: deadline=" << entry->deadline_ms 
                               << "ms, must_send=" << entry->must_send
                               << ", violated=" << entry->deadline_violated;
#endif
            FillStatus status = entry->fig->fill(buf + written, max_size - written);
#if PRIORITY_CAROUSEL_DEBUG
            etiLog.level(debug) << "  FIG 0/7: wrote " << status.num_bytes_written 
                               << " bytes, complete=" << status.complete_fig_transmitted;
#endif
            if (status.num_bytes_written > 0) {
                written += status.num_bytes_written;
            }
            // If complete (even with 0 bytes - means nothing to send), reset the cycle
            if (status.complete_fig_transmitted) {
                entry->on_cycle_complete();
#if PRIORITY_CAROUSEL_DEBUG
                etiLog.level(debug) << "  FIG 0/7: cycle complete, new deadline=" << entry->deadline_ms;
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
    
#if PRIORITY_CAROUSEL_DEBUG
    if (written > 0) {
        etiLog.level(debug) << "FIGCarouselPriority: " << entry->name()
            << " wrote " << written << " bytes"
            << (status.complete_fig_transmitted ? " (complete)" : " (partial)");
    }
#endif
    
    if (status.complete_fig_transmitted) {
        entry->on_cycle_complete();
    }
    
    return written;
}

} // namespace FIC
