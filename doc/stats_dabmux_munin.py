#!/usr/bin/env python
#
# present statistics from dabmux Stats Server and ZeroMQ RC
# to munin. Expects Stats server on port 12720 and ZeroMQ RC
# on port 12722.
#
# Copy this to /etc/munin/plugins/stats_dabmux_munin
# and make it executable (chmod +x)

import sys
import json
import zmq
import os
import re

config_top = """
multigraph clocktai_expiry
graph_title Time to expiry for TAI bulletin
graph_order expiry
graph_args --base 1000
graph_vlabel Number of seconds until expiry
graph_category dabmux
graph_info This graph shows the number of remaining seconds this bulletin is valid

expiry.info Seconds until expiry
expiry.label Seconds until expiry
expiry.min 0
expiry.warning {onemonth}:
""".format(onemonth=3600*24*30)

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
graph_order left left_slow right right_slow
graph_args --base 1000
graph_vlabel peak audio level during last ${{graph_period}}
graph_category encoders
graph_info This graph shows the audio level and peak of both channels of the {ident} ZMQ input

left.info Left channel audio level
left.label Left level
left.min -90
left.max 0
left.warning -40:0
left.critical -80:0
left_slow.info Left channel audio peak over last 5 minutes
left_slow.label Left peak
left_slow.min -90
left_slow.max 0
left_slow.warning -40:0
left_slow.critical -80:0
right.info Right channel audio level
right.label Right level
right.min -90
right.max 0
right.warning -40:0
right.critical -80:0
right_slow.info Right channel audio peak over last 5 minutes
right_slow.label Right peak
right_slow.min -90
right_slow.max 0
right_slow.warning -40:0
right_slow.critical -80:0

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

class RCException(Exception):
    pass

def do_transaction(command, sock):
    """To a send + receive transaction, quit whole program on timeout"""
    sock.send(command.encode("utf-8"))

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)

    socks = dict(poller.poll(1000))
    if socks:
        if socks.get(sock) == zmq.POLLIN:
            return sock.recv().decode("utf-8")

    sys.stderr.write("Could not receive data for command '{}'\n".format(command))
    sys.exit(1)

def do_multipart_transaction(message_parts, sock):
    """To a send + receive transaction, quit whole program on timeout"""
    if isinstance(message_parts, str):
        sys.stderr.write("do_transaction expects a list!\n");
        sys.exit(1)

    for i, part in enumerate(message_parts):
        if i == len(message_parts) - 1:
            f = 0
        else:
            f = zmq.SNDMORE
        sock.send(part, flags=f)

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)

    socks = dict(poller.poll(1000))
    if socks:
        if socks.get(sock) == zmq.POLLIN:
            rxpackets = sock.recv_multipart()
            return rxpackets

    raise RCException("Could not receive data for command '{}'\n".format(
        message_parts))

def get_rc_value(module, name, sock):
    try:
        parts = do_multipart_transaction([b"get", module.encode(), name.encode()],
                sock)
        if len(parts) != 1:
            sys.stderr.write("Received unexpected multipart message {}\n".format(
                parts))
            sys.exit(1)
        return parts[0].decode()
    except RCException as e:
        print("get {} {} fail: {}".format(module, name, e))
        return ""

def connect_to_stats():
    """Create a connection to the dabmux stats server

    returns: the socket"""

    sock = zmq.Socket(ctx, zmq.REQ)
    sock.set(zmq.LINGER, 5)
    sock.connect("tcp://localhost:12720")

    version = json.loads(do_transaction("info", sock))

    if not version['service'].startswith("ODR-DabMux"):
        sys.stderr.write("Wrong version\n")
        sys.exit(1)

    return sock

def connect_to_rc():
    """Create a connection to the dabmux RC

    returns: the socket"""

    sock = zmq.Socket(ctx, zmq.REQ)
    sock.set(zmq.LINGER, 5)
    sock.connect("tcp://localhost:12722")

    try:
        ping_answer = do_multipart_transaction([b"ping"], sock)

        if not ping_answer == [b"ok"]:
            sys.stderr.write("Wrong answer to ping\n")
            sys.exit(1)
    except RCException as e:
        print("connect failed because: {}".format(e))
        sys.exit(1)

    return sock

def handle_re(graph_name, re, rc_value, group_number=1):
    match = re.search(rc_value)
    if match:
        return "{}.value {}\n".format(graph_name, match.group(group_number))
    else:
        return "{}.value U\n".format(graph_name)

if len(sys.argv) == 1:
    munin_values = ""

    sock_rc = connect_to_rc()
    clocktai_expiry = get_rc_value("clocktai", "expiry", sock_rc)
    re_clocktai_expiry = re.compile(r"(\d+)", re.X)
    munin_values += "multigraph clocktai_expiry\n"
    munin_values += handle_re("expiry", re_clocktai_expiry, clocktai_expiry)

    sock_stats = connect_to_stats()
    values = json.loads(do_transaction("values", sock_stats))['values']

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

        if 'peak_left_slow' in v:
            # If ODR-DabMux is v2.0.0-3 or older, it doesn't export the slow peaks
            munin_values += "left_slow.value {}\n".format(v['peak_left_slow'])
            munin_values += "right_slow.value {}\n".format(v['peak_right_slow'])

        if 'state' in v:
            # If ODR-DabMux is v1.3.1-3 or older, it doesn't export state
            re_state = re.compile(r"\w+ \((\d+)\)")
            match = re_state.match(v['state'])
            if match:
                munin_values += "multigraph state_{ident}\n".format(ident=ident_)
                munin_values += "state.value {}\n".format(match.group(1))
            else:
                sys.stderr.write("Cannot parse state '{}'\n".format(v['state']))

    print(munin_values)

elif len(sys.argv) == 2 and sys.argv[1] == "config":
    sock_stats = connect_to_stats()

    config = json.loads(do_transaction("config", sock_stats))

    munin_config = config_top

    for conf in config['config']:
        munin_config += config_template.format(ident=conf.replace('-', '_'))

    print(munin_config)
else:
    sys.stderr.write("Invalid command line arguments")
    sys.exit(1)

