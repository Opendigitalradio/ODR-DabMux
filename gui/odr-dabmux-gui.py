#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   Copyright (C) 2016
#   Matthias P. Braendli, matthias.braendli@mpb.li
#
#    http://www.opendigitalradio.org
#
#   This is a management server for ODR-DabMux, and it will become much
#   more interesting in the future.
#
#   Run this script and connect your browser to
#   http://localhost:8000 to show the currently running
#
#   This file is part of ODR-DabMux.
#
#   ODR-DabMux is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of the
#   License, or (at your option) any later version.
#
#   ODR-DabMux is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.

from muxconfig import *
from muxrc import *
from bottle import route, run, template, static_file, request
import json

import argparse

@route('/config')
def config():
    """Show the JSON ptree in a textbox for editing"""

    conf.load()

    return template('configeditor',
            version = conf.get_mux_version(),
            config  = conf.get_full_configuration(),
            message = "")

@route('/config', method="POST")
def config_json_post():
    """Update the ODR-DabMux configuration with the JSON ptree
    given as POST data"""

    new_config = request.forms.get('config')
    print("New config %s" % new_config)

    success = conf.set_full_configuration(new_config)

    if success:
        successmessage = "Success"
    else:
        successmessage = "Failure"

    conf.load()

    return template('configeditor',
            version = conf.get_mux_version(),
            config  = json.dumps(conf.get_full_configuration(), indent=4),
            message = successmessage)

@route('/config.json', method="GET")
def config_json_get():
    """Return a application/json containing the full
    ptree of the mux"""

    conf.load()

    return { 'version': conf.get_mux_version(),
            'config': conf.get_full_configuration() }


@route('/')
def index():
    conf.load()
    rc.load()

    return template('index',
            version     = conf.get_mux_version(),
            g           = conf.get_general_options(),
            services    = conf.get_services(),
            subchannels = conf.get_subchannels(),
            components  = conf.get_components(),
            rcmodules   = rc.get_modules())

@route('/services')
def index():
    conf.load()

    return template('services',
            version     = conf.get_mux_version(),
            services    = conf.get_services())

@route('/stats')
def index():
    conf.load()

    return template('stats',
            version     = conf.get_mux_version())

@route('/stats.json')
def stats_json():
    return conf.get_stats_dict()


@route('/static/<filename:path>')
def send_static(filename):
    return static_file(filename, root='./static')


if __name__ == '__main__':
    # Get configuration file in argument
    parser = argparse.ArgumentParser(description='management server for ODR-DabMux')
    parser.add_argument('--host', default='127.0.0.1', help='socket host (default: 127.0.0.1)',required=False)
    parser.add_argument('--port', default='8000', help='socket port (default: 8000)',required=False)
    parser.add_argument('--mhost', default='127.0.0.1', help='mux host (default: 127.0.0.1)',required=False)
    parser.add_argument('--mport', default='12720', help='mux management server port (default: 12720)',required=False)
    parser.add_argument('--rcport', default='12722', help='mux zmq rc port (default: 12722)',required=False)
    cli_args = parser.parse_args()

    conf = ConfigurationHandler(cli_args.mhost, cli_args.mport)

    rc = MuxRemoteControl(cli_args.mhost, cli_args.rcport)

    run(host=cli_args.host, port=int(cli_args.port), debug=True, reloader=False)

