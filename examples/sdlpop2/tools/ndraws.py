#!/usr/bin/env python3
"""ndraws.py CHAIN.sol...: the random draws since the game's start (the seed stepped from 7881336) at each level's
first line of each chain"""
import sys, subprocess
def seeds(chain):
    out = subprocess.run(["/home/jaffar/pop2tools-m/trs", "prince2", "7881336", "0", "0", "/dev/shm/empty.sol", chain], capture_output=True, text=True).stdout.splitlines()
    r = []
    for l in out:
        p = l.split()
        if len(p) >= 8 and p[0].isdigit(): r.append((int(p[0]), int(p[6]), p[7]))
    return r
def counts(rows):
    lv = None; firsts = []
    for t, sd, l in rows:
        if l != lv: lv = l; firsts.append((l, t, sd))
    targets = {sd for _, _, sd in firsts}; pos = {}; s = 7881336; n = 0
    while len(pos) < len(targets) and n < 5000000:
        if s in targets and s not in pos: pos[s] = n
        s = (s * 0x343FD + 0x269EC3) & 0xFFFFFFFF; n += 1
    return [(l, t, sd, pos.get(sd)) for l, t, sd in firsts]
for c in sys.argv[1:]:
    print(c.split("/")[-3] if "v1-work" not in c else "old", " ".join("%s@%d:%s" % (l, t, n) for l, t, sd, n in counts(seeds(c))))
