#!/usr/bin/env python3
"""framecmp2.py SNAP DOSVID_PREFIX RAW.bgra CMP_IN.txt CMP_OUT.txt: the ticks of CMP_IN (framecmp.py) that differ, tried
again against every DOS frame from two before tick k starts to one after tick k + 1 starts: DOS shows a tick's
picture sooner in a fight (6/60 s a tick) than walking, and the three frames of the first pass miss it there."""
import sys, numpy as np
snap, pre, raw, cin, cout = sys.argv[1:6]
F = []
for l in open(snap, errors="replace"):
    if " ds_tick at" in l: F.append(int(l[6:l.index(" ")]))
def dos(fr):
    try: d = np.fromfile("%s%05d.tga" % (pre, fr), dtype=np.uint8)
    except FileNotFoundError: return None
    w = int(d[12]) | int(d[13]) << 8; h = int(d[14]) | int(d[15]) << 8
    if w != 640 or h != 400: return None
    return d[18:18 + w * h * 3].reshape(h, w, 3)[::2, ::2] >> 2
rf = open(raw, "rb"); o = open(cout, "w"); n = fixed = 0
for l in open(cin):
    k, row, fr, d0, lv, rm = [int(x) for x in l.split()]
    if d0 == 0 or k + 1 >= len(F): o.write(l); continue
    rf.seek(row * 256000); m = np.frombuffer(rf.read(256000), dtype=np.uint8).reshape(200, 320, 4)[:, :, :3] >> 2
    best = (d0, fr)
    for f in range(F[k - 1] - 2, F[k] + 2):
        a = dos(f)
        if a is None: continue
        d = int(np.count_nonzero(np.any(a != m, axis=2)))
        if d < best[0]: best = (d, f)
        if d == 0: break
    n += 1; fixed += best[0] == 0
    o.write("%d %d %d %d %d %d\n" % (k, row, best[1], best[0], lv, rm))
print(n, "ticks tried again,", fixed, "of them exact on another frame")
