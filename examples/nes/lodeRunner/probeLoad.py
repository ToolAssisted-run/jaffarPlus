#!/usr/bin/env python3
"""Load-window NMI-manipulation probe, v1 seed (menu 0-90 declared optimal, speed 1 asserted).

Stage A patterns over the load window [91, 413]:
  - hold <input> across the whole window
  - alternate <input>/null across the whole window (fresh press edge every 2 frames)
  - hold <input> across each quarter of the window
Inputs: 8 singles + heavy combos (more handler branches per poll = more cycle perturbation).
Readiness metric asserts Game Speed $E5==1 (the v2 lesson) + DB==1, lvl==1, gold==6.
Baseline readiness = 414. Any earlier readiness = HIT (NMI/loading alignment shifted).

Also probes the post-ready hold (414-427): a skip press at each frame with an insert during
the hold, looking for anything that shortens the 13-frame swallow.

Usage: probeLoad.py [outFile]
"""
import os, sys, multiprocessing as mp

HERE = "/home/jaffar/jaffarPlus/examples/nes/lodeRunner"
sys.path.insert(0, HERE)
from resyncTool import NULL, RamDump, loadSol, saveSol, replay, WORKDIR

# input line format: |..|UDLRSsBA|
def mk(bits):
    s = list("........")
    for i, ch in zip((0, 1, 2, 3, 4, 5, 6, 7), "UDLRSsBA"):
        if ch in bits: s[i] = ch
    return "|..|" + "".join(s) + "|"

SINGLES = ["U", "D", "L", "R", "s", "B", "A", "S"]
COMBOS  = ["AB", "UDLR", "UDLRsBA", "AsB", "LRA", "UDA"]
W0, W1  = 91, 413
BASE    = 414
PRESS   = "|..|..L..s..|"


def readiness(d):
    for f in range(d.frames):
        if d.b(f, 0x00DB) == 1 and d.b(f, 0x00A6) == 1 and d.b(f, 0x0093) == 6 and d.b(f, 0x00E5) == 1:
            return f
    return None


def probe(task):
    tag, lines = task
    sol = os.path.join(WORKDIR, tag + ".sol")
    ram = os.path.join(WORKDIR, tag + ".ram")
    saveSol(lines + [NULL] * 40, sol)
    ok = replay(sol, ram, timeout=300)
    r = readiness(RamDump(ram)) if ok else None
    for p in (sol, ram):
        try: os.remove(p)
        except OSError: pass
    return (tag, r)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "probeLoad.results")
    os.makedirs(WORKDIR, exist_ok=True)
    seed = loadSol(os.path.join(HERE, "stage01.initial.sol"))[:428]
    tasks = []
    for key in SINGLES + COMBOS:
        line = mk(key)
        # hold across whole window
        l = list(seed)
        for f in range(W0, W1 + 1): l[f] = line
        tasks.append((f"hold.{key}", l[:BASE]))
        # alternate across whole window
        l = list(seed)
        for f in range(W0, W1 + 1, 2): l[f] = line
        tasks.append((f"alt.{key}", l[:BASE]))
        # hold per quarter
        span = (W1 - W0 + 1) // 4
        for q in range(4):
            a = W0 + q * span
            b = a + span - 1 if q < 3 else W1
            l = list(seed)
            for f in range(a, b + 1): l[f] = line
            tasks.append((f"q{q}.{key}", l[:BASE]))
    print(f"{len(tasks)} stage-A probes", flush=True)
    hits = []
    with mp.Pool(12) as pool:
        for i, (tag, r) in enumerate(pool.imap_unordered(probe, tasks, chunksize=2)):
            if r is not None and r < BASE:
                hits.append((r, tag))
                print(f"HIT: {tag} -> ready {r} (-{BASE - r})", flush=True)
            elif r is None:
                print(f"  note: {tag} -> never ready (broke the load)", flush=True)
            if (i + 1) % 20 == 0:
                print(f"  ...{i + 1}/{len(tasks)}", flush=True)
    hits.sort()
    with open(out, "w") as fp:
        fp.write("ready\tdelta\tpattern\n")
        for r, tag in hits:
            fp.write(f"{r}\t{BASE - r}\t{tag}\n")
    print(f"stage A done: {len(hits)} hits -> {out}", flush=True)

    # hold-phase probe: insert single input at hf in [414..427], skip press at pf in [420..428]
    print("hold-phase probe (earliest effective skip press with hold-window inserts)", flush=True)
    seedF = loadSol(os.path.join(HERE, "stage01.initial.sol"))[:428]
    tasks2 = []
    for key in SINGLES + ["AB"]:
        line = mk(key)
        for hf in range(414, 428):
            for pf in range(max(hf + 1, 420), 429):
                l = list(seedF[:pf]) + [PRESS]
                l[hf] = line
                tasks2.append((f"hp.{key}.{hf}.{pf}", l))
    print(f"{len(tasks2)} hold-phase probes", flush=True)
    best = []
    def playStarted(d, pf):
        # skip landed = camX stops <=2 AND player responds: check gt running and camX small at pf+6
        cam = [d.b(f, 0x0004) for f in range(pf, min(pf + 8, d.frames))]
        db  = d.b(min(pf + 8, d.frames - 1), 0x00DB)
        gtA = d.b(min(pf + 2, d.frames - 1), 0x009E)
        gtB = d.b(min(pf + 8, d.frames - 1), 0x009E)
        return db == 1 and max(cam) <= 2 and gtB != gtA
    def probe2(task):
        tag, lines = task
        sol = os.path.join(WORKDIR, tag + ".sol")
        ram = os.path.join(WORKDIR, tag + ".ram")
        saveSol(lines + [NULL] * 30, sol)
        ok = replay(sol, ram, timeout=300)
        res = None
        if ok:
            d = RamDump(ram)
            pf = len(lines) - 1
            if readiness(d) is not None and playStarted(d, pf): res = pf
        for p in (sol, ram):
            try: os.remove(p)
            except OSError: pass
        return (tag, res)
    with mp.Pool(12) as pool:
        for tag, pf in pool.imap_unordered(probe2, tasks2, chunksize=8):
            if pf is not None and pf < 428:
                best.append((pf, tag))
                print(f"HOLD-PHASE HIT: {tag} -> effective press at {pf} (-{428 - pf})", flush=True)
    best.sort()
    with open(out, "a") as fp:
        for pf, tag in best:
            fp.write(f"holdphase\t{428 - pf}\t{tag}\n")
    print(f"done: {len(best)} hold-phase hits", flush=True)


if __name__ == "__main__":
    main()
