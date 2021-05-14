This TODO file lists ideas and features for future developments. They are
more or less ordered according to their benefit, but that is subjective
to some degree.

Unless written, no activity has been started on the topics.


Explicit Service Linking
------------------------
It is impossible to activate/deactive linkage sets. Commit 5c3c6d7 added
some code to transmit a FIG0/6 CEI, but this was subsequently reverted
because it was not tested enough.


Inputs for packet data
----------------------
It is currently unclear what input formats and sources work for packet data,
and which ones would make sense to add.

Also, there is no documentation on the possibilites of packet data.


Improvements for inputs
-----------------------
Add statistics to UDP input, in a similar way that ZeroMQ offers statistics.
This would mean we have to move the packet buffer from the operating system
into our own buffer, so that we can actually get the statistics.


Fix DMB input
-------------
The code that does interleaving and reed-solomon encoding for DMB is not used
anymore, and is untested. The relevant parts are `src/dabInputDmb*` and
`src/Dmb.cpp`


Communicate Leap Seconds
------------------------
Actually, we're supposed to say in FIG0/10 when there is a UTC leap second
upcoming, but since that's not trivial to find out because the POSIX time
concept is totally unaware of that, this is not done. We need to know for EDI
TIST, and the ClockTAI class can get the information from the Internet, but it
is not used in FIG0/10.


Implement FIG0/20 Service List
------------------------------
See ETSI TS 103 176
