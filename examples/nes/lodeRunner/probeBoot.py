#!/usr/bin/env python3
"""Boot/loading-time probe (Bomberman NMI-timing principle, same developer).

Hypothesis: extra inputs during the boot->level-load window shift the NMI/loading alignment
and make the level ready EARLIER than the baseline frame 414 (metric: first frame with
Play Mode $DB==1, Current Level $A6==1, Gold $93==6).

Phase 1 sweep: for every null frame f of the seed's pre-play span and every single input,
insert that input at f (leaving all required presses untouched), replay from power-on, and
record the readiness frame R. Reports every candidate with R < baseline, ranked.

Usage: probeBoot.py [outFile]  (writes TSV: R, delta, frame, input)
"""
import os, sys, multiprocessing as mp

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from resyncTool import NULL, RamDump, loadSol, saveSol, replay, WORKDIR

INPUTS = {
    'A': '|..|.......A|',
    'B': '|..|......B.|',
    'S': '|..|....S...|',
    's': '|..|.....s..|',
    'U': '|..|U.......|',
    'D': '|..|.D......|',
    'L': '|..|..L.....|',
    'R': '|..|...R....|',
}
READY_BASE = 414
SCAN_TO    = 470


def readiness(dump):
    for f in range(dump.frames):
        if dump.b(f, 0x00DB) == 1 and dump.b(f, 0x00A6) == 1 and dump.b(f, 0x0093) == 6:
            return f
    return None


def probe(task):
    f, key, seed = task
    tag = f"probe.{f:03d}.{key}"
    sol = os.path.join(WORKDIR, tag + ".sol")
    ram = os.path.join(WORKDIR, tag + ".ram")
    lines = list(seed)
    lines[f] = INPUTS[key]
    saveSol(lines[:SCAN_TO] + [NULL] * 20, sol)
    ok = replay(sol, ram, timeout=300)
    r = readiness(RamDump(ram)) if ok else None
    for p in (sol, ram):
        try: os.remove(p)
        except OSError: pass
    return (f, key, r)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "probeBoot.results")
    os.makedirs(WORKDIR, exist_ok=True)
    seed = loadSol(os.path.join(HERE, "stage01.initial.sol"))
    nullFrames = [f for f in range(0, READY_BASE) if seed[f] == NULL]
    tasks = [(f, k, seed) for f in nullFrames for k in INPUTS]
    print(f"{len(nullFrames)} null frames x {len(INPUTS)} inputs = {len(tasks)} probes", flush=True)
    hits = []
    with mp.Pool(12) as pool:
        for i, (f, key, r) in enumerate(pool.imap_unordered(probe, tasks, chunksize=8)):
            if r is not None and r < READY_BASE:
                hits.append((r, f, key))
                print(f"HIT: insert {key}@{f} -> ready at {r} (-{READY_BASE - r})", flush=True)
            if (i + 1) % 200 == 0:
                print(f"  ...{i + 1}/{len(tasks)} probed, {len(hits)} hits", flush=True)
    hits.sort()
    with open(out, "w") as fp:
        fp.write("ready\tdelta\tframe\tinput\n")
        for r, f, key in hits:
            fp.write(f"{r}\t{READY_BASE - r}\t{f}\t{key}\n")
    print(f"done: {len(hits)} hits -> {out}", flush=True)
    if hits:
        r, f, key = hits[0]
        print(f"BEST: {key}@{f} -> ready {r} (-{READY_BASE - r} frames)", flush=True)


if __name__ == "__main__":
    main()
