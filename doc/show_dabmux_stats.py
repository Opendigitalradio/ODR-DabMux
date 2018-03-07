#!/usr/bin/env python2
#
# present statistics from dabmux Stats Server
# to standard output.
#
# If you are looking for munin integration, use
# ODR-DabMux/doc/stats_dabmux_multi.py

import sys
import json
import zmq
import os

ctx = zmq.Context()

def connect():
    """Create a connection to the dabmux stats server

    returns: the socket"""

    sock = zmq.Socket(ctx, zmq.REQ)
    sock.connect("tcp://localhost:12720")

    sock.send("info")
    infojson = json.loads(sock.recv())

    sys.stderr.write("Statistics from ODR-DabMux {}\n".format(infojson['version']))

    if not infojson['service'].startswith("ODR-DabMux"):
        sys.stderr.write("This is not ODR-DabMux: {}\n".format(infojson['service']))
        sys.exit(1)

    return sock

if len(sys.argv) == 1:
    sock = connect()
    sock.send("values")

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)

    socks = dict(poller.poll(1000))
    if socks:
        if socks.get(sock) == zmq.POLLIN:

            data = sock.recv()
            values = json.loads(data)['values']

            tmpl = "{ident:20}{maxfill:>8}{minfill:>8}{under:>8}{over:>8}{audioleft:>8}{audioright:>8}{peakleft:>8}{peakright:>8}{state:>16}"
            print(tmpl.format(
                ident="id",
                maxfill="max",
                minfill="min",
                under="under",
                over="over",
                audioleft="audio L",
                audioright="audio R",
                peakleft="peak L",
                peakright="peak R",
                state="state"))

            for ident in values:
                v = values[ident]['inputstat']

                if 'state' not in v:
                    v['state'] = None

                print(tmpl.format(
                    ident=ident,
                    maxfill=v['max_fill'],
                    minfill=v['min_fill'],
                    under=v['num_underruns'],
                    over=v['num_overruns'],
                    audioleft=v['peak_left'],
                    audioright=v['peak_right'],
                    peakleft=v['peak_left_slow'],
                    peakright=v['peak_right_slow'],
                    state=v['state']))


elif len(sys.argv) == 2 and sys.argv[1] == "config":
    sock = connect()

    sock.send("config")

    config = json.loads(sock.recv())

    print(config['config'])

