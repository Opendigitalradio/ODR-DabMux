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

#include "fig/FIG0structs.h"
#include "fig/FIG0_19.h"
#include "utils.h"

namespace FIC {

struct FIGtype0_19 {
    uint8_t  ClusterId;
    uint16_t ASw;
    uint8_t  SubChId:6;
    uint8_t  RegionFlag:1; // shall be zero
    uint8_t  NewFlag:1;
    // Region and RFa not supported
} PACKED;


FIG0_19::FIG0_19(FIGRuntimeInformation *rti) :
    m_rti(rti)
{ }

#define FIG0_19_TRACE discard

FillStatus FIG0_19::fill(uint8_t *buf, size_t max_size)
{
    using namespace std;

    auto ensemble = m_rti->ensemble;

    m_transition.update_state(std::chrono::seconds(2), ensemble->clusters);

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
    etiLog.level(FIG0_19_TRACE) << "FIG0_19::loop with " << allclusters.size() <<
        " clusters";
    for (auto& cluster : allclusters) {
        etiLog.level(FIG0_19_TRACE) << "FIG0_19::cluster " << cluster->cluster_id;

        if (fig0 == NULL) {
            if (remaining < 2 + length_0_19) {
                fs.complete_fig_transmitted = false;
                etiLog.level(FIG0_19_TRACE) << "FIG0_19::no space FIG0";
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
            etiLog.level(FIG0_19_TRACE) << "FIG0_19::no space FIG0/19";
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

        /* From the crc-mmbtools google groups, 2019-07-11, L. Cornell:
         *
         * Long ago, there was a defined use for the New flag - it was intended
         * to indicate whether the announcement was new or was a repeated
         * announcement.  But the problem is that it doesn't really help
         * receivers because they might tune to the ensemble at any time, and
         * might tune to a service that may be interrupted at any time.  So
         * some years ago it was decided that the New flag would not longer be
         * used in transmissions.  The setting was fixed to be 1 because some
         * receivers would have never switched to the announcement if the flag
         * was set to 0.
         */
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

        etiLog.level(FIG0_19_TRACE) << "FIG0_19::advance " << length_0_19;

        fig0->Length += length_0_19;
        buf += length_0_19;
        remaining -= length_0_19;
    }

    fs.num_bytes_written = max_size - remaining;
    etiLog.level(FIG0_19_TRACE) << "FIG0_19::out " << fs.num_bytes_written;
    return fs;
}

FIG_rate FIG0_19::repetition_rate() const
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
