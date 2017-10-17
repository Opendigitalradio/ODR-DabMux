#!/usr/bin/env python2
#
# present statistics from dabmux Stats Server
# to munin

import sys
import json
import zmq
import os
import re

config_top = """
"""

#default data type is GAUGE

config_template = """
multigraph buffers_{ident}
graph_title Contribution {ident} buffer
graph_order high low
graph_args --base 1000
graph_vlabel max/min buffer size bytes during last ${{graph_period}}
graph_category dabmux
graph_info This graph shows the high and low buffer sizes for the {ident} ZMQ input

high.info Max buffer size
high.label Max Buffer Bytes
high.min 0
high.warning 1:
low.info Min buffer size
low.label Min Buffer Bytes
low.min 0
low.warning 1:

multigraph over_underruns_{ident}
graph_title Contribution {ident} over/underruns
graph_order underruns overruns
graph_args --base 1000 --logarithmic
graph_vlabel number of underruns/overruns during last ${{graph_period}}
graph_category dabmux
graph_info This graph shows the number of under/overruns for the {ident} ZMQ input

underruns.info Number of underruns
underruns.label Number of underruns
underruns.min 0
underruns.warning 0:0
underruns.type COUNTER
overruns.info Number of overruns
overruns.label Number of overruns
overruns.min 0
overruns.warning 0:0
overruns.type COUNTER

multigraph audio_levels_{ident}
graph_title Contribution {ident} audio level (peak)
graph_order left right
graph_args --base 1000
graph_vlabel peak audio level during last ${{graph_period}}
graph_category encoders
graph_info This graph shows the audio level of both channels of the {ident} ZMQ input

left.info Left channel audio level
left.label Left channel audio level
left.min -90
left.max 0
left.warning -40:0
left.critical -80:0
right.info Right channel audio level
right.label Right channel audio level
right.min -90
right.max 0
right.warning -40:0
right.critical -80:0

multigraph state_{ident}
graph_title State of contribution {ident}
graph_order state
graph_args --base 1000 --lower-limit 0 --upper-limit 5
graph_vlabel Current state of the input
graph_category dabmux
graph_info This graph shows the state for the {ident} ZMQ input

state.info Input state
state.label 0 Unknown, 1 NoData, 2 Unstable, 3 Silent, 4 Streaming
state.warning 4:4
state.critical 2:4
"""

ctx = zmq.Context()

if not os.environ.get("MUNIN_CAP_MULTIGRAPH"):
    print("This needs munin version 1.4 at least")
    sys.exit(1)

def connect():
    """Create a connection to the dabmux stats server

    returns: the socket"""

    sock = zmq.Socket(ctx, zmq.REQ)
    sock.connect("tcp://localhost:12720")

    sock.send("info")
    version = json.loads(sock.recv())

    if not version['service'].startswith("ODR-DabMux"):
        sys.stderr.write("Wrong version\n")
        sys.exit(1)

    return sock

re_state = re.compile(r"\w+ \((\d+)\)")

if len(sys.argv) == 1:
    sock = connect()
    sock.send("values")
    values = json.loads(sock.recv())['values']

    munin_values = ""
    for ident in values:
        v = values[ident]['inputstat']

        ident_ = ident.replace('-', '_')
        munin_values += "multigraph buffers_{ident}\n".format(ident=ident_)
        munin_values += "high.value {}\n".format(v['max_fill'])
        munin_values += "low.value {}\n".format(v['min_fill'])
        munin_values += "multigraph over_underruns_{ident}\n".format(ident=ident_)
        munin_values += "underruns.value {}\n".format(v['num_underruns'])
        munin_values += "overruns.value {}\n".format(v['num_overruns'])
        munin_values += "multigraph audio_levels_{ident}\n".format(ident=ident_)
        munin_values += "left.value {}\n".format(v['peak_left'])
        munin_values += "right.value {}\n".format(v['peak_right'])

        if 'state' in v:
            # If ODR-DabMux is v1.3.1-3 or older, it doesn't export state
            match = re_state.match(v['state'])
            if match:
                munin_values += "multigraph state_{ident}\n".format(ident=ident_)
                munin_values += "state.value {}\n".format(match.group(1))
            else:
                print("Cannot parse state '{}'".format(v['state']))

    print(munin_values)

elif len(sys.argv) == 2 and sys.argv[1] == "config":
    sock = connect()

    sock.send("config")

    config = json.loads(sock.recv())

    munin_config = config_top

    for conf in config['config']:
        munin_config += config_template.format(ident=conf.replace('-', '_'))

    print(munin_config)
else:
    sys.exit(1)

