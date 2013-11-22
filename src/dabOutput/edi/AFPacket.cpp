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
#include "config.h"
#include "AFPacket.h"
#include <vector>
#include <string>
#include <stdint.h>

std::vector<uint8_t> AFPacket::Assemble()
{
    header.sync = 'A' << 8 | 'F';
    header.seq = 0;
    header.ar_cf = 0;
    header.ar_maj = 1;
    header.ar_min = 0;
    header.pt = 'T';

    std::string pack_data("Nothingyetpleasecomelater");
    std::vector<uint8_t> packet(pack_data.begin(), pack_data.end());
    return packet;
}

