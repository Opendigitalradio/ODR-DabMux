Required dependencies:
======================

* A C++11 compiler
* Boost 1.48 or later
* ZeroMQ 4 or later
* (optional) cURL to download the TAI-UTC bulletin, needed for the EDI output.

Dependencies on Debian
----------------------

On Debian you will need to install the following packages:
build-essential, libzmq3-dev, libzmq3, automake, libtool, libboost-all-dev, libcurl4-openssl-dev

Dependencies on CentOS
----------------------

In addition to the packages needed to install a compiler, install the packages:
boost-devel libcurl-devel zeromq-devel

Third-party RPM packages are maintained by RaBe, and are built by the
[openSUSE Build Service](https://build.opensuse.org/project/show/home:radiorabe:dab).
For questions regarding these packages, please get in touch with the maintainer of
the [radio RaBe repository](https://github.com/radiorabe/).

For openSUSE, mnhauke is maintaining packages, also built using
[OBS](https://build.opensuse.org/project/show/home:mnhauke:ODR-mmbTools).

Compile odr-dabmux
==================

The *master* branch in the repository always points to the
latest release. If you are looking for a new feature or bug-fix
that did not yet make its way into a release, you can clone the
*next* branch from the repository.

* Download and install the dependencies as above
* Clone the git repository
* Switch to the *next* branch
* Bootstrap autotools: <pre>% ./bootstrap.sh</pre>
* Run the configure script <pre>./configure</pre>
* Build ODR-DabMux <pre>make</pre>
* Install ODR-DabMux (as root) <pre>sudo make install</pre>

It is advised to run the bootstrap and configure steps again every
time you pull updates from the repository.

Develop on OSX and FreeBSD
==========================

If you want to develop on OSX platform install the necessary build tools
and dependencies with brew

    brew install boost zeromq automake curl

On FreeBSD, pkg installs all dependencies to /usr/local, but the build
tools will not search there by default. Set the following environment variables
before calling ./configure

    LDFLAGS="-L/usr/local/lib"
    CFLAGS="-I/usr/local/include"
    CXXFLAGS="-I/usr/local/include"

On both systems, RAW output is not available. Note that these systems
are not tested regularly.

Advanced install procedure:
===========================

The configure script can be launched with a variety of options, launch the
following command for a complete list:

    % ./configure --help

