/*
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

#include "fig/FIGSchedulerType.h"
#include "Log.h"
#include <algorithm>
#include <cctype>

namespace FIC {

FIGSchedulerType parse_scheduler_type(const std::string& type_str)
{
    std::string lower = type_str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    if (lower == "priority") {
        return FIGSchedulerType::Priority;
    }
    else if (lower == "classic" || lower == "default" || lower.empty()) {
        return FIGSchedulerType::Classic;
    }
    else {
        etiLog.level(warn) << "Unknown FIC scheduler type '" << type_str 
                           << "', defaulting to classic";
        return FIGSchedulerType::Classic;
    }
}

std::string scheduler_type_to_string(FIGSchedulerType type)
{
    switch (type) {
        case FIGSchedulerType::Classic:
            return "classic";
        case FIGSchedulerType::Priority:
            return "priority";
        default:
            return "unknown";
    }
}

} // namespace FIC
