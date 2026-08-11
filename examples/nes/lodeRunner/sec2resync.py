#!/usr/bin/env python3
"""Resync the win519 section-2 slice (ref frames 119-262) onto the sec1.win.sol seed.

The sec1 win is 3 frames faster than the reference's section 1, so the slice's inputs land on a
phase-shifted world (player 3px deeper in the fall, enemies 1/2 one tile off) and desync around
relative frame 62. Fix: phase-shift null edits (insert/delete nulls), greedy hill-climb scored by
route-odometer depth along sec2.route.txt (same gate semantics as the module), then gold count.

Success = all 47 waypoints consumed in order (ends below the fake brick at (24,9)) with C4>=8.
Result written to sec2.seedref.sol (the section-2 floor/pin reference).
"""
import os, subprocess, sys, tempfile, concurrent.futures

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = os.path.join(HERE, "../../../build-lodeRunner/jaffar-player")
CONFIG = os.path.join(HERE, "sec2hash.tmp.jaffar")  # Initial Sequence = sec1.win.sol
NULL = "|..|........|"
RAMSZ = 2048
MAX_WORKERS = 12
MAX_ROUNDS = 30
WORKDIR = os.path.join(tempfile.gettempdir(), "sec2resync")

ROUTE = [tuple(map(int, l.split())) for l in open(os.path.join(HERE, "sec2.route.txt")) if l.strip()]


def loadSol(p):
    return [l.rstrip("\n") for l in open(p) if l.strip()]


def saveSol(lines, p):
    open(p, "w").write("\n".join(lines) + "\n")


def replayScore(tag, tail):
    """Returns (routeProgress, goldMax, stallFrame): stallFrame = frame of last odometer advance
    (or off-route break), the anchor for the next round's edit window."""
    sol = os.path.join(WORKDIR, f"c_{tag}.sol")
    ram = os.path.join(WORKDIR, f"c_{tag}.ram")
    saveSol(tail, sol)
    subprocess.run([PLAYER, CONFIG, sol, "--disableRender", "--unattended", "--exitOnEnd",
                    "--dumpRam", ram], capture_output=True)
    if not os.path.exists(ram):
        return (-1, 0, 0)
    buf = open(ram, "rb").read()
    n = len(buf) // RAMSZ
    prog, prevT, gmax, lastAdv = 0, None, 0, 0
    for fr in range(n):
        b = buf[fr * RAMSZ:(fr + 1) * RAMSZ]
        t, g = (b[0x20], b[0x21]), b[0xC4]
        gmax = max(gmax, g)
        before = prog
        while prog < len(ROUTE) and (ROUTE[prog][0], ROUTE[prog][1]) == t and g >= ROUTE[prog][2]:
            prog += 1
        if prog != before:
            lastAdv = fr
        if prevT is not None and t != prevT and prog == before:
            lastAdv = fr
            break  # off-route tile change
        prevT = t
    os.remove(ram); os.remove(sol)
    return (prog, gmax, lastAdv)


def inputRuns(tail, lo, hi):
    """Maximal runs of identical inputs [(start, length, input)] overlapping [lo, hi)."""
    runs, i = [], 0
    while i < len(tail):
        j = i
        while j < len(tail) and tail[j] == tail[i]:
            j += 1
        if j > lo and i < hi:
            runs.append((i, j - i, tail[i]))
        i = j
    return runs


def main():
    os.makedirs(WORKDIR, exist_ok=True)
    tail = loadSol(os.path.join(HERE, "sec2.ref.sol"))
    best = replayScore("base", tail)
    print(f"round 0: score {best} (progress/{len(ROUTE)} wps, gold, -len)", flush=True)
    for rnd in range(1, MAX_ROUNDS + 1):
        if best[0] >= len(ROUTE) and best[1] >= 8:
            break
        # Edit vocabulary: for every held-input run near the stall front, extend it by 1..3 copies
        # or shrink it by 1..3 (a positional phase-shift, not just a temporal one). The window
        # reaches back 45 frames from the stall so speed-phase drift accumulated upstream of the
        # failing action is also correctable.
        stall = best[2]
        lo, hi = 0, min(len(tail), stall + 15)
        cands = []
        for start, length, inp in inputRuns(tail, lo, hi):
            for k in (1, 2, 3):
                cands.append((f"x{start}+{k}", tail[:start] + [inp] * k + tail[start:]))
                if length > k:
                    cands.append((f"s{start}-{k}", tail[:start] + tail[start + k:]))
        results = []
        with concurrent.futures.ThreadPoolExecutor(MAX_WORKERS) as ex:
            futs = {ex.submit(replayScore, f"r{rnd}_{tag}", t): (tag, t) for tag, t in cands}
            for f in concurrent.futures.as_completed(futs):
                results.append((f.result(), *futs[f]))
        results.sort(key=lambda x: (x[0][0], x[0][1], -x[0][2]), reverse=True)
        top, tag, cand = results[0]
        if (top[0], top[1], -top[2]) <= (best[0], best[1], -best[2]):
            print(f"round {rnd}: no improving edit (best probe {top} via {tag}); stopping", flush=True)
            break
        best, tail = top, cand
        print(f"round {rnd}: applied {tag} -> score {best}, len {len(tail)}", flush=True)
    out = os.path.join(HERE, "sec2.seedref.sol")
    saveSol(tail, out)
    ok = best[0] >= len(ROUTE) and best[1] >= 8
    print(f"{'SUCCESS' if ok else 'FAILED'}: final score {best}, written {out} ({len(tail)} inputs)", flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
