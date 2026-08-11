#!/usr/bin/env python3
"""Segmented endgame (user plan, 2026-08-10): section A wins on banking the last gold; its
winning tail is appended to the seed, then section B searches the exit climb to the level wrap.
No timeouts (directive)."""
import json, os, shutil, subprocess, collections

HERE = os.path.dirname(os.path.abspath(__file__))
JAFFAR = os.path.join(HERE, "../../../build-lodeRunner/jaffar")

def run(cfg, log):
    subprocess.run([JAFFAR, cfg], cwd=HERE, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    exitLine = None; win = False
    for l in open(log, errors="replace"):
        if "Exit Reason" in l: exitLine = l.strip()
        if "Solution found" in l: win = True
    return exitLine, win

seed = [l.rstrip("\n") for l in open(f"{HERE}/newStartPoint.sol") if l.strip()]

exitLine, win = run("stage50.secA.jaffar", f"{HERE}/stage50.secA.log")
print(f"SECTION A: {exitLine} win={win}", flush=True)
if not win: raise SystemExit(1)
shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.secA.win.sol")
aw = [l.rstrip("\n") for l in open(f"{HERE}/stage50.secA.win.sol") if l.strip()]
open(f"{HERE}/stage50.secB.initial.sol", "w").write("\n".join(seed + aw) + "\n")
print(f"SECTION A WIN: {len(aw)} steps (last gold banked at {len(seed)+len(aw)})", flush=True)

cfg = json.load(open(f"{HERE}/stage50.secA.jaffar"), object_pairs_hook=collections.OrderedDict)
cfg["Emulator Configuration"]["Initial Sequence File Path"] = "stage50.secB.initial.sol"
cfg["Driver Configuration"]["Max Steps"] = 40
for r in cfg["Game Configuration"]["Rules"]:
    if r.get("Label") == 1999:
        r["Description"] = "WIN (section B): level wrap"
        r["Conditions"] = [{"Property": "Current Level", "Op": "==", "Value": 1}]
cfg["Game Configuration"]["Reference Pickup Order"] = []
open(f"{HERE}/stage50.secB.jaffar", "w").write(json.dumps(cfg, indent=1) + "\n")

exitLine, win = run("stage50.secB.jaffar", f"{HERE}/stage50.secB.log")
print(f"SECTION B: {exitLine} win={win}", flush=True)
if win:
    shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.secB.win.sol")
    bw = [l.rstrip("\n") for l in open(f"{HERE}/stage50.secB.win.sol") if l.strip()]
    total = len(seed) + len(aw) + len(bw)
    print(f"SECTION B WIN: {len(bw)} steps -> TOTAL {total} (baseline 521)", flush=True)
