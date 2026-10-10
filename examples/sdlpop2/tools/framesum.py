#!/usr/bin/env python3
"""framesum.py CMP.txt [CMP_BEFORE.txt]: framecmp.py's ticks summed up: how many are equal to the pixel, and room by
room (of the rooms with more than one tick off) how many are not, their median and largest count of differing
pixels; with a second file the same for it beside (the renderer before a change)"""
import collections, statistics, sys
def load(p): return [tuple(map(int, l.split())) for l in open(p)]
new = load(sys.argv[1]); old = {r[0]: r for r in load(sys.argv[2])} if len(sys.argv) > 2 else {}
d = [r[3] for r in new]
print("fixed renderer: %d ticks, exact %d, <=50 px off %d, >50 %d" % (len(d), sum(x == 0 for x in d), sum(0 < x <= 50 for x in d), sum(x > 50 for x in d)))
if old:
    do = [old[r[0]][3] for r in new if r[0] in old]
    print("old renderer:   %d ticks, exact %d, <=50 px off %d, >50 %d" % (len(do), sum(x == 0 for x in do), sum(0 < x <= 50 for x in do), sum(x > 50 for x in do)))
seg = []
for k, row, fr, n, lv, rm in new:
    if seg and seg[-1][0] == (lv, rm): seg[-1][1].append((k, n, old.get(k, (0, 0, 0, -1))[3]))
    else: seg.append(((lv, rm), [(k, n, old.get(k, (0, 0, 0, -1))[3])]))
for (lv, rm), v in seg:
    n = [x[1] for x in v]; o = [x[2] for x in v]
    bad = sum(x > 0 for x in n); obad = sum(x > 0 for x in o)
    if bad > 1 or obad > 1:
        print("L%d room %d (ticks %d-%d, %d): fixed: %d not exact (median %d, max %d) | old: %d not exact (median %d, max %d)" % (lv, rm, v[0][0], v[-1][0], len(v), bad, statistics.median(n), max(n), obad, statistics.median(o), max(o)))
