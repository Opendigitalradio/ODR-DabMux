Required dependencies:
======================

* Boost 1.48 or later
* ZeroMQ 4 or later
* (optional) cURL to download the TAI-UTC bulletin, needed for the EDI output.

Simple install procedure using tarball release:
===============================================

Install odr-dabmux
------------------

    % tar xjf odr-dabmux-x.y.z.tar.gz       # Unpack the source
    % cd odr-dabmux-x.y.z                   # Change to the source directory
    % ./configure
                                            # Run the configure script
    % make                                  # Build ODR-DabMux
    [ as root ]
    % make install                          # Install ODR-DabMux

Nearly as simple install procedure using repository:
----------------------------------------------------

The *master* branch in the repository always points to the
latest release. If you are looking for a new feature or bug-fix
that did not yet make its way into a release, you can clone the
*next* branch from the repository.

* Download and install the dependencies as above
* Clone the git repository
* Switch to the *next* branch
* Bootstrap autotools: <pre>% ./bootstrap.sh</pre>
* Then use ./configure as above

It is advised to run the bootstrap and configure steps again every
time you pull updates from the repository.

Develop on OSX and FreeBSD
==========================

If you want to develop on OSX platform install the necessary build tools
with brew

    brew install automake boost

On FreeBSD, pkg installs all dependencies to /usr/local, but the build
tools will not search there by default. Set the following environment variables
before calling ./configure

    LDFLAGS="-L/usr/local/lib"
    CFLAGS="-I/usr/local/include"
    CXXFLAGS="-I/usr/local/include"

On both systems, raw output is not available.

Advanced install procedure:
===========================

The configure script can be launch with a variety of options, launch the
following command for a complete list:

    % ./configure --help

