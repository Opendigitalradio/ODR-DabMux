/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   The command-line parser reads settings from a configuration file
   whose definition is given in doc/example.config
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
#ifndef _PARSER_CONFIGFILE
#define _PARSER_CONFIGFILE

#include <vector>
#include <string>
#include "MuxElements.h"
#include "DabMux.h"
#include <boost/property_tree/ptree.hpp>

void parse_configfile(std::string configuration_file,
        std::vector<dabOutput*> &outputs,
        dabEnsemble* ensemble,
        bool* enableTist,
        unsigned* FICL,
        bool* factumAnalyzer,
        unsigned long* limit,
        BaseRemoteController** rc,
        int* statsServerPort,
        edi_configuration_t* edi);

void setup_subchannel_from_ptree(dabSubchannel* subchan,
        boost::property_tree::ptree &pt,
        dabEnsemble* ensemble,
        std::string subchanuid,
        BaseRemoteController* rc);

#endif

