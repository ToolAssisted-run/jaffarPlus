#!/usr/bin/env python3
"""Post-ready hold-phase probe (stage B of the load-window investigation).

Baseline: ready 414, 13-frame input swallow (gt frozen), scroll auto-start ~427, skip press
428, play from 429. Question: can any insert during [414..427] shorten the swallow so the
scroll-cancel press lands earlier than 428 with play genuinely running?
"""
import os, sys, multiprocessing as mp

HERE = "/home/jaffar/jaffarPlus/examples/nes/lodeRunner"
sys.path.insert(0, HERE)
from resyncTool import NULL, RamDump, loadSol, saveSol, replay, WORKDIR

def mk(bits):
    s = list("........")
    for i, ch in zip(range(8), "UDLRSsBA"):
        if ch in bits: s[i] = ch
    return "|..|" + "".join(s) + "|"

PRESS = "|..|..L..s..|"
SEED  = loadSol(os.path.join(HERE, "stage01.initial.sol"))[:428]
KEYS  = ["U", "D", "L", "R", "s", "B", "A", "S", "AB"]


def probe(task):
    tag, hf, key, pf = task
    lines = list(SEED[:pf]) + [PRESS]
    if hf >= 0: lines[hf] = mk(key)
    sol = os.path.join(WORKDIR, tag + ".sol")
    ram = os.path.join(WORKDIR, tag + ".ram")
    saveSol(lines + [NULL] * 30, sol)
    ok = replay(sol, ram, timeout=300)
    res = None
    if ok:
        d = RamDump(ram)
        rdy = next((f for f in range(d.frames) if d.b(f, 0xDB) == 1 and d.b(f, 0xA6) == 1
                    and d.b(f, 0x93) == 6 and d.b(f, 0xE5) == 1), None)
        if rdy is not None:
            cam = [d.b(f, 0x0004) for f in range(pf, min(pf + 8, d.frames))]
            gtA = d.b(min(pf + 2, d.frames - 1), 0x009E)
            gtB = d.b(min(pf + 8, d.frames - 1), 0x009E)
            if d.b(min(pf + 8, d.frames - 1), 0xDB) == 1 and max(cam) <= 2 and gtB != gtA:
                res = pf
    for p in (sol, ram):
        try: os.remove(p)
        except OSError: pass
    return (tag, res)


def main():
    tasks = [(f"ctl.{pf}", -1, "", pf) for pf in range(420, 429)]
    for key in KEYS:
        for hf in range(414, 428):
            for pf in range(max(hf + 1, 420), 429):
                tasks.append((f"hp.{key}.{hf}.{pf}", hf, key, pf))
    print(f"{len(tasks)} probes", flush=True)
    hits = []
    with mp.Pool(12) as pool:
        for tag, pf in pool.imap_unordered(probe, tasks, chunksize=8):
            if pf is not None:
                hits.append((pf, tag))
                if pf < 428: print(f"HIT: {tag} -> play from press at {pf} (-{428 - pf})", flush=True)
    hits.sort()
    ctl = [t for p, t in hits if t.startswith("ctl.")]
    print("controls that played:", ctl, flush=True)
    real = [(p, t) for p, t in hits if not t.startswith("ctl.") and p < 428]
    print(f"done: {len(real)} genuine early-press hits", flush=True)
    for p, t in real[:20]: print(f"  {t}", flush=True)


if __name__ == "__main__":
    main()
