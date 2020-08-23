Some knowledge accumulated about timestamping
=============================================

The meaning of the timestamps changed between v2.3.1 and v3.0.0, this document gives some guidance about the interaction between different settings.

The following table tries to summarise the differences.

+-----------------------------+----------------------------------------------+-------------------------------------+-----------------------------------------------+
| ODR-DabMux version          | Meaning of timestamp inside EDI              | ODR-ZMQ2EDI wait time w             | Offset that should be set in the mod          |
+=============================+==============================================+=====================================+===============================================+
| Up to and including v2.3.1  | t_frame = t_mux (No offset in mux available) | positive, meaning delay after t_mux | Something larger than w + mod processing time |
+=============================+==============================================+=====================================+===============================================+
| Later than v2.3.1           | t_frame = t_tx = t_mux + tist_offset         | negative, meaning delay before t_tx | Something larger than mod processing time     |
+-----------------------------+----------------------------------------------+-------------------------------------+-----------------------------------------------+

For historical reasons, ODR-DabMod decodes absolute timestamp from MNSC, not from “EDI seconds”.
The edilib tool decodes both EDI timestamp and MNSC, and can be used to verify both are identical.

Issues in ODR-DabMux v2.3.1
---------------------------

Running ODR-DabMux against the absolute timestamp firmware has uncovered a few issues:

 * At startup, the UTCO was not properly applied to the EDI seconds. This offset was 5 seconds (TAI-UTC offset - 32s, see EDI spec);
 * odr-zmq2edi did not compensate for UTCO, hiding the above issue;
 * ODR-DabMux needs a configurable offset;
 * (minor) MNSC and EDI timestamps did not use the same internal representation, making it difficult to prove that they encode the same value;
 * (minor) odr-zmq2edi swapped endianness when regenerating EDI from ETI (minor because only ODR-DabMod considers MNSC, and usually isn't used with EDI);

**Important** Do not combine odr-zmq2edi with odr-dabmux of a different version!

