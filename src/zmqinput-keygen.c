/* Create a key file for the ZMQinput
 * and save to file.
 *
 * Copyright (c) 2014 Matthias P. Braendli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <zmq_utils.h>
#include <zmq.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char** argv)
{
    if (argc == 1) {
        fprintf(stderr, "Generate a random key for dabInputZMQ and save it to two files.\n\n");
        fprintf(stderr, "Usage: %s <name>\n", argv[0]);
        return 1;
    }

    const char* keyname = argv[1];

    if (strlen(keyname) > 2048) {
        fprintf(stderr, "name too long\n");
        return 1;
    }

    char pubkeyfile[strlen(keyname) + 10];
    char seckeyfile[strlen(keyname) + 10];

    sprintf(pubkeyfile, "%s.pub", keyname);
    sprintf(seckeyfile, "%s.sec", keyname);

    char public_key [41];
    char secret_key [41];
    int rc = zmq_curve_keypair(public_key, secret_key);
    if (rc != 0) {
        fprintf(stderr, "key generation failed\n");
        return 1;
    }

    int fdpub = creat(pubkeyfile, S_IRUSR | S_IWUSR);
    if (fdpub < 0) {
        perror("File creation failed");
        return 1;
    }

    int fdsec = creat(seckeyfile, S_IRUSR | S_IWUSR);
    if (fdsec < 0) {
        perror("File creation failed");
        return 1;
    }

    int r = write(fdpub, public_key, 41);

    int ret = 0;

    if (r < 0) {
        perror("write failed");
        ret = 1;
    }
    else if (r != 41) {
        fprintf(stderr, "Not enough key data written to file\n");
        ret = 1;
    }

    close(fdpub);

    if (ret == 0) {
        r = write(fdsec, secret_key, 41);

        if (r < 0) {
            perror("write failed");
            ret = 1;
        }
        else if (r != 41) {
            fprintf(stderr, "Not enough key data written to file\n");
            ret = 1;
        }
    }

    close(fdsec);

    return ret;
}

