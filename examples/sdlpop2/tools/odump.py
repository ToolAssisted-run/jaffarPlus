#!/usr/bin/env python3
"""odump.py SNAP TICK0 TICK1 ROOM: from the oracle's snapshot, after each of the ticks: the prince's record, Char's
opponent, the five characters, and ROOM's tiles (type:modifier). mem= starts at DS:2900."""
import re, struct, sys
snap, t0, t1, room = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
B = 0x2900
def ch(d, ds):
    o = ds - B; r = d[o:o + 64]
    x, y = struct.unpack_from("<hh", r, 2); fr = struct.unpack_from("<H", r, 7)[0]
    return "i%d d%d x%d y%d c%d f%d col%d row%d a%d r%d f0f=%d f10=%d al%d hp%d seq%d/%d f19=%d f23=%d f24=%d opp%d" % (
        r[0], struct.unpack_from("<b", r, 1)[0], x, y, r[6], fr, struct.unpack_from("<b", r, 9)[0], struct.unpack_from("<b", r, 10)[0], r[11], r[14], r[15], r[16],
        struct.unpack_from("<b", r, 17)[0], r[18], struct.unpack_from("<H", r, 0x17)[0], struct.unpack_from("<H", r, 0x15)[0], struct.unpack_from("<H", r, 0x19)[0], r[0x23],
        struct.unpack_from("<H", r, 0x24)[0], r[0x39])
k = -1
for l in open(snap, errors="replace"):
    if " ds_tick at" not in l: continue
    k += 1
    if k < t0: continue
    if k > t1: break
    d = bytes.fromhex(re.search(r" mem=([0-9A-Fa-f]+)", l).group(1))
    print("== after tick %d (mem %d bytes)  seed %d" % (k, len(d), struct.unpack_from("<I", d, 0x3DDCA - 0x3DB50)[0]))
    print("  Kid : " + ch(d, 0x5B36)); print("  Opp : " + ch(d, 0x5AF6))
    for i in range(5): print("  ch%d : %s" % (i, ch(d, 0x5B76 + 64 * i)))
    lv = 0x2BB8 - B
    tl = d[lv + (room - 1) * 30: lv + room * 30]; at = d[lv + 0x348 + room * 120: lv + 0x348 + room * 120 + 120]
    for r in range(3):
        print("  row %d: " % r + " ".join("%02X:%08X" % (tl[r * 10 + c], struct.unpack_from("<I", at, (r * 10 + c) * 4)[0]) for c in range(10)))
    rr = 0x43AC - B + room * 0x74
    print("  room record: n=%d " % d[rr - 1] + d[rr:rr + 46].hex())
