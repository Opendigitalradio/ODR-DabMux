#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include "dabOutput.h"


int DabOutputFifo::Write(void* buffer, int size)
{
    uint8_t padding[6144];

    switch (this->type_) {
        case ETI_FILE_TYPE_FRAMED:
            if (this->nbFrames_ == 0) {
                uint32_t nbFrames = (uint32_t)-1;
                // Writing nb frames
                if (write(this->file_, &nbFrames, 4) == -1)
                    goto FIFO_WRITE_ERROR;
            }
        case ETI_FILE_TYPE_STREAMED:
            // Writting frame length
            if (write(this->file_, &size, 2) == -1)
                goto FIFO_WRITE_ERROR;
            // Appending data
            if (write(this->file_, buffer, size) == -1)
                goto FIFO_WRITE_ERROR;
            break;
        case ETI_FILE_TYPE_RAW:
            // Appending data
            if (write(this->file_, buffer, size) == -1)
                goto FIFO_WRITE_ERROR;
            // Appending padding
            memset(padding, 0x55, 6144 - size);
            if (write(this->file_, padding, 6144 - size) == -1)
                goto FIFO_WRITE_ERROR;
            break;
        default:
            etiLog.printHeader(TcpLog::ERR, "File type is not supported.\n");
            return -1;
    }

    return size;

FIFO_WRITE_ERROR:
    perror("Error while writting to file");
    return -1;
}

