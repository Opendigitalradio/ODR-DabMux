/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   EDI output,
   Protection, Fragmentation and Transport. (PFT)

   Are supported:
    Reed-Solomon and Fragmentation

   This implements part of PFT as defined ETSI TS 102 821.

   */
/*
   This file is part of the ODR-mmbTools.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "config.h"
#include <vector>
#include <list>
#include <stdexcept>
#include <cstdint>
#include "AFPacket.h"
#include "Log.h"
#include "ReedSolomon.h"
#include "EDIConfig.h"

namespace edi {

typedef std::vector<uint8_t> RSBlock;
typedef std::vector<uint8_t> PFTFragment;

class PFT
{
    public:
        static constexpr int PARITYBYTES = 48;

        PFT();
        PFT(const configuration_t& conf);

        // return a list of PFT fragments with the correct
        // PFT headers
        std::vector< PFTFragment > Assemble(AFPacket af_packet);

        // Apply Reed-Solomon FEC to the AF Packet
        RSBlock Protect(AFPacket af_packet);

        // Cut a RSBlock into several fragments that can be transmitted
        std::vector< std::vector<uint8_t> > ProtectAndFragment(AFPacket af_packet);

    private:
        unsigned int m_k = 207; // length of RS data word
        unsigned int m_m = 3; // number of fragments that can be recovered if lost
        uint16_t m_pseq = 0;
        size_t m_num_chunks = 0;
        bool m_verbose = false;

        // Transport header is always deactivated
        const bool m_transport_header = false;
        const uint16_t m_addr_source = 0;
        const unsigned int m_dest_port = 0;
};

}

