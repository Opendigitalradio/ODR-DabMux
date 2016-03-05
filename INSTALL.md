Required dependencies:
======================

* libfec from Phil Karn, with compatibility patch:
[ka9q-fec](https://github.com/Opendigitalradio/ka9q-fec)
* Boost 1.48 or later
* Optional ZeroMQ 4 from [http://www.zeromq.org](http://www.zeromq.org).
  Please prefer the zeromq from your distribution, but mind that some distributions
  ship ZeroMQ 2, which is not enough.

Use the --disable-output-zeromq ./configure option if you don't have ZeroMQ.

Simple install procedure using tarball release:
===============================================

Install libfec
--------------

    % git clone https://github.com/Opendigitalradio/ka9q-fec.git
    % cd ka9q-fec
    % ./bootstrap
    % ./configure                           # Run the configure script
    % make                                  # Build the library
    [as root]
    % make install                          # Install the library

Install odr-dabmux
------------------

    % tar xjf odr-dabmux-x.y.z.tar.bz2      # Unpack the source
    % cd odr-dabmux-x.y.z                   # Change to the source directory
    % ./configure
                                            # Run the configure script
    % make                                  # Build ODR-DabMux
    [ as root ]
    % make install                          # Install ODR-DabMux

Nearly as simple install procedure using repository:
====================================================

The *master* branch in the repository always points to the
latest release. If you are looking for a new feature or bug-fix
that did not yet make its way into a release, you can clone the
*next* branch from the repository.

* Download and install the dependencies as above
* Clone the git repository
* Switch to the *next* branch
* Bootstrap autotools: <pre>% ./bootstrap.sh</pre>
* Then use ./configure as above


Advanced install procedure:
===========================

The configure script can be launch with a variety of options, launch the
following command for a complete list:

    % ./configure --help

Notes about libfec
==================
The original libfec version from
[ka9q.net](http://www.ka9q.net/code/fec/fec-3.0.1.tar.bz2)
does not compile on x86\_64 nor on ARM. That is the reason why the patched
version is suggested.

On x86 systems, the original version can also be used.
