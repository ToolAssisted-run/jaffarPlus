#!/usr/bin/env python3
"""ocmp.py SNAP HEADLESS.txt: a DOS oracle capture (ds_tick events: the data segment from phys 3DB50 at each tick's
start) against the headless core's per-tick lines (tick room x y frame hp seed, the state after that tick): the DOS
state at the start of tick k+1 is the core's after tick k. Prints the levels' tick counts and the first mismatch."""
import re, struct, sys
KID = 0x40D86 - 0x3DB50; SEED = 0x3DDCA - 0x3DB50
dos = []; marks = []
for l in open(sys.argv[1], errors="replace"):
    m = re.match(r"frame=(\d+) n=\d+ probe=\d+ (\w+) at", l)
    if not m: continue
    if m.group(2) != "ds_tick":
        marks.append((len(dos), m.group(2), int(m.group(1)), re.search(r"ax=([0-9A-F]{4})", l).group(1))); continue
    h = re.search(r" mem=([0-9A-Fa-f]+)", l).group(1); d = bytes.fromhex(h)
    x, y = struct.unpack_from("<hh", d, KID + 2); fr = struct.unpack_from("<H", d, KID + 7)[0]
    dos.append((d[KID + 14], x, y, fr, d[KID + 18], struct.unpack_from("<I", d, SEED)[0], int(m.group(1))))
for n, k, f, ax in marks: print("  %s (ax %s) at frame %d after %d ticks" % (k, ax, f, n))
core = {}
for l in open(sys.argv[2]):
    f = l.split()
    if len(f) >= 7 and f[0].isdigit(): core[int(f[0])] = tuple(int(v) for v in f[1:7])
n = 0; bad = None
for k in range(1, len(dos)):          # dos[k] = the start of tick k+1 = the core after tick k
    if k not in core: break
    if dos[k][:6] != core[k]: bad = (k, dos[k][:6], core[k]); break
    n += 1
print("  %d DOS ticks, %d compared equal (room x y frame hp seed); first mismatch: %s" % (len(dos), n, bad))
print("  seed at the last DOS tick: %d" % dos[-1][5])
