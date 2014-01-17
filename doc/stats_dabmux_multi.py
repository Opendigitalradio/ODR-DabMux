#!/usr/bin/env python2
#
# present statistics from dabmux Stats Server
# to munin

import sys
import json
import socket

config_template_top = """
multigraph zmq_inbuf
graph_title {title}
graph_order high low underruns overruns
graph_args --base 1000
graph_vlabel max/min buffer size bytes during last ${{graph_period}}
graph_category dabmux
graph_info This graph shows the high and low buffer sizes for ZMQ inputs

high.info Max buffer size
high.label Bytes
high.min 0
low.info Min buffer size
low.label Bytes
low.min 0
underruns.info Number of underruns
underruns.label Occurrencies
underruns.min 0
overruns.info Number of overruns
overruns.label Occurrencies
overruns.min 0

"""

config_template_individual = """
multigraph zmq_inbuf.id_{ident}

graph_title Contribution {ident} buffer
graph_order high low
graph_args --base 1000
graph_vlabel max/min buffer size bytes during last ${{graph_period}}
graph_category dabmux
graph_info This graph shows the high and low buffer sizes for the {ident} ZMQ input

high.info Max buffer size
high.label Bytes
high.min 0
high.warning 1:
low.info Min buffer size
low.label Bytes
low.min 0
low.warning 1:
underruns.info Number of underruns
underruns.label Occurrencies
underruns.min 0
underruns.warning 1:
overruns.info Number of overruns
overruns.label Occurrencies
overruns.min 0
overruns.warning 1:
"""

#TODO enable
#if not os.environ.get("MUNIN_CAP_MULTIGRAPH"):
#    print("This needs munin version 1.4 at least")
#    sys.exit(1)

def connect():
    """Create a connection to the dabmux stats server

    returns: the socket"""

    sock = socket.socket()
    sock.connect(("localhost", 12720))

    version = json.loads(sock.recv(256))

    if not version['service'].startswith("CRC-DabMux"):
        sys.stderr.write("Wrong version\n")
        sys.exit(1)

    return sock

def get_id_from_uri(uri):
    proto, _, port = uri.partition('://*:')
    return "zmq_" + proto + "_" + port

if len(sys.argv) == 1:
    sock = connect()
    sock.send("values\n")
    values = json.loads(sock.recv(256))['values']

    munin_values = ""
    for ident in values:
        v = values[ident]['inputstat']
        munin_values += "multigraph zmq_inbuf.id_{ident}\n".format(ident=get_id_from_uri(ident))
        munin_values += "high.value {}\n".format(v['max_fill'])
        munin_values += "low.value {}\n".format(v['min_fill'])
        munin_values += "underruns.value {}\n".format(v['num_underruns'])
        munin_values += "overruns.value {}\n".format(v['num_overruns'])

    print(munin_values)

elif len(sys.argv) == 2 and sys.argv[1] == "config":
    sock = connect()

    sock.send("config\n")

    config = json.loads(sock.recv(256))

    munin_config = config_template_top.format(title="dabmux ZMQ input buffers")

    for conf in config['config']:
        munin_config += config_template_individual.format(ident=get_id_from_uri(conf))

    print(munin_config)

