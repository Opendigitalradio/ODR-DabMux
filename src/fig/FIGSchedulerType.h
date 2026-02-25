/*
   Copyright (C) 2026
   Samuel Hunt, Maxxwave Ltd. sam@maxxwave.co.uk

   Copyright (C) 2026
   Matthias P. Braendli, matthias.braendli@mpb.li

   FIG Scheduler type definitions.
   Separated into own header to avoid circular dependencies with MuxElements.h
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

#include <string>

namespace FIC {

/* Scheduler type selection.
 *
 * Classic: Original ODR-DabMux deadline-based scheduler
 * ClassicRateTuning: Original ODR-DabMux deadline-based scheduler,
 *                    plus per-fig correction factors.
 * Priority: New priority-based scheduler with weighted round-robin
 */
enum class FIGSchedulerType {
    Classic,
    ClassicRateTuning,
    Priority
};

// Parse scheduler type from config string
// Returns Classic if string is unrecognised
FIGSchedulerType parse_scheduler_type(const std::string& type_str);

// Convert scheduler type to string for logging
std::string scheduler_type_to_string(FIGSchedulerType type);

} // namespace FIC
