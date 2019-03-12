/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
   2011, 2012 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

   This file contains a set of utility functions that are used to show
   useful information to the user, and handles time and date for the
   the signalling.
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

#include <string>
#include <vector>
#include <cstdio>
#include <memory>
#include "MuxElements.h"

/* Must be called once per ETI frame to update the time */
void update_dab_time(void);
void get_dab_time(time_t *time, uint32_t *millis);

/* Convert a date and time into the modified Julian date
 * used in FIG 0/10
 *
 * Year is four digit format.
 * Months are Jan=1, Feb=2, etc.
 * First day of the month is 1, as usual.
 *
 * Returns corresponding MJD
 */
uint32_t gregorian2mjd(int year, int month, int day);

/* Shows the introductory header on program start */
void header_message();

/* The usage information refers to the command-line
 * ensemble definition, and explains how to create
 * an ensemble without using a configuration file
 */
void printUsage(char *name, FILE* out = stderr);

/* This usage information explains how to run the program
 * with a configuration file
 */
void printUsageConfigfile(char *name, FILE* out = stderr);

/* The following four utility functions display a
 * description of all outputs, services, components
 * resp. subchannels*/
void printOutputs(const std::vector<std::shared_ptr<DabOutput> >& outputs);

void printServices(const std::vector<std::shared_ptr<DabService> >& services);

void printComponents(const vec_sp_component& components);

void printSubchannels(const vec_sp_subchannel& subchannels);

/* Print information about the whole ensemble */
void printEnsemble(const std::shared_ptr<dabEnsemble>& ensemble);

/* Print detailed component information */
void printComponent(const std::shared_ptr<DabComponent>& component);

long hexparse(const std::string& input);

