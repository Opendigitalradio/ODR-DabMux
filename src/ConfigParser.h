/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    The Configuration parser sets up the ensemble according
     to the configuration given in a boost property tree, which
     is directly derived from a config file.

     The format of the configuration is given in doc/example.mux
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
#ifndef __CONFIG_PARSER_H_
#define __CONFIG_PARSER_H_

#include <vector>
#include <string>
#include "MuxElements.h"
#include "DabMux.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>

void parse_ptree(boost::property_tree::ptree& pt,
        boost::shared_ptr<dabEnsemble> ensemble,
        boost::shared_ptr<BaseRemoteController> rc);

void setup_subchannel_from_ptree(dabSubchannel* subchan,
        boost::property_tree::ptree &pt,
        boost::shared_ptr<dabEnsemble> ensemble,
        std::string subchanuid,
        boost::shared_ptr<BaseRemoteController> rc);

#endif

