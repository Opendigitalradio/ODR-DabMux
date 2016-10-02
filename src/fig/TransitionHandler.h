/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <memory>

namespace FIC {

// Some FIGs need to adapt their rate or their contents depending
// on if some data entries are stable or currently undergoing a
// change. The TransitionHandler keeps track of which entries
// are in what state.
template<class T>
class TransitionHandler {
    public:
        // update_state must be called once per ETI frame, and will
        // move entries from new to repeated to disabled depending
        // on their is_active() return value.
        void update_state(int timeout, std::vector<std::shared_ptr<T> > all_entries)
        {
            for (const auto& entry : all_entries) {
                if (entry->is_active()) {
                    if (repeated_entries.count(entry) > 0) {
                        // We are currently announcing this entry
                        continue;
                    }

                    if (new_entries.count(entry) > 0) {
                        // We are currently announcing this entry at a
                        // fast rate. Handle timeout:
                        new_entries[entry] -= 1;
                        if (new_entries[entry] <= 0) {
                            repeated_entries.insert(entry);
                            new_entries.erase(entry);
                        }
                        continue;
                    }

                    // unlikely
                    if (disabled_entries.count(entry) > 0) {
                        new_entries[entry] = timeout;
                        disabled_entries.erase(entry);
                        continue;
                    }

                    // It's a new entry!
                    new_entries[entry] = timeout;
                }
                else { // Not active
                    if (disabled_entries.count(entry) > 0) {
                        disabled_entries[entry] -= 1;
                        if (disabled_entries[entry] <= 0) {
                            disabled_entries.erase(entry);
                        }
                        continue;
                    }

                    if (repeated_entries.count(entry) > 0) {
                        // We are currently announcing this entry
                        disabled_entries[entry] = timeout;
                        repeated_entries.erase(entry);
                        continue;
                    }

                    // unlikely
                    if (new_entries.count(entry) > 0) {
                        // We are currently announcing this entry at a
                        // fast rate. We must stop announcing it
                        disabled_entries[entry] = timeout;
                        new_entries.erase(entry);
                        continue;
                    }
                }
            }
        }
        // The FIG that needs the information about what state an entry is in
        // can read from the following data structures. It shall not modify them.

        /* Map to frame count */
        std::map<
            std::shared_ptr<T>,int> new_entries;

        std::set<
            std::shared_ptr<T> > repeated_entries;

        /* Map to frame count */
        std::map<
            std::shared_ptr<T>,int> disabled_entries;
};

} // namespace FIC

