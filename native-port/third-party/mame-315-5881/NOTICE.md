# Sega 315-5881 stream decoding

`src/native_crypt5881.cpp` and `.hpp` adapt the decoding algorithm and tables from
MAME tag `mame0289`, under BSD-3-Clause:

- https://github.com/mamedev/mame/blob/mame0289/src/mame/sega/315-5881_crypt.cpp
- https://github.com/mamedev/mame/blob/mame0289/src/mame/sega/315-5881_crypt.h

Original copyright holders: Andreas Naive, Olivier Galibert, David Haywood
(implementation); David Haywood (header).

Codex adapted the standalone interface, removed MAME device/framework dependencies,
provided the bit helpers, and added explicit bounds/invalid-stream checks. The
S-box tables, key scheduling and stream/decompression algorithm retain their
upstream authorship. This is a data decoder, not a CPU execution engine. No ROM,
firmware, or game resources are licensed by this software notice.

The original reference copies remain in `references/mame0289-315-5881_crypt.*`.
Keep this notice, the source copyright headers and LICENSE.txt with redistribution.
