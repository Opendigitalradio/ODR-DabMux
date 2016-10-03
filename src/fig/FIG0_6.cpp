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

#include "fig/FIG0_6.h"
#include "utils.h"

namespace FIC {

FIG0_6::FIG0_6(FIGRuntimeInformation *rti) :
    m_rti(rti),
    m_initialised(false)
{
}

FillStatus FIG0_6::fill(uint8_t *buf, size_t max_size)
{
    FillStatus fs;
    ssize_t remaining = max_size;
    auto ensemble = m_rti->ensemble;

    if (not m_initialised) {
        update();
        m_initialised = true;
    }

    FIGtype0* fig0 = NULL;

    for (; linkageSetFIG0_6 != linkageSubsets.end();
            ++linkageSetFIG0_6) {

        const bool PD = false;
        const bool ILS = linkageSetFIG0_6->international;

        // need to add key service to num_ids
        const size_t num_ids = 1 + linkageSetFIG0_6->id_list.size();

        const size_t headersize = sizeof(struct FIGtype0_6_header);
        const int required_size = sizeof(struct FIGtype0_6) +
            (num_ids > 0 ?
             headersize + (PD == 0 ? (ILS == 0 ? 2*num_ids : 3*num_ids) : 4*num_ids) :
             0);

        if (fig0 == NULL) {
            if (remaining < 2 + required_size) {
                break;
            }
            fig0 = (FIGtype0*)buf;
            fig0->FIGtypeNumber = 0;
            fig0->Length = 1;
            fig0->CN = 0;
            fig0->OE = 0;
            fig0->PD = PD;
            fig0->Extension = 6;

            buf += 2;
            remaining -= 2;
        }
        else if (remaining < required_size) {
            break;
        }

        FIGtype0_6 *fig0_6 = (FIGtype0_6*)buf;

        fig0_6->IdListFlag = (num_ids > 0);
        fig0_6->LA = linkageSetFIG0_6->active;
        fig0_6->SH = linkageSetFIG0_6->hard;
        fig0_6->ILS = ILS;
        fig0_6->LSN = linkageSetFIG0_6->lsn;

        fig0->Length += sizeof(struct FIGtype0_6);
        buf += sizeof(struct FIGtype0_6);
        remaining -= sizeof(struct FIGtype0_6);

        if (num_ids > 0) {
            FIGtype0_6_header *header = (FIGtype0_6_header*)buf;
            header->rfu = 0;
            if (num_ids > 0x0F) {
                etiLog.log(error, "Too large number of links for linkage set 0x%04x",
                        linkageSetFIG0_6->lsn);
                throw MuxInitException();
            }

            // update() guarantees us that all entries in a linkage set
            // have the same type
            for (const auto& l : linkageSetFIG0_6->id_list) {
                if (l.type != linkageSetFIG0_6->id_list.front().type) {
                    etiLog.log(error, "INTERNAL ERROR: invalid linkage subset 0x%04x",
                        linkageSetFIG0_6->lsn);
                    throw std::runtime_error("INTERNAL ERROR");
                }
            }

            switch (linkageSetFIG0_6->id_list.front().type) {
                case ServiceLinkType::DAB: header->IdLQ  = FIG0_6_IDLQ_DAB; break;
                case ServiceLinkType::FM: header->IdLQ   = FIG0_6_IDLQ_RDS; break;
                case ServiceLinkType::DRM: header->IdLQ  = FIG0_6_IDLQ_DRM_AMSS; break;
                case ServiceLinkType::AMSS: header->IdLQ = FIG0_6_IDLQ_DRM_AMSS; break;
            }
            header->rfa = 0;
            header->num_ids = num_ids;

            fig0->Length += headersize;
            buf += headersize;
            remaining -= headersize;

            // TODO insert key service first
            const std::string keyserviceuid = linkageSetFIG0_6->keyservice;
            const auto& keyservice = std::find_if(
                    ensemble->services.begin(),
                    ensemble->services.end(),
                    [&](const std::shared_ptr<DabService> srv) {
                        return srv->uid == keyserviceuid;
                    });

            if (keyservice == ensemble->services.end()) {
                etiLog.log(error, "Invalid key service %s in linkage set 0x%04x",
                        keyserviceuid.c_str(), linkageSetFIG0_6->lsn);
                throw MuxInitException();
            }

            if (not PD and not ILS) {
                buf[0] = (*keyservice)->id >> 8;
                buf[1] = (*keyservice)->id & 0xFF;
                fig0->Length += 2;
                buf += 2;
                remaining -= 2;

                for (const auto& l : linkageSetFIG0_6->id_list) {
                    buf[0] = l.id >> 8;
                    buf[1] = l.id & 0xFF;
                    fig0->Length += 2;
                    buf += 2;
                    remaining -= 2;
                }
            }
            if (not PD and ILS) {
                buf[0] = ensemble->ecc;
                buf[1] = (*keyservice)->id >> 8;
                buf[2] = (*keyservice)->id & 0xFF;
                fig0->Length += 3;
                buf += 3;
                remaining -= 3;

                for (const auto& l : linkageSetFIG0_6->id_list) {
                    buf[0] = l.ecc;
                    buf[1] = l.id >> 8;
                    buf[2] = l.id & 0xFF;
                    fig0->Length += 3;
                    buf += 3;
                    remaining -= 3;
                }
            }
            else { // PD == true
                // TODO if IdLQ is 11, MSB shall be zero
                buf[0] = (*keyservice)->id >> 24;
                buf[1] = (*keyservice)->id >> 16;
                buf[2] = (*keyservice)->id >> 8;
                buf[3] = (*keyservice)->id & 0xFF;
                fig0->Length += 4;
                buf += 4;
                remaining -= 4;

                for (const auto& l : linkageSetFIG0_6->id_list) {
                    buf[0] = l.id >> 24;
                    buf[1] = l.id >> 16;
                    buf[2] = l.id >> 8;
                    buf[3] = l.id & 0xFF;
                    fig0->Length += 4;
                    buf += 4;
                    remaining -= 4;
                }
            }
        }
    }

    if (linkageSetFIG0_6 == linkageSubsets.end()) {
        update();
        fs.complete_fig_transmitted = true;
    }

    fs.num_bytes_written = max_size - remaining;
    return fs;
}

void FIG0_6::update()
{
    linkageSubsets.clear();

    for (const auto& linkageset : m_rti->ensemble->linkagesets) {
        const auto lsn = linkageset->data.lsn;

        for (const auto& link : linkageset->data.id_list) {
            const auto type = link.type;

            const auto subset =
                std::find_if(linkageSubsets.begin(), linkageSubsets.end(),
                    [&](const LinkageSetData& l) {
                        return not l.id_list.empty() and
                            l.lsn == lsn and l.id_list.front().type == type;
                    });

            if (subset == linkageSubsets.end()) {
                // A subset with that LSN and type does not exist yet
                linkageSubsets.push_back( linkageset->data.filter_type(type) );
            }
        }
    }

    linkageSetFIG0_6 = linkageSubsets.begin();

#if 0
    etiLog.log(info, " Linkage Sets");
    for (const auto& lsd : linkageSubsets) {

        etiLog.log(info,      "   LSN 0x%04x", lsd.lsn);
        etiLog.level(info) << "   active " << (lsd.active ? "true" : "false");
        etiLog.level(info) << "   " << (lsd.hard ? "hard" : "soft");
        etiLog.level(info) << "   international " << (lsd.international ? "true" : "false");
        etiLog.level(info) << "   key service " << lsd.keyservice;

        etiLog.level(info) << "   ID list";
        for (const auto& link : lsd.id_list) {
            switch (link.type) {
                case ServiceLinkType::DAB:
                    etiLog.level(info) << "    type DAB";
                    break;
                case ServiceLinkType::FM:
                    etiLog.level(info) << "    type FM";
                    break;
                case ServiceLinkType::DRM:
                    etiLog.level(info) << "    type DRM";
                    break;
                case ServiceLinkType::AMSS:
                    etiLog.level(info) << "    type AMSS";
                    break;
            }
            etiLog.log(info,      "    id 0x%04x", link.id);
            if (lsd.international) {
                etiLog.log(info,      "    ecc 0x%04x", link.ecc);
            }
        }
    }
#endif
}

}

