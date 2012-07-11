/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#include "dabUtils.h"

long gregorian2mjd(int year,int month,int day)
{
	long MJD;

	//This is the algorithm for the JD, just substract 2400000.5 for MJD
	year += 8000;
	if(month < 3) {
		year--;
		month += 12;
	}
	MJD = (year * 365) + (year / 4) - (year / 100) + (year / 400) - 1200820
		+ ((month * 153 + 3) / 5) - 92 + (day - 1);

	return (long)(MJD - 2400000.5); //truncation, loss of data OK!
}


void dumpBytes(void* data, int size, FILE* out)
{
	fprintf(out, "Packet of %i bytes", size);

	for (int index = 0; index < size; ++index) {
		if ((index % 8) == 0) {
			fprintf(out, "\n 0x%.4x(%.4i):", index, index);
		}
		fprintf(out, " 0x%.2x", ((unsigned char*)data)[index]);
	}
	fprintf(out, "\n\n");
}
