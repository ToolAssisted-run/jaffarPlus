#!/usr/bin/env python3
"""reuse.py GAMEDIR SEED OLDCHAIN.sol NEWCHAIN.sol OLDLEVEL.sol [NEWLEVEL.sol [NEWTICK OLDTICK]]

Version 2 (a level changed earlier in the run: the later levels start on another random seed and tick). A level's
old solution is kept wherever the game plays it the same: OLDLEVEL.sol (lNNcut.sol) after OLDCHAIN.sol against
NEWLEVEL.sol (default: the same lines) after NEWCHAIN.sol, tick by tick (tools/trj), the tick number and the seed
aside. With NEWTICK OLDTICK (a join: the new level's line NEWTICK leaves the prince as the old one's line OLDTICK
did, the old lines following it from there), the lines after the join are compared, new NEWTICK + i with old
OLDTICK + i.

Prints the first tick the prince differs on (trj's fields), the first tick the tiles differ on (their hash), the
first tick the drawn room's characters differ on, the random draws of both, and whether the next level loads on
the last line of both. Then, for cutting a stretch to search anew (tools/segment.sh): the old solution's rooms with
the ticks they are entered on, and after the first difference the old ticks where the prince hangs, climbs or
stands still (states to join on: his place there does not depend on what came before).
env TRJ (default ~/pop2tools-m/trj)."""
import os, subprocess, sys
game, seed, oldc, newc, oldl = sys.argv[1:6]
newl = sys.argv[6] if len(sys.argv) > 6 else oldl
jn, jo = (int(sys.argv[7]), int(sys.argv[8])) if len(sys.argv) > 8 else (0, 0)
TRJ = os.environ.get("TRJ", os.path.expanduser("~/pop2tools-m/trj"))
def trace(chain, lev):
    rows = [None]
    for l in subprocess.run([TRJ, game, seed, "0", "0", chain, lev], capture_output=True, text=True).stdout.splitlines():
        p = l.split("|")
        if len(p) < 5 or not p[0].split()[0].isdigit(): continue
        rows.append({"lv": int(p[0].split()[1][1:]), "kid": tuple(int(v) for v in p[1].split()), "seed": int(p[2]), "tiles": p[3].strip(), "chars": p[4].strip(), "line": l})
    return rows
def draws(rows, a):
    n = 0
    for k in range(a + 1, len(rows)):
        s, e, c = rows[k - 1]["seed"], rows[k]["seed"], 0
        while s != e and c < 100000: s = (s * 0x343FD + 0x269EC3) & 0xFFFFFFFF; c += 1
        n += c
    return n
old, new = trace(oldc, oldl), trace(newc, newl)
no, nn = len(old) - 1, len(new) - 1
n = min(no - jo, nn - jn)
def first(key):
    return next((i for i in range(1, n + 1) if (new[jn + i]["lv"], new[jn + i][key]) != (old[jo + i]["lv"], old[jo + i][key])), None)
print("old %d lines, new %d lines; compared from new %d / old %d on: %d lines; draws in them: old %d, new %d" % (no, nn, jn + 1, jo + 1, n, draws(old, max(jo, 1)), draws(new, max(jn, 1))))
if jn or jo: print("the join: new %d: %s\n          old %d: %s" % (jn, new[jn]["line"][:150], jo, old[jo]["line"][:150]))
res = {}
for name, key in (("the prince", "kid"), ("the tiles", "tiles"), ("a character", "chars")):
    i = res[key] = first(key)
    if i is None: print("%s: never differs" % name)
    else: print("%s differs first on new tick %d / old tick %d:\n  old %s\n  new %s" % (name, jn + i, jo + i, old[jo + i]["line"][:260], new[jn + i]["line"][:260]))
lv0 = old[1]["lv"]
ends = nn - jn == no - jo and new[nn]["lv"] == old[no]["lv"] == lv0 + 1 and new[nn - 1]["lv"] == lv0
last = nn - jn == no - jo and new[nn]["lv"] == old[no]["lv"] == lv0 == 14   # (the last level: no level after it)
good = res["kid"] is None and (ends or last)
print("the last line: old %d (L%d), new %d (L%d)" % (no, old[no]["lv"], nn, new[nn]["lv"]))
print("GOOD AS IT IS: %d lines" % nn if good else "NOT GOOD AS IT IS")
if not good:
    rooms = []; r = None
    for k in range(1, no + 1):
        if old[k]["kid"][0] != r: r = old[k]["kid"][0]; rooms.append("%d@%d" % (r, k))
    print("old rooms (room@tick):", " ".join(rooms))
    d = min([v for v in res.values() if v is not None], default=None)
    if d is not None:
        HANG = set(range(87, 100)) | set(range(135, 150)) | {15, 109}
        c = [k for k in range(jo + d, no + 1) if old[k]["kid"][3] in HANG and old[k]["kid"][12] == 0]
        runs = []; last = None   # the first tick of each run of such frames
        for k in c:
            if last is None or k != last + 1: runs.append(k)
            last = k
        print("old ticks to join on after the first difference (frame):", " ".join("%d(f%d r%d)" % (k, old[k]["kid"][3], old[k]["kid"][0]) for k in runs[:40]))
