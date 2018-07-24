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
#include <vector>
#include <memory>

namespace FIC {

/* TODO
 * This FIG code is unable to transmit the CEI to announce
 * activation/deactivation of linkage sets.
 * The TransitionHandler.h would be useful for that purpose
 */

// FIG type 0/6
// Service Linking
//
// This feature shall use the SIV signalling (see clause 5.2.2.1). The database
// shall be divided by use of a database key.  Changes to the database shall be
// signalled using the CEI. The first service in the list of services in each
// part of the database, as divided by the database key, shall be a service
// carried in the ensemble. This service is called the key service.
//
// The database key comprises the OE and P/D flags and the S/H, ILS, and LSN
// fields.
class FIG0_6 : public IFIG
{
    public:
        FIG0_6(FIGRuntimeInformation* rti);
        virtual FillStatus fill(uint8_t *buf, size_t max_size);
        virtual FIG_rate repetition_rate() const { return FIG_rate::E; }

        virtual int figtype() const { return 0; }
        virtual int figextension() const { return 6; }

    private:
        FIGRuntimeInformation *m_rti;
        bool m_initialised;

        /* Update the linkageSubsets */
        void update();

        /* A LinkageSet can contain links of different types
         * (DAB, FM, DRM, AMSS), but the FIG needs to send
         * different FIGs for each of those, because the IdLQ flag
         * is common to a list.
         *
         * We reorganise all LinkageSets into subsets that have
         * the same type.
         */
        std::vector<LinkageSet> linkageSubsets;
        std::vector<LinkageSet>::iterator linkageSetFIG0_6;
};

}
