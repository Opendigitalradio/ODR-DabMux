#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   Copyright (C) 2016
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

class RCParameter(object):
    def __init__(self, param, value):
        self.param = param
        self.value = value

class RCModule(object):
    """Container object for RC module"""
    def __init__(self, name):
        self.name = name
        self.parameters = []

class MuxRemoteControl(object):
    """Interact with ODR-DabMux using the ZMQ RC"""

    def __init__(self, mux_host, mux_port=12722):
        self._host = mux_host
        self._port = mux_port
        self._ctx = zmq.Context()

        self.module_list = []

    def zRead(self, message_parts):
        sock = zmq.Socket(self._ctx, zmq.REQ)
        sock.setsockopt(zmq.LINGER, 0)
        sock.connect("tcp://{}:{}".format(self._host, self._port))

        for i, part in enumerate(message_parts):
            if i == len(message_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE

            print("Send {} {}".format(i, part))
            sock.send(part, flags=f)

        print("Poll")

        # use poll for timeouts:
        poller = zmq.Poller()
        poller.register(sock, zmq.POLLIN)
        if poller.poll(5*1000): # 5s timeout in milliseconds
            recv = sock.recv_multipart()
            print("RX {}".format(recv))
            sock.close()
            return recv
        else:
            raise IOError("Timeout processing ZMQ request")

    def load(self):
        """Load the list of RC modules"""
        module_names = self.zRead([b'list'])

        self.module_list = []

        for name in module_names:
            mod = RCModule(name)
            module_params = self.zRead([b'show', name])

            for param in module_params:
                p, v = param.split(': ')
                mod.parameters.append(RCParameter(p, v))

            self.module_list.append(mod)

    def get_modules(self):
        return self.module_list

