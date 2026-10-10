#!/usr/bin/env python3
"""frametrip.py CMP.txt DOSVID_PREFIX RAW.bgra OUT.png TICK...: for each tick a row of three pictures: DOS, the core, and
the core's with the differing pixels in magenta (the others dimmed); the diff's bounding box printed"""
import sys, subprocess, numpy as np
cmpf, pre, raw, out = sys.argv[1:5]; ticks = [int(x) for x in sys.argv[5:]]
m = {}
for l in open(cmpf):
    f = l.split(); m[int(f[0])] = (int(f[1]), int(f[2]))
rows = []
for k in ticks:
    row, fr = m[k]
    d = np.fromfile("%s%05d.tga" % (pre, fr), dtype=np.uint8); w = int(d[12]) | int(d[13]) << 8; h = int(d[14]) | int(d[15]) << 8
    a = d[18:18 + w * h * 3].reshape(h, w, 3)
    if not d[17] & 0x20: a = a[::-1]
    a = a[::2, ::2][:200, :320]
    b = np.fromfile(raw, dtype=np.uint8, count=256000, offset=row * 256000).reshape(200, 320, 4)[:, :, :3]
    diff = np.any((a >> 2) != (b >> 2), axis=2)
    c = (b // 3).copy(); c[diff] = (255, 0, 255)
    ys, xs = np.nonzero(diff)
    print("tick %d (row %d, DOS frame %d): %d px differ%s" % (k, row, fr, diff.sum(), "" if not len(ys) else ", box x %d..%d y %d..%d" % (xs.min(), xs.max(), ys.min(), ys.max())))
    rows.append(np.hstack([a, b, c]))
img = np.vstack(rows)
subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", "%dx%d" % (img.shape[1], img.shape[0]), "-i", "-", out], input=img.tobytes(), check=True)
