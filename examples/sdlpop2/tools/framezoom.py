#!/usr/bin/env python3
"""framezoom.py CMP.txt DOSVID_PREFIX RAW.bgra OUT.png SCALE TICK:X0:Y0:X1:Y1 ...: DOS | core crops, enlarged"""
import sys, subprocess, numpy as np
cmpf, pre, raw, out, sc = sys.argv[1:6]; sc = int(sc)
m = {}
for l in open(cmpf):
    f = l.split(); m[int(f[0])] = (int(f[1]), int(f[2]))
rows = []; W = 0
for spec in sys.argv[6:]:
    k, x0, y0, x1, y1 = [int(v) for v in spec.split(":")]
    row, fr = m[k]
    d = np.fromfile("%s%05d.tga" % (pre, fr), dtype=np.uint8); w = int(d[12]) | int(d[13]) << 8; h = int(d[14]) | int(d[15]) << 8
    a = d[18:18 + w * h * 3].reshape(h, w, 3)
    if not d[17] & 0x20: a = a[::-1]
    a = a[::2, ::2][:200, :320]
    b = np.fromfile(raw, dtype=np.uint8, count=256000, offset=row * 256000).reshape(200, 320, 4)[:, :, :3]
    ca, cb = a[y0:y1, x0:x1], b[y0:y1, x0:x1]
    sep = np.full((y1 - y0, 2, 3), 255, dtype=np.uint8)
    r = np.hstack([ca, sep, cb]).repeat(sc, axis=0).repeat(sc, axis=1)
    rows.append(r); W = max(W, r.shape[1])
    da = np.unique(ca.reshape(-1, 3), axis=0); db = np.unique(cb.reshape(-1, 3), axis=0)
    print("tick %d: %d colors in DOS's crop, %d in the core's; differing px %d" % (k, len(da), len(db), np.any(ca != cb, axis=2).sum()))
rows = [np.hstack([r, np.zeros((r.shape[0], W - r.shape[1], 3), dtype=np.uint8)]) for r in rows]
img = np.vstack(rows)
subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", "%dx%d" % (img.shape[1], img.shape[0]), "-i", "-", out], input=img.tobytes(), check=True)
