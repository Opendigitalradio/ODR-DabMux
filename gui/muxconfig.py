#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   Copyright (C) 2015
#   Matthias P. Braendli, matthias.braendli@mpb.li
#
#    http://www.opendigitalradio.org
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
import zmq
import json

class General(object):
    """Container object for general options"""
    def __init__(self, pt):
        ptree = pt['general']
        for fieldname in ["nbframes",
                "statsserverport",
                "writescca",
                "tist",
                "dabmode",
                "syslog"]:
            if fieldname in ptree:
                setattr(self, fieldname.replace("-", "_"), ptree[fieldname])
            else:
                setattr(self, fieldname.replace("-", "_"), "")
        self.telnetport = pt['remotecontrol']['telnetport']

class Service(object):
    """Container object for a service"""
    def __init__(self, name, ptree):
        self.name = name

        for fieldname in ['id',
                "label",
                "shortlabel",
                "pty",
                "language" ]:
            if fieldname in ptree:
                setattr(self, fieldname.replace("-", "_"), ptree[fieldname])
            else:
                setattr(self, fieldname.replace("-", "_"), "")

class Subchannel(object):
    """Container object for a subchannel"""
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
    """Container object for a component"""
    def __init__(self, name, ptree):
        self.name = name
        for fieldname in ['label', 'shortlabel', 'service',
                'subchannel', 'figtype']:
            if fieldname in ptree:
                setattr(self, fieldname.replace("-", "_"), ptree[fieldname])
            else:
                setattr(self, fieldname.replace("-", "_"), "")

class ConfigurationHandler(object):
    """Load and present the configration from ODR-DabMux"""

    def __init__(self, mux_host, mux_port=12720):
        self._host = mux_host
        self._port = mux_port

        # local copy of the configuration
        self._server_version = None
        self._config = None
        self._statistics = None

        self._ctx = zmq.Context()
        self.sock = zmq.Socket(self._ctx, zmq.REQ)
        self.sock.connect("tcp://{}:{}".format(self._host, self._port))

    def load(self):
        """Load the configuration from the multiplexer and
        save it locally"""
        self.sock.send(b'info')
        server_info = self.sock.recv()

        self.sock.send(b'getptree')
        config_info = self.sock.recv()

        print("Config '%r'" % config_info)

        self._server_version = json.loads(server_info)['service']
        self._config = json.loads(config_info)

    def update_stats(self):
        """Load the statistics from the multiplexer and
        save them locally"""

        self.sock.send(b'info')
        server_info = self.sock.recv()

        self.sock.send(b'values')
        stats_info = self.sock.recv()

        self._statistics = json.loads(stats_info)['values']

    def get_full_configuration(self):
        return json.dumps(self._config, indent=4)

    def set_full_configuration(self, config_json):
        self.sock.send(b'setptree', flags=zmq.SNDMORE)
        self.sock.send(config_json)
        return self.sock.recv() == "OK"

    def get_mux_version(self):
        return self._server_version

    def get_services(self):
        srv_pt = self._config['services']
        return [Service(name, srv_pt[name]) for name in srv_pt]

    def get_subchannels(self):
        sub_pt = self._config['subchannels']
        return [Subchannel(name, sub_pt[name]) for name in sub_pt]

    def get_components(self):
        comp_pt = self._config['components']
        return [Component(name, comp_pt[name]) for name in comp_pt]

    def get_general_options(self):
        return General(self._config)

    def get_stats_dict(self):
        """Return a dictionary with all stats"""
        self.update_stats()
        return self._statistics
