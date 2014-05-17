/*
   Copyright (C) 2013 Matthias P. Braendli
   http://mpb.li

   EDI output.
    This implements an AF Packet as defined ETSI TS 102 821.
    Also see ETSI TS 102 693

   */
/*
   This file is part of CRC-DabMux.

   CRC-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#ifndef _AFPACKET_H_
#define _AFPACKET_H_

#include "config.h"
#include <vector>
#include <stdint.h>
#include "TagItems.h"
#include "TagPacket.h"

#define EDI_AFPACKET_PROTOCOLTYPE_TAGITEMS ('T')

// ETSI TS 102 821, 6.1 AF packet structure
class AFPacket
{
    public:
        AFPacket(char protocolType) : protocol_type(protocolType) {};

        std::vector<uint8_t> Assemble(TagPacket tag_packet);

    private:
        static const bool have_crc = true;

        uint16_t seq; //counter that overflows at 0xFFFF

        char protocol_type;
};

#endif

