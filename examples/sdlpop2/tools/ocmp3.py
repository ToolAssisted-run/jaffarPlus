#!/usr/bin/env python3
"""ocmp3.py SNAP CORE.txt [FROM]: the run checked in DOS in segments. The oracle's script (made with SYNC=CORE.txt)
writes the core's random seed at every tick's start; where DOS's own seed is the core's already the write changes
nothing, so DOS plays on by itself until a tick whose draws differ, and a new segment starts on the tick after it.
Against the core's trace (tools/trs.c): the prince's room, x, y, frame and hit points on every tick, the segments, and
the ticks that end them (DOS's seed after the tick, read before the next write, is not the core's): how many draws
DOS made more (+) or fewer (-) in that tick.

DOS starts the ambient music's next piece (one draw) at the end of the pass in which the piece is found over, and that
moment moves with the drawing's time; the core's model has it 5 ms into the pass. So DOS is often a tick early with
that draw (+1 in a tick, then -1 in the next: the core's), and from then on the two play different pieces, each with
its own ends: +1 and -1 ticks apart. A tick with any other difference is the game's own draws differing."""
import re, struct, sys
KID = 0x40D86 - 0x3DB50; SEED = 0x3DDCA - 0x3DB50
dos = []
for l in open(sys.argv[1], errors="replace"):
    m = re.match(r"frame=(\d+) n=\d+ probe=\d+ (\w+) at", l)
    if not m or m.group(2) != "ds_tick": continue
    d = bytes.fromhex(re.search(r" mem=([0-9A-Fa-f]+)", l).group(1))
    x, y = struct.unpack_from("<hh", d, KID + 2); fr = struct.unpack_from("<H", d, KID + 7)[0]
    dos.append((d[KID + 14], x, y, fr, d[KID + 18], struct.unpack_from("<I", d, SEED)[0]))
core = {}; level = {}
for l in open(sys.argv[2]):
    f = l.split()
    if len(f) >= 7 and f[0].isdigit(): core[int(f[0])] = tuple(int(v) for v in f[1:7]); level[int(f[0])] = f[7] if len(f) > 7 else "?"
step = lambda s: (s * 214013 + 2531011) & 0xFFFFFFFF
def ahead(a, b):   # draws a is ahead of b
    s = b
    for k in range(0, 13):
        if s == a: return k
        s = step(s)
    s = a
    for k in range(0, 13):
        if s == b: return -k
        s = step(s)
    return None
frm = int(sys.argv[3]) if len(sys.argv) > 3 else 1
posbad = []; drawbad = {}; n = 0
for k in range(frm, len(dos)):
    if k not in core: break
    n += 1
    if dos[k][:5] != core[k][:5]: posbad.append(k)
    if dos[k][5] != core[k][5]: drawbad[k] = ahead(dos[k][5], core[k][5])
print("  %d ticks compared (%d..%d)" % (n, frm, frm + n - 1))
print("  position mismatches: %d %s" % (len(posbad), posbad[:12]))
for k in posbad[:3]: print("    tick %d (%s): dos %s core %s" % (k, level[k], dos[k][:5], core[k][:5]))
ks = sorted(drawbad)
pairs = [k for k in ks if drawbad[k] == 1 and drawbad.get(k + 1) == -1]
single = [k for k in ks if k not in pairs and (k - 1) not in pairs]
seg = [frm] + [k + 1 for k in ks if k + 1 < frm + n]
lens = [b - a for a, b in zip(seg, seg[1:] + [frm + n])]
print("  %d segments (DOS by itself in each; the longest %d ticks, the shortest %d); they start on ticks %s%s" % (len(seg), max(lens), min(lens), ", ".join(str(x) for x in seg[:30]), " ..." if len(seg) > 30 else ""))
print("  ticks whose draws differ: %d; of them a draw a tick early in DOS (+1, then -1): %d pairs" % (len(ks), len(pairs)))
print("  the others: %s" % ", ".join("%d (%s): %s" % (k, level[k], "%+d" % drawbad[k] if drawbad[k] is not None else "?") for k in single[:40]))
