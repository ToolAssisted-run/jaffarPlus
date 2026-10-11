#!/usr/bin/env python3
"""draws.py TRS.txt [FIRST LAST]: the random draws a tick of a trs trace (the seed stepped until it meets the next
tick's), summed by room; lines FIRST..LAST of the trace (default all)"""
import sys
rows = []
for l in open(sys.argv[1]):
    p = l.split()
    if len(p) >= 8 and p[0].isdigit(): rows.append((int(p[0]), int(p[1]), int(p[2]), int(p[4]), int(p[6]), p[7]))
if len(sys.argv) > 3: rows = [r for r in rows if int(sys.argv[2]) <= r[0] <= int(sys.argv[3])]
def steps(a, b):
    n = 0
    while a != b:
        a = (a * 0x343FD + 0x269EC3) & 0xFFFFFFFF; n += 1
        if n > 100000: return None
    return n
tot = 0; byroom = {}; per = []
for k in range(1, len(rows)):
    n = steps(rows[k - 1][4], rows[k][4])
    if n is None: per.append((rows[k][0], rows[k][1], "?")); continue
    tot += n; key = (rows[k][5], rows[k][1]); byroom[key] = byroom.get(key, [0, 0]); byroom[key][0] += n; byroom[key][1] += 1
    if n: per.append((rows[k][0], rows[k][1], n))
print("ticks %d..%d: %d draws; first seed %d last seed %d" % (rows[0][0], rows[-1][0], tot, rows[0][4], rows[-1][4]))
print("by level/room (draws/ticks):", " ".join("%s/r%d:%d/%d" % (k[0], k[1], v[0], v[1]) for k, v in byroom.items()))
print("ticks with draws:", " ".join("%d(r%d):%s" % p for p in per[:400]))
