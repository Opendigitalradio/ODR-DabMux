#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   Copyright (C) 2015
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
from bottle import route, run, template, static_file
import json

conf = ConfigurationHandler('localhost')

@route('/config')
def config():
    """Return a application/json containing the full
    ptree of the mux"""

    conf.load()

    return {'version': conf.get_mux_version(),
            'config':  conf.get_full_configuration()}

@route('/')
def index():
    conf.load()

    return template('index',
            version     = conf.get_mux_version(),
            g           = conf.get_general_options(),
            services    = conf.get_services(),
            subchannels = conf.get_subchannels(),
            components  = conf.get_components())

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

run(host='localhost', port=8000, debug=True, reloader=True)

