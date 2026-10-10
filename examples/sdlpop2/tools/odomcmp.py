#!/usr/bin/env python3
"""odomcmp.py SNAP TICK LEVEL.bin CHARS.bin KID.bin [ROOM]: DOS's memory after a tick (the oracle's ds_tick sample,
DS:2900..) against the core runner's domains dumped after the same row (run-native --dump-domain): the level
(DS:2BB8, 12024 bytes), the five characters (DS:5B76) and the prince (DS:5B36), the bytes that differ as DS offsets.
The DOS tick is the one of TICK-2..TICK+2 whose prince record is nearest the core's (a picture's tick and the
state's are one apart in places). With ROOM: that room's tiles (type:modifier) as DOS has them."""
import re, sys
snap, tick = sys.argv[1], int(sys.argv[2]); B = 0x2900; k = -1; ds = {}
for l in open(snap, errors="replace"):
    if " ds_tick at" not in l: continue
    k += 1
    if tick - 2 <= k <= tick + 2: ds[k] = bytes.fromhex(re.search(r" mem=([0-9A-Fa-f]+)", l).group(1))
    if k > tick + 2: break
kid = open(sys.argv[5], "rb").read()
best = min(ds, key=lambda t: sum(a != b for a, b in zip(kid[:0x1B], ds[t][0x5B36 - B: 0x5B36 - B + 0x1B])))
d = ds[best]; print("DOS tick", best)
for name, path, a in (("Level", sys.argv[3], 0x2BB8), ("Characters", sys.argv[4], 0x5B76), ("Kid", sys.argv[5], 0x5B36)):
    m = open(path, "rb").read(); o = d[a - B: a - B + len(m)]
    diff = [i for i in range(len(m)) if m[i] != o[i]]
    print("%s: %d differ: %s" % (name, len(diff), " ".join("DS:%04X dos %02X core %02X" % (a + i, o[i], m[i]) for i in diff[:30])))
if len(sys.argv) > 6:
    room = int(sys.argv[6]); lv = 0x2BB8 - B
    tl = d[lv + (room - 1) * 30: lv + room * 30]; at = d[lv + 0x348 + room * 120: lv + 0x348 + room * 120 + 120]
    import struct
    for r in range(3): print("  row %d: " % r + " ".join("%02X:%04X" % (tl[r * 10 + c], struct.unpack_from("<I", at, (r * 10 + c) * 4)[0] & 0xFFFF) for c in range(10)))
