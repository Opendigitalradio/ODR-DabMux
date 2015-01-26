/*
   Copyright (C) 2013,2014 Matthias P. Braendli
   http://mpb.li

   EDI output,
   Protection, Fragmentation and Transport. (PFT)

   Are supported:
    Reed-Solomon and Fragmentation

   This implements part of PFT as defined ETSI TS 102 821.

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

#ifndef _PFT_H_
#define _PFT_H_

#include "config.h"
#include <vector>
#include <list>
#include <stdexcept>
#include <stdint.h>
#include "AFPacket.h"
#include "Log.h"
#include "ReedSolomon.h"
#include "dabOutput/dabOutput.h"

typedef std::vector<uint8_t> RSBlock;
typedef std::vector<uint8_t> PFTFragment;

class PFT
{
    public:
        static const int ParityBytes = 48;

        PFT(unsigned int RSDataWordLength,
            unsigned int NumRecoverableFragments,
            const edi_configuration_t &conf) :
            m_k(RSDataWordLength),
            m_m(NumRecoverableFragments),
            m_dest_port(conf.dest_port),
            m_pseq(0),
            m_verbose(conf.verbose)
        {
            if (m_k > 207) {
                etiLog.level(warn) <<
                        "EDI PFT: maximum chunk size is 207.";
                throw std::out_of_range("EDI PFT Chunk size too large.");
            }

            if (m_m > 5) {
                etiLog.level(warn) <<
                    "EDI PFT: high number of recoverable fragments"
                    " may lead to large overhead";
                // See TS 102 821, 7.2.1 Known values, list entry for 'm'
            }
        }

        // return a list of PFT fragments with the correct
        // PFT headers
        std::vector< PFTFragment > Assemble(AFPacket af_packet);

        // Apply Reed-Solomon FEC to the AF Packet
        RSBlock Protect(AFPacket af_packet);

        // Cut a RSBlock into several fragments that can be transmitted
        std::vector< std::vector<uint8_t> > ProtectAndFragment(AFPacket af_packet);

    private:
        unsigned int m_k; // length of RS data word
        unsigned int m_m; // number of fragments that can be recovered if lost

        unsigned int m_dest_port; // Destination port for transport header

        uint16_t m_pseq;

        size_t m_num_chunks;

        bool m_verbose;

};

#endif

