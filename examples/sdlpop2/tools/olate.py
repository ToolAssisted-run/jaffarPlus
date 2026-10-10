#!/usr/bin/env python3
"""olate.py SNAP TICK0 TICK1 ROOM TILEPOS: from the oracle's snapshot (sol2script.py's ds_tick probe: DS:2900.. after
every tick), after each of the ticks: DS:2BA4 (the lateness meter: a pass that took a timer tick more) and the
tile's attribute word; then how many ticks of each 500 of the run start late. (ROUTING.md: level 14's room 3.)"""
import re, struct, sys
snap, t0, t1, room, tp = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
B = 0x2900; lv = 0x2BB8 - B; k = -1; out = []; hist = {}
for l in open(snap, errors="replace"):
    if " ds_tick at" not in l: continue
    k += 1
    m = re.search(r" mem=([0-9A-Fa-f]+)", l)
    if len(m.group(1)) < 2 * 0x400: continue
    d = bytes.fromhex(m.group(1)[:2 * 0x1200])
    late = struct.unpack_from("<H", d, 0x2BA4 - B)[0]
    lvl = d[lv + 0x1847] if lv + 0x1847 < len(d) else -1
    hist.setdefault(k // 500, [0, 0]); hist[k // 500][0] += 1; hist[k // 500][1] += late != 0
    if t0 <= k <= t1:
        a = struct.unpack_from("<I", d, lv + 0x348 + room * 120 + tp * 4)[0]
        out.append("%d:late%d:%04X" % (k, late, a & 0xFFFF))
print(" ".join(out))
print("ticks late (of 500) by 500 ticks: " + " ".join("%d:%d" % (b * 500, v[1]) for b, v in sorted(hist.items())))
