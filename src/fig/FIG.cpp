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

#include "FIG.h"

namespace FIC {

int rate_increment_ms(FIG_rate rate)
{
    switch (rate) {
        /* All these values are multiples of 24, so that it is easier to reason
         * about the behaviour when considering ETI frames of 24ms duration
         */
        case FIG_rate::FIG0_0:    return 96;        // Is a special case
        case FIG_rate::A:         return 240;
        case FIG_rate::A_B:       return 480;
        case FIG_rate::B:         return 2400;
        case FIG_rate::C:         return 24000;
        case FIG_rate::D:         return 30000;
        case FIG_rate::E:         return 120000;
    }
    return 1000; //some default value, shouldn't be used
}

} // namespace FIC

