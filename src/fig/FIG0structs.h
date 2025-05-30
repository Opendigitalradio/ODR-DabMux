/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2020
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
#include "fig/FIG.h"

constexpr uint16_t FIG0_13_APPTYPE_SLIDESHOW  = 0x2;
constexpr uint16_t FIG0_13_APPTYPE_WEBSITE    = 0x3;
constexpr uint16_t FIG0_13_APPTYPE_TPEG       = 0x4;
constexpr uint16_t FIG0_13_APPTYPE_DGPS       = 0x5;
constexpr uint16_t FIG0_13_APPTYPE_TMC        = 0x6;
constexpr uint16_t FIG0_13_APPTYPE_SPI        = 0x7;
constexpr uint16_t FIG0_13_APPTYPE_DABJAVA    = 0x8;
constexpr uint16_t FIG0_13_APPTYPE_JOURNALINE = 0x44a;


struct FIGtype0 {
    uint8_t Length:5;
    uint8_t FIGtypeNumber:3;
    uint8_t Extension:5;
    uint8_t PD:1;
    uint8_t OE:1;
    uint8_t CN:1;
} PACKED;

