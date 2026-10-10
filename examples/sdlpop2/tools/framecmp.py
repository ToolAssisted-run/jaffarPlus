#!/usr/bin/env python3
"""framecmp.py SNAP DOSVID_PREFIX RAW.bgra TRACE.txt OUT.txt [STEP]: the run's pictures against DOS's, a game tick at a
time. SNAP: the oracle's events of the run that dumped DOSVID_PREFIX%05d.tga (640x400, every frame); RAW.bgra: the
core runner's frames (GATE_RAWVIDEO, 320x200 BGRA, a movie row each); TRACE: its trace with "IGT Ticks" last (the
row of line k is the first whose IGT is k - 1). DOS's picture of tick k is taken from the frames just before tick
k + 1 starts (the least different of three: the screen is being written during some). OUT: a line a tick: tick,
row, DOS frame, pixels differing (of 64000, colors compared at 6 bits), level, room."""
import re, sys, numpy as np
snap, pre, raw, trace, out = sys.argv[1:6]; step = int(sys.argv[6]) if len(sys.argv) > 6 else 1
F = []
for l in open(snap, errors="replace"):
    if " ds_tick at" in l: F.append(int(l[6:l.index(" ")]))
rowof = {}; info = {}
for l in open(trace):
    f = l.split()
    if f and f[0].isdigit():
        igt = int(float(f[-1])); r = int(f[0])
        if int(f[3]) >= 1 and igt + 1 not in rowof: rowof[igt + 1] = r
        info[r] = (int(f[3]), int(f[4]))
def dos(fr):
    try: d = np.fromfile("%s%05d.tga" % (pre, fr), dtype=np.uint8)
    except FileNotFoundError: return None
    w = int(d[12]) | int(d[13]) << 8; h = int(d[14]) | int(d[15]) << 8
    a = d[18:18 + w * h * 3].reshape(h, w, 3)
    if not d[17] & 0x20: a = a[::-1]
    if w == 640: a = a[::2, ::2]
    elif h == 400: a = a[::2]
    return a[:200, :320] >> 2   # BGR
rf = open(raw, "rb")
def mine(row):
    rf.seek(row * 256000); b = np.frombuffer(rf.read(256000), dtype=np.uint8)
    return b.reshape(200, 320, 4)[:, :, :3] >> 2 if len(b) == 256000 else None
o = open(out, "w"); n = 0
for k in range(1, len(F), step):
    if k not in rowof: continue
    m = mine(rowof[k])
    if m is None: continue
    best = None
    for fr in (F[k] - 1, F[k] - 2, F[k]):
        a = dos(fr)
        if a is None or a.shape != m.shape: continue
        d = int(np.count_nonzero(np.any(a != m, axis=2)))
        if best is None or d < best[0]: best = (d, fr)
    if best is None: continue
    lv, rm = info[rowof[k]]
    o.write("%d %d %d %d %d %d\n" % (k, rowof[k], best[1], best[0], lv, rm)); n += 1
print(n, "ticks compared")
