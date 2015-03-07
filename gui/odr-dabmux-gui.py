#!/usr/bin/python
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

from bottle import route, run, template, static_file
import socket
import json
import collections

class Service(object):
    def __init__(self, name, ptree):
        self.name = name
        self.label = ptree['label']
        if 'shortlabel' in ptree:
            self.shortlabel = ptree['shortlabel']
        else:
            self.shortlabel = ""
        self.srvid = ptree['id']


class Subchannel(object):
    def __init__(self, name, ptree):
        self.name = name
        for fieldname in ['type',
                "inputfile",
                "zmq-buffer",
                "zmq-prebuffering",
                "bitrate",
                "id",
                "protection",
                "encryption",
                "secret-key",
                "public-key",
                "encoder-key"]:
            if fieldname in ptree:
                setattr(self, fieldname.replace("-", "_"), ptree[fieldname])
            else:
                setattr(self, fieldname.replace("-", "_"), "")

class Component(object):
    def __init__(self, name, ptree):
        self.name = name
        for fieldname in ['label', 'shortlabel', 'service',
                'subchannel', 'figtype']:
            if fieldname in ptree:
                setattr(self, fieldname.replace("-", "_"), ptree[fieldname])
            else:
                setattr(self, fieldname.replace("-", "_"), "")

def get_mgmt_ptree():
    HOST = 'localhost'
    PORT = 12720
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.sendall(b'getptree\n')
    server_info = s.recv(32768)
    config_info = s.recv(32768)
    s.close()

    server_version = json.loads(server_info.decode())['service']
    config = json.loads(config_info.decode())

    return {'version': server_version, 'config': config}

@route('/config')
def config():
    return get_mgmt_ptree()

@route('/')
def index():
    ptree = get_mgmt_ptree()
    version = ptree['version']
    srv_pt = ptree['config']['services']
    services = [Service(name, srv_pt[name]) for name in srv_pt]

    sub_pt = ptree['config']['subchannels']
    subchannels = [Subchannel(name, sub_pt[name]) for name in sub_pt]

    comp_pt = ptree['config']['components']
    components = [Component(name, comp_pt[name]) for name in comp_pt]

    return template('index',
            version=version,
            services=services,
            components=components,
            subchannels=subchannels)


@route('/static/<filename:path>')
def send_static(filename):
    return static_file(filename, root='./static')

run(host='localhost', port=8000, debug=True, reloader=True)

