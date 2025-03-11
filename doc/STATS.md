Stats available through Management Server
=========================================

Interface
---------

The management server makes statistics about the inputs and EDI/TCP outputs
available through a ZMQ request/reply socket.

The `show_dabmux_stats.py` illustrates how to access this information.

Meaning of values for inputs
----------------------------

`max` and `min` indicate input buffer fullness in bytes.

`under` and `over` count the number of buffer underruns and overruns.

`audio L` and `audio R` show the maximum audio level in dBFS over the last 500ms.

`peak L` and `audio R` show the max audio level in dBFS over the last 5 minutes.

The audio levels are measured in the audio encoder and carried in the EDI
`ODRa` TAG, or in the ZMQ metadata. Otherwise ODR-DabMux would have to decode
all audio contributions to measure the audio level.

`State` is either NoData, Unstable, Silence, Streaming.
Unstable means that underruns or overruns have occurred in the previous 30 minutes.
Silence means the stream is working, but audio levels are always below -50dBFS.

`version` and `uptime` are fields directly coming from the contribution source,
and are only supported for the EDI input. These are carried over EDI using custom
TAG `ODRv` (see function `parse_odr_version_data` in `lib/edi/common.cpp`).

