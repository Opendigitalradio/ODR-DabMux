#!/usr/bin/env python3
#
# A munin plugin for ODR-ZMQ2EDI
#
# Reads the logfile, and the previously rotated logfile (suffixed by .1) and
# analyses the output. Generates a graph with percentage of frames late, and a
# graph with min/max wait time.
#
# Copy this to /etc/munin/plugins/stats_zmq2edi_munin
# and make it executable (chmod +x)
#
# Then make sure that zmq2edi log output gets written to LOGFILE below,
# and setup up a logrotate script to rotate the log. The rotated log
# filename must be appended with .1

# Every six seconds a line is output. We are polled in 5 min = 300s intervals
NUM_LINES = int(300 / 6)
LOGFILE = "/var/log/supervisor/zmq2edi.log"

import time
import sys
import os
import re

munin_config = """
multigraph wait_time_zmq2edi
graph_title zmq2edi wait_time
graph_order high low
graph_args --base 1000
graph_vlabel max/min wait times during last ${graph_period}
graph_category zmq2edi
graph_info This graph shows the min and max wait times

high.info Max wait time
high.label Max wait time ms
high.min 0
high.warning 1:
low.info Min wait time
low.label Min wait time ms
low.min -6000
low.warning 1:

multigraph late_packets_zmq2edi
graph_title EDI packets delivered too late
graph_order late
graph_args --base 1000
graph_vlabel late packets during last ${graph_period}
graph_category zmq2edi
graph_info This graph shows the number late EDI packets (250 packets = 6 seconds)

late.info Number of late packets
late.label Number of late packets
late.min 0
late.max %s
late.warning 0:0
""" % (NUM_LINES * 250,)

def parse_logs():
    # example lines:
    #  Buffering time statistics [milliseconds]: min: 907.799 max: 981.409 mean: 944.335 stdev: 26.827 late: 0 of 250 (0%)
    # Values might also be in scientific form, e.g. -1.80938e+07
    re_logline = re.compile(r"""Buffering time statistics.* min: (.+) max: (.+) mean: (.+) stdev: (.+) late: (.+) of 250""", flags=re.ASCII)

    # The following lines are output at startup and during a reset respectively:
    startup_pattern = "starting up"
    backoff_pattern = "Backoff"

    lines = []

    # Check that the file exists and was last written to in the previous 2* 6s,
    # otherwise assume the tool isn't running
    if not os.path.exists(LOGFILE) or (time.time() - os.stat(LOGFILE).st_mtime) > 12:
        num_late = None
        t_min_period = None
        t_max_period = None
    else:
        # Keep only the last NUM_LINES

        # Read the previously rotated logfile too to make sure we have enough data
        for fname in [LOGFILE+ ".1", LOGFILE]:
            if os.path.exists(fname):
                with open(fname, "r") as fd:
                    for line in fd:
                        lines.append(line)
                        if len(lines) > NUM_LINES:
                            del lines[0]

        # Calculate min, max over the whole period, and sum the number of late
        num_late = 0
        t_min_period = None
        t_max_period = None
        num_statistics = 0

        for line in lines:
            if startup_pattern in line:
                num_late += 250
            elif backoff_pattern in line:
                num_late += 250
            else:
                match = re_logline.search(line)
                if match:
                    num_statistics += 1
                    t_min = float(match.group(1))
                    t_max = float(match.group(2))
                    t_mean = float(match.group(3))
                    stdev = float(match.group(4))
                    late = int(match.group(5))

                    if t_min_period is None or t_min < t_min_period:
                        t_min_period = t_min

                    if t_max_period is None or t_max > t_max_period:
                        t_max_period = t_max

                    if num_late is None:
                        num_late = 0

                    num_late += late

    # The min can take extremely low values, we clamp it here to -6 seconds
    # to keep the graph readable
    if t_min_period is not None and t_min_period < -6000:
        t_min_period = -6000

    return num_late, round(t_min_period) if t_min_period is not None else None, round(t_max_period) if t_max_period is not None else None

def muninify(value):
    """ According to http://guide.munin-monitoring.org/en/latest/develop/plugins/plugin-concise.html#plugin-concise
    "If the plugin - for any reason - has no value to report, then it may send the value U for undefined."
    """
    return 'U' if value is None else value

# No arguments means that munin wants values
if len(sys.argv) == 1:
    num_late, t_min, t_max = parse_logs()

    munin_values = "multigraph wait_time_zmq2edi\n"
    munin_values += "high.value {}\n".format(muninify(t_max))
    munin_values += "low.value {}\n".format(muninify(t_min))

    munin_values += "multigraph late_packets_zmq2edi\n"
    munin_values += "late.value {}\n".format(muninify(num_late))
    print(munin_values)

elif len(sys.argv) == 2 and sys.argv[1] == "config":
    print(munin_config)
else:
    sys.exit(1)
