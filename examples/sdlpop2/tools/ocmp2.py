#!/usr/bin/env python3
"""ocmp2.py SNAP CORE.txt [FROM]: as ocmp.py, but past the first mismatch: positions compared on every tick, and the
random seed as a number of draws DOS is ahead of the core (LCG steps, -8..+8; '?' if none)."""
import re, struct, sys
KID = 0x40D86 - 0x3DB50; SEED = 0x3DDCA - 0x3DB50
dos = []
for l in open(sys.argv[1], errors="replace"):
    m = re.match(r"frame=(\d+) n=\d+ probe=\d+ (\w+) at", l)
    if not m or m.group(2) != "ds_tick": continue
    d = bytes.fromhex(re.search(r" mem=([0-9A-Fa-f]+)", l).group(1))
    x, y = struct.unpack_from("<hh", d, KID + 2); fr = struct.unpack_from("<H", d, KID + 7)[0]
    dos.append((d[KID + 14], x, y, fr, d[KID + 18], struct.unpack_from("<I", d, SEED)[0]))
core = {}
for l in open(sys.argv[2]):
    f = l.split()
    if len(f) >= 7 and f[0].isdigit(): core[int(f[0])] = tuple(int(v) for v in f[1:7])
step = lambda s: (s * 214013 + 2531011) & 0xFFFFFFFF
def ahead(a, b):   # draws a is ahead of b
    s = b
    for k in range(0, 9):
        if s == a: return k
        s = step(s)
    s = a
    for k in range(0, 9):
        if s == b: return -k
        s = step(s)
    return None
frm = int(sys.argv[3]) if len(sys.argv) > 3 else 1
prev = 0; posbad = []
for k in range(frm, len(dos)):
    if k not in core: break
    if dos[k][:5] != core[k][:5]: posbad.append(k)
    a = ahead(dos[k][5], core[k][5])
    if a != prev: print("  tick %d: DOS %s draws ahead (was %s)  dos %s core %s" % (k, a, prev, dos[k], core[k])); prev = a
print("  position mismatches:", len(posbad), posbad[:12])
