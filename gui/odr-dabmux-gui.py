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

import os
import argparse
import json
import cherrypy
import muxconfig
import muxrc
import jinja2

class Root:
    def __init__(self, env, conf, rc):
        self.mconf = conf
        self.mrc = rc
        self.env = env
        self.mparam = ModuleParameter(env, rc)

    def _cp_dispatch(self, vpath):
        if len(vpath) == 3:
            vpath.pop(0) # /rc/
            cherrypy.request.params['module'] = vpath.pop(0)  # /module name/
            cherrypy.request.params['param'] = vpath.pop(0) # /parameter name/
            return self.mparam

        return vpath

    @cherrypy.expose
    def config(self, config=None):
        if config == None:
            """Show the JSON ptree in a textbox for editing"""
            self.mconf.load()
            tmpl = self.env.get_template('configeditor.tpl')
            return tmpl.render(
                version = self.mconf.get_mux_version(),
                config  = json.dumps(self.mconf.get_full_configuration(), indent=4),
                message = "")
        else:
            """Record the new configuration"""
            success = self.mconf.set_full_configuration(config)
            if success:
                successmessage = "Success"
            else:
                successmessage = "Failure"
            self.mconf.load()
            return template('configeditor',
                version = self.mconf.get_mux_version(),
                config  = json.dumps(self.mconf.get_full_configuration(), indent=4),
                message = successmessage)

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def config_json(self):
        """Return a application/json containing the full
        ptree of the mux"""

        self.mconf.load()
        return { 'version': self.mconf.get_mux_version(),
                 'config': self.mconf.get_full_configuration() }

    @cherrypy.expose
    def index(self):
        self.mconf.load()
        self.mrc.load()
        tmpl = self.env.get_template('index.tpl')
        return tmpl.render(
            version     = self.mconf.get_mux_version(),
            g           = self.mconf.get_general_options(),
            services    = self.mconf.get_services(),
            subchannels = self.mconf.get_subchannels(),
            components  = self.mconf.get_components(),
            rcmodules   = self.mrc.get_modules())

    @cherrypy.expose
    def services(self):
        self.mconf.load()
        tmpl = self.env.get_template('services.tpl')
        return tmpl.render(
            version     = self.mconf.get_mux_version(),
            services    = self.mconf.get_services())

    @cherrypy.expose
    def stats(self):
        self.mconf.load()
        tmpl = self.env.get_template('stats.tpl')
        return tmpl.render(
            version     = self.mconf.get_mux_version())


    @cherrypy.expose
    @cherrypy.tools.json_out()
    def stats_json(self):
        return self.mconf.get_stats_dict()

class ModuleParameter:
    def __init__(self, env, rc):
        self.mrc = rc
        self.env = env

    @cherrypy.expose
    def index(self, module, param, newvalue=None):
        if newvalue != None:
            rc.set_param_value(module, param, newvalue)
            raise cherrypy.HTTPRedirect('/#rcmodules')
        else:
            self.mrc.load()
            value = self.mrc.get_param_value(module, param)

            if param in paramObj:
                paramList = paramObj[param]
                label = paramObj["labels"][param]
            else:
                paramList = []
                label = ""

            tmpl = self.env.get_template('rcparam.tpl')
            return tmpl.render(
                module    = module,
                param     = param,
                value     = value,
                label     = label,
                list      = paramList)

if __name__ == '__main__':
    # Get configuration file in argument
    parser = argparse.ArgumentParser(description='management server for ODR-DabMux')
    parser.add_argument('--host', default='127.0.0.1', help='socket host (default: 127.0.0.1)',required=False)
    parser.add_argument('--port', default='8000', help='socket port (default: 8000)',required=False)
    parser.add_argument('--mhost', default='127.0.0.1', help='mux host (default: 127.0.0.1)',required=False)
    parser.add_argument('--mport', default='12720', help='mux management server port (default: 12720)',required=False)
    parser.add_argument('--rcport', default='12722', help='mux zmq rc port (default: 12722)',required=False)
    cli_args = parser.parse_args()

    # Instanciate mux-configuration and mux-remote-control objects
    conf = muxconfig.ConfigurationHandler(cli_args.mhost, int(cli_args.mport))
    rc = muxrc.MuxRemoteControl(cli_args.mhost, int(cli_args.rcport))

    # Import selectable paramaters values
    paramFile = open("rcparam.json")
    paramStr = paramFile.read()
    paramObj = json.loads(paramStr)

    # Start cherrypy
    env = jinja2.Environment(loader=jinja2.FileSystemLoader('views'), trim_blocks=True)
    cherrypy.config.update({'server.socket_host': cli_args.host, 'server.socket_port': int(cli_args.port),})
    appconf = {
        '/': {
            'tools.sessions.on': True,
            'tools.staticdir.root': os.path.abspath(os.getcwd())
        },
        '/static': {
            'tools.staticdir.on': True,
            'tools.staticdir.dir': './static'
        }
    }
    cherrypy.quickstart(Root(env, conf, rc), '/', appconf)
