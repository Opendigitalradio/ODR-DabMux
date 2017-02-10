#!/usr/bin/env python
#
# This is an example program that illustrates
# how to interact with the zeromq remote control
#
# LICENSE: see bottom of file

import sys
import zmq

context = zmq.Context()

sock = context.socket(zmq.REQ)

if len(sys.argv) < 2:
    print("Usage: program url cmd [args...]")
    sys.exit(1)

sock.connect(sys.argv[1])

message_parts = sys.argv[2:]

# first do a ping test

print("Send ping")
sock.send("ping".encode())
data = sock.recv_multipart()

if len(data) != 1:
    print("Received invalid number of parts: {}".format(len(data)))
    for i,part in enumerate(data):
        print("   {}".format(part))
    sys.exit(1)

if data[0] != b'ok':
    print("Received invalid ping response: {}".format(data.decode()))
    sys.exit(1)

print("Ping ok, sending request '{}'...".format(" ".join(message_parts)))

for i, part in enumerate(message_parts):
    if i == len(message_parts) - 1:
        f = 0
    else:
        f = zmq.SNDMORE

    sock.send(part.encode(), flags=f)

data = sock.recv_multipart()

print("Received {} entries:".format(len(data)))
print("  " + "  ".join([d.decode() for d in data]))

# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org>


