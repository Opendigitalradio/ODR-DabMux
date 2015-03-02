/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Includes modifications
   2012, Matthias P. Braendli, matthias.braendli@mpb.li

   The command-line parser reads the parameters given on the command
   line, and builds an ensemble.
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
#ifndef _PARSER_CMDLINE
#define _PARSER_CMDLINE

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <vector>
#include "MuxElements.h"

#define ENABLE_CMDLINE_OPTIONS 0

#if ENABLE_CMDLINE_OPTIONS

bool parse_cmdline(char **argv,
        int argc,
        std::vector<dabOutput*> &outputs,
        dabEnsemble* ensemble,
        bool* enableTist,
        unsigned* FICL,
        bool* factumAnalyzer,
        unsigned long* limit
        );

#endif

#endif
