#!/usr/bin/env python3
"""Transition-alignment search: find input-timing levers that make stage02's board+state at
play start match the reference's, starting from OUR fast stage01 (win721).

Target vector: the reference's persistent LRAM state at its scroll-cancel press (frame 1087),
scored over CAUSAL bytes (counters, enemy AI, camera, audio engine), ignoring known-cosmetic
state (score digits, stack page, per-frame scratch).

Lever space (staged search):
  stage A: (k flip-delay, select slot, start frame) -> replay to READY, score build-state
  stage B: top-N stage-A candidates x (press frame sweep) -> score play-start state
  stage C: best stage-B candidates -> full reference-tail resync oracle

Usage: alignTool.py [topN_A] [topN_B]
"""
import os, sys, multiprocessing as mp

HERE = "/home/jaffar/jaffarPlus/examples/nes/lodeRunner"
sys.path.insert(0, HERE)
from resyncTool import NULL, RamDump, loadSol, saveSol, replay, WORKDIR

S  = "|..|.....s..|"
ST = "|..|....S...|"
LS = "|..|..L..s..|"

# Causal byte set for scoring (LRAM addresses), with weights.
CAUSAL = {}
for a in (0x53, 0x9E, 0x9F): CAUSAL[a] = 10.0            # free-running counters
for a in range(0x661, 0x674): CAUSAL[a] = 8.0            # enemy pos/timers
for a in range(0x679, 0x67C): CAUSAL[a] = 8.0            # enemy AI substate
CAUSAL[0x04] = 6.0                                        # camera X
for a in (0xB7, 0xB8, 0xB9): CAUSAL[a] = 4.0             # audio engine
for a in range(0xD0, 0xE0): CAUSAL[a] = 4.0              # audio work
for a in (0x93, 0xC4, 0xC5, 0xA6, 0xDB, 0xE5): CAUSAL[a] = 20.0   # game basics must match
for a in range(0x55, 0x5C): CAUSAL[a] = 2.0              # spawn work area
for a in range(0x400, 0x788): CAUSAL[a] = 3.0            # layout + entity tables


def buildBase(k):
    full = loadSol(os.path.join(HERE, "stage01.win721.full.sol"))
    return full[:720] + [NULL] * k + full[720:]


def refTargets():
    d = RamDump(os.path.join(WORKDIR, "..", "refchk.ram")) if False else None
    # use the scratchpad refchk dump
    sp = os.environ.get("ALIGN_SP")
    d = RamDump(os.path.join(sp, "refchk.ram"))
    ready, press = 1073, 1087
    tReady = {a: d.b(ready, a) for a in CAUSAL}
    tPress = {a: d.b(press, a) for a in CAUSAL}
    return tReady, tPress


def dist(d, f, target):
    s = 0.0
    for a, w in CAUSAL.items():
        if d.b(f, a) != target[a]: s += w
    return s


def stageA(task):
    k, sel, st, tReady = task
    tag = f"A{k}_{sel}_{st}"
    seq = buildBase(k)
    if sel <= len(seq): return (task[:3], None, None)
    seq += [NULL] * (sel - len(seq)) + [S]
    seq += [NULL] * (st - len(seq)) + [ST]
    sol = os.path.join(WORKDIR, tag + ".sol"); ram = os.path.join(WORKDIR, tag + ".ram")
    saveSol(seq + [NULL] * 400, sol)
    res = (task[:3], None, None)
    try:
        if replay(sol, ram, timeout=300):
            d = RamDump(ram)
            rdy = next((f for f in range(st, d.frames) if d.b(f, 0xDB) == 1 and d.b(f, 0xA6) == 2 and 0 < d.b(f, 0x93) < 200 and d.b(f, 0xE5) == 1), None)
            if rdy is not None: res = (task[:3], rdy, dist(d, rdy, tReady))
    finally:
        for p in (sol, ram):
            try: os.remove(p)
            except OSError: pass
    return res


def stageB(task):
    k, sel, st, press, tPress = task
    tag = f"B{k}_{sel}_{st}_{press}"
    seq = buildBase(k)
    seq += [NULL] * (sel - len(seq)) + [S]
    seq += [NULL] * (st - len(seq)) + [ST]
    seq += [NULL] * (press - len(seq)) + [LS]
    sol = os.path.join(WORKDIR, tag + ".sol"); ram = os.path.join(WORKDIR, tag + ".ram")
    saveSol(seq + [NULL] * 30, sol)
    res = (task[:4], None)
    try:
        if replay(sol, ram, timeout=300):
            d = RamDump(ram)
            f = min(press + 1, d.frames - 1)
            # play-started validity: gt must RUN and the scroll must be canceled (camX stable),
            # not swallowed (probeHold lesson: DB==1 alone proves nothing)
            e = min(press + 9, d.frames - 1)
            gtRuns = d.b(e, 0x9E) != d.b(f, 0x9E)
            camStable = d.b(e, 0x04) <= d.b(min(press + 3, d.frames - 1), 0x04) + 1
            if d.b(f, 0xDB) == 1 and d.b(f, 0xA6) == 2 and gtRuns and camStable: res = (task[:4], dist(d, f, tPress))
    finally:
        for p in (sol, ram):
            try: os.remove(p)
            except OSError: pass
    return res


def stageC(task):
    k, sel, st, press = task
    tag = f"C{k}_{sel}_{st}_{press}"
    seq = buildBase(k)
    seq += [NULL] * (sel - len(seq)) + [S]
    seq += [NULL] * (st - len(seq)) + [ST]
    seq += [NULL] * (press - len(seq)) + [LS]
    tail = loadSol(os.path.join(HERE, "stage02.reference.sol"))
    sol = os.path.join(WORKDIR, tag + ".sol"); ram = os.path.join(WORKDIR, tag + ".ram")
    saveSol(seq + tail + [NULL] * 40, sol)
    res = (task, -1, None)
    try:
        if replay(sol, ram, timeout=300):
            d = RamDump(ram)
            p0 = press + 1
            picks = max(d.b(f, 0x00C4) for f in range(p0, d.frames))
            g0 = next((f for f in range(p0, d.frames) if d.b(f, 0x93) == 0 and d.b(f, 0xDB) == 1 and d.b(f, 0xA6) == 2), None)
            flip = None
            if g0 is not None: flip = next((f for f in range(g0, d.frames) if d.b(f, 0xA6) == 3), None)
            res = (task, picks, flip)
    finally:
        for p in (sol, ram):
            try: os.remove(p)
            except OSError: pass
    return res


def main():
    topA = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    topB = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    os.makedirs(WORKDIR, exist_ok=True)
    tReady, tPress = refTargets()

    tasksA = []
    for k in range(0, 12):
        flip = 721 + (3 if k >= 2 else k)   # approximate; actual flip found in replay anyway
        for sel in range(726, 748):
            for st in range(sel + 5, sel + 26):
                tasksA.append((k, sel, st, tReady))
    import json as _json
    cacheP = os.path.join(os.environ.get("ALIGN_SP", "."), "alignA.cache.json")
    if os.path.isfile(cacheP):
        resA = [(s_, r_, tuple(k_)) for s_, r_, k_ in _json.load(open(cacheP))]
        print(f"stage A: {len(resA)} results from cache", flush=True)
    else:
        print(f"stage A: {len(tasksA)} candidates", flush=True)
        resA = []
        with mp.Pool(12) as pool:
            for i, (key, rdy, sc) in enumerate(pool.imap_unordered(stageA, tasksA, chunksize=8)):
                if sc is not None: resA.append((sc, rdy, key))
                if (i + 1) % 500 == 0: print(f"  A {i+1}/{len(tasksA)}, kept {len(resA)}", flush=True)
        resA.sort()
        _json.dump(resA, open(cacheP, "w"))
    print("best A:", [(round(s, 1), r, k) for s, r, k in resA[:8]], flush=True)

    # sanity anchor: the known-good manual candidate must flow through B and C
    anchor = next(((sc_, r_, k_) for sc_, r_, k_ in resA if tuple(k_) == (3, 734, 739)), None)
    picksA = resA[:topA] + ([anchor] if anchor and anchor not in resA[:topA] else [])
    tasksB = []
    for sc, rdy, (k, sel, st) in picksA:
        for press in range(rdy + 5, rdy + 26):
            tasksB.append((k, sel, st, press, tPress))
    print(f"stage B: {len(tasksB)} candidates", flush=True)
    resB = []
    with mp.Pool(12) as pool:
        for i, (key, sc) in enumerate(pool.imap_unordered(stageB, tasksB, chunksize=8)):
            if sc is not None: resB.append((sc, key))
            if (i + 1) % 200 == 0: print(f"  B {i+1}/{len(tasksB)}, kept {len(resB)}", flush=True)
    resB.sort()
    print("best B:", [(round(s, 1), k) for s, k in resB[:10]], flush=True)

    print(f"stage C: tail oracle on top {topB}", flush=True)
    bestC = None
    with mp.Pool(12) as pool:
        for key, picks, flip in pool.imap_unordered(stageC, [k for _, k in resB[:topB]], chunksize=1):
            print(f"  C {key}: picks={picks} flip={flip}", flush=True)
            if flip is not None and (bestC is None or flip < bestC[1]):
                bestC = (key, flip)
    if bestC:
        print(f"ALIGNMENT FOUND: levers {bestC[0]} -> reference tail flips at {bestC[1]}", flush=True)
    else:
        print("no full resync in stage C; best-pick depths above show closest candidates", flush=True)


if __name__ == "__main__":
    main()
