#!/usr/bin/env python3
"""Resync repair of the stage02 reference tail onto the k=3 fast seed (press@1077).

Greedy hill-climb: each round probes single null-run insert/delete edits (1-2 frames) across
the tail, replays each candidate from power-on, and scores by choreography depth:
    (pickups reached, gold0 reached, genuine flip reached, -|total shift|)
The best strictly-improving edit is applied and the loop repeats. Success = genuine stage03
flip (play mode 1, gold0 first). Time-boxed by MAX_ROUNDS.
"""
import os, sys, multiprocessing as mp

HERE = "/home/jaffar/jaffarPlus/examples/nes/lodeRunner"
sys.path.insert(0, HERE)
from resyncTool import NULL, RamDump, loadSol, saveSol, replay, WORKDIR, nullRunsIn

MAX_ROUNDS = 10
S  = "|..|.....s..|"
ST = "|..|....S...|"
LS = "|..|..L..s..|"


def buildPrefix():
    full = loadSol(os.path.join(HERE, "stage01.win721.full.sol"))
    base = full[:720] + [NULL] * 3 + full[720:]          # k=3 flip delay -> $53=22
    seq  = list(base)
    seq += [NULL] * (734 - len(seq)) + [S]               # celebration skip (poll grid)
    seq += [NULL] * (739 - len(seq)) + [ST]              # transition start
    seq += [NULL] * (1077 - len(seq)) + [LS]             # scroll-cancel press
    return seq                                            # play from 1078


PREFIX = buildPrefix()
PLAY0  = len(PREFIX)


def score(tag, tail):
    sol = os.path.join(WORKDIR, f"rt_{tag}.sol")
    ram = os.path.join(WORKDIR, f"rt_{tag}.ram")
    saveSol(PREFIX + tail + [NULL] * 60, sol)
    res = (-1, 0, 0, 0)
    try:
        if replay(sol, ram, timeout=300):
            d = RamDump(ram)
            picks = max(d.b(f, 0x00C4) for f in range(PLAY0, d.frames))
            g0 = next((f for f in range(PLAY0, d.frames) if d.b(f, 0x0093) == 0 and d.b(f, 0x00DB) == 1 and d.b(f, 0x00A6) == 2), None)
            flip = None
            if g0 is not None:
                flip = next((f for f in range(g0, d.frames) if d.b(f, 0x00A6) == 3), None)
            res = (picks, 1 if g0 is not None else 0, 1 if flip is not None else 0, flip if flip is not None else 0)
    finally:
        for p in (sol, ram):
            try: os.remove(p)
            except OSError: pass
    return res


def probe(args):
    kind, pos, count, tail = args
    if kind == "insNull":
        t = tail[:pos] + [NULL] * count + tail[pos:]
    elif kind == "insCopy":
        line = tail[pos] if pos < len(tail) else NULL
        t = tail[:pos] + [line] * count + tail[pos:]
    else:
        if any(tail[pos + i] != NULL for i in range(count) if pos + i < len(tail)): return (kind, pos, count, (-1, 0, 0, 0))
        t = tail[:pos] + tail[pos + count:]
    return (kind, pos, count, score(f"{kind}{pos}x{count}", t))


def main():
    tail = loadSol(os.path.join(HERE, "stage02.reference.sol"))
    cur = score("base", tail)
    print(f"baseline: picks={cur[0]} gold0={cur[1]} flip={cur[2]}", flush=True)
    for rnd in range(MAX_ROUNDS):
        if cur[2] == 1:
            print(f"SUCCESS: flip at {cur[3]}", flush=True)
            break
        nulls = nullRunsIn(tail, 0, len(tail))
        tasks  = [(k, p, c, tail) for p in range(len(tail)) for k in ("insNull", "insCopy") for c in (1, 2)]
        tasks += [("delete", p, c, tail) for p in nulls for c in (1, 2)]
        print(f"round {rnd}: {len(tasks)} probes (current picks={cur[0]})", flush=True)
        best = None
        with mp.Pool(12) as pool:
            for kind, pos, count, sc in pool.imap_unordered(probe, tasks, chunksize=4):
                if sc > cur and (best is None or sc > best[3]):
                    best = (kind, pos, count, sc)
        if best is None:
            print("no improving edit found -- stuck", flush=True)
            break
        kind, pos, count, sc = best
        if kind == "insNull": tail = tail[:pos] + [NULL] * count + tail[pos:]
        elif kind == "insCopy": tail = tail[:pos] + [tail[pos]] * count + tail[pos:]
        else: tail = tail[:pos] + tail[pos + count:]
        cur = sc
        print(f"  applied {kind}@{pos}x{count} -> picks={cur[0]} gold0={cur[1]} flip={cur[2]} ({cur[3]})", flush=True)
        saveSol(tail, os.path.join(HERE, "stage02.reference.k3repair.sol"))
    saveSol(tail, os.path.join(HERE, "stage02.reference.k3repair.sol"))
    print(f"final: picks={cur[0]} gold0={cur[1]} flip={cur[2]} flipFrame={cur[3]}", flush=True)


if __name__ == "__main__":
    main()
