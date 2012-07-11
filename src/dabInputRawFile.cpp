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

#include "dabInputRawFile.h"
#include "dabInputFile.h"


#ifdef HAVE_FORMAT_RAW
#   ifdef HAVE_INPUT_FILE


struct dabInputOperations dabInputRawFileOperations = {
    dabInputFileInit,
    dabInputFileOpen,
    dabInputSetbuf,
    dabInputFileRead,
    NULL,
    NULL,
    dabInputRawFileRead,
    dabInputSetbitrate,
    dabInputFileClose,
    dabInputFileClean,
    dabInputFileRewind
};


int dabInputRawFileRead(dabInputOperations* ops, void* args, void* buffer,
        int size)
{
    int ret = ops->read(args, buffer, size);
    if (ret == 0) {
        etiLog.print(TcpLog::NOTICE, "reach end of raw file -> rewinding\n");
        ops->rewind(args);
        ret = ops->read(args, buffer, size);
    }
    return ret;
}


#   endif
#endif
