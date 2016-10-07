The ODR-DabMux Web Management GUI
=================================

The whole world has repeatedly been asking for a graphical administration
console for the ODR-mmbTools. I give in now, and start working on this
web-based GUI.

In the current state, it can display part of the configuration of a running
ODR-DabMux in your browser. It doesn't seem like much, but you *will* be
impressed.

Usage
-----

Launch ODR-DabMux with your preferred multiplex, and enable the statistics and
management server in the configuration file to port 12720, and the zeromq RC on
tcp://lo:12722

Start the gui/odr-dabmux-gui.py script on the same machine

Connect to http://localhost:8000

Admire the fabulously well-designed presentation of the configuration. In the
remote control tab, you can interact with the ODR-DabMux RC to get an set
parameters.

Expect more features to come: Better design; integrated statistics, dynamically
updated information, configuration upload and download, less ridiculous README,
and much more. We can even start dreaming about live multiplex reconfiguration.

2016-10-07 mpb

