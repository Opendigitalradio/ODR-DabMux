/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)
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

#ifndef DAB_INPUT_SLIP_H
#define DAB_INPUT_SLIP_H


#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include "dabInput.h"


#ifdef HAVE_FORMAT_BRIDGE
#   ifdef HAVE_INPUT_SLIP


extern struct dabInputOperations dabInputSlipOperations;

int dabInputSlipInit(void** args);
void* dabInputSlipThread(void* args);
int dabInputSlipOpen(void* args, const char* inputName);
int dabInputSlipSetbuf(void* args, int size);
int dabInputSlipRead(void* args, void* buffer, int size);
int dabInputSlipReadFrame(dabInputOperations* ops, void* args, void* buffer, int size);
int dabInputSlipClose(void* args);
int dabInputSlipClean(void** args);


#   endif
#endif


#endif // DAB_INPUT_SLIP_H
