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

#include "fig/FIG0_19.h"
#include "utils.h"

namespace FIC {

FIG0_19::FIG0_19(FIGRuntimeInformation *rti) :
    m_rti(rti)
{ }

FillStatus FIG0_19::fill(uint8_t *buf, size_t max_size)
{
    using namespace std;

    auto ensemble = m_rti->ensemble;

    // We are called every 24ms, and must timeout after 2s
    const int timeout = 2000/24;

    m_transition.update_state(timeout, ensemble->clusters);

    FillStatus fs;
    ssize_t remaining = max_size;

    FIGtype0* fig0 = NULL;

    // Combine all clusters into one list
    set<AnnouncementCluster*> allclusters;
    for (const auto& cluster : m_transition.new_entries) {
        allclusters.insert(cluster.first.get());
    }
    for (const auto& cluster : m_transition.repeated_entries) {
        allclusters.insert(cluster.get());
    }
    for (const auto& cluster : m_transition.disabled_entries) {
        allclusters.insert(cluster.first.get());
    }

    const int length_0_19 = 4;
    fs.complete_fig_transmitted = true;
    for (auto& cluster : allclusters) {

        if (fig0 == NULL) {
            if (remaining < 2 + length_0_19) {
                fs.complete_fig_transmitted = false;
                break;
            }

            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN = 0;
            fig0->OE = 0;
            fig0->PD = 0;
            fig0->Extension = 19;
            buf += 2;
            remaining -= 2;
        }
        else if (remaining < length_0_19) {
            fs.complete_fig_transmitted = false;
            break;
        }


        auto fig0_19 = (FIGtype0_19*)buf;
        fig0_19->ClusterId = cluster->cluster_id;
        if (cluster->is_active()) {
            fig0_19->ASw = htons(cluster->flags);
        }
        else {
            fig0_19->ASw = 0;
        }
        fig0_19->NewFlag = 1;
        fig0_19->RegionFlag = 0;
        fig0_19->SubChId = 0;
        bool found = false;

        for (const auto& subchannel : ensemble->subchannels) {
            if (subchannel->uid == cluster->subchanneluid) {
                fig0_19->SubChId = subchannel->id;
                found = true;
                break;
            }
        }
        if (not found) {
            etiLog.level(warn) << "FIG0/19: could not find subchannel " <<
                cluster->subchanneluid << " for cluster " <<
                (int)cluster->cluster_id;
            continue;
        }

        fig0->Length += length_0_19;
        buf += length_0_19;
        remaining -= length_0_19;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

FIG_rate FIG0_19::repetition_rate(void)
{
    if (    m_transition.new_entries.size() > 0 or
            m_transition.disabled_entries.size() ) {
        return FIG_rate::A_B;
    }
    else {
        return FIG_rate::B;
    }
}

}
