/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   This file declares several structures used in the multiplexer,
   and defines default values for some parameters.
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

#include <stdint.h>
#include <string>
#include <vector>
#include "DabMultiplexer.h"
#include "RemoteControl.h"
#include "dabOutput/dabOutput.h"
#include "input/inputs.h"
#include "Eti.h"
#include "MuxElements.h"

#ifdef _WIN32
#   include <time.h>
#else
#   include <sys/time.h>
#endif

