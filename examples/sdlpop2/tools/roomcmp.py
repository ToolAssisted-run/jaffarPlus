#!/usr/bin/env python3
"""roomcmp.py REF TRACE: a solution's rooms against a reference's, room by room.

REF is watch_level.sh's reference (mkref.sh: "# DIR CHAIN LATE", then "ROOM TICK": an earlier solution's rooms and
the ticks it enters them on); TRACE the new solution tick by tick (trs GAMEDIR SEED 0 0 CHAIN LEVEL.sol). Prints both
ways side by side, the ticks spent in each room and the difference; a room the new one spends 5 ticks or more longer
in is marked. To be run on every search's solution before it is kept: a wide search can drop the fast way through a
room without any alarm (level 8's first: 88 ticks in rooms 2 and 12, 19 gained after them, 885 for 817).
"""
import sys

ref = [tuple(map(int, l.split())) for l in open(sys.argv[1]) if l.strip() and not l.startswith("#")]
new = []; last = None; end = None
for l in open(sys.argv[2], errors="replace"):
    f = l.split()
    if len(f) >= 8 and f[0].isdigit() and f[7].startswith("L"):
        if new and f[7] != lv: end = int(f[0]); break
        lv = f[7]
        if int(f[1]) != last: last = int(f[1]); new.append((last, int(f[0])))
j = 0; late = 0
print("room   ref  new  (ticks in it: ref new)")
for k, (room, t) in enumerate(new):
    m = next((i for i in range(j, min(len(ref), j + 4)) if ref[i][0] == room), None)
    dn = (new[k + 1][1] if k + 1 < len(new) else end or t) - t
    if m is None: print("%4d     - %4d  (- %d)" % (room, t, dn)); continue
    dr = (ref[m + 1][1] - ref[m][1]) if m + 1 < len(ref) else None
    mark = "" if dr is None or dn - dr < 5 else "   <-- %d longer" % (dn - dr)
    late += bool(mark)
    print("%4d  %4d %4d  (%s %d)%s" % (room, ref[m][1], t, dr if dr is not None else "-", dn, mark)); j = m + 1
print("%d rooms 5 ticks or more longer than the reference's%s" % (late, "; the level's end at tick %d" % end if end else ""))
