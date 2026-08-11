#!/usr/bin/env python3
"""Two-phase endgame experiment (user plan, 2026-08-09).

Phase 1: from ref[:263] under the strict regime (grace 0, prune tol 0, 30GB), win when both
piles ((26,10),(23,10)) are banked AND the (23,11) enemy-freeing dig is underway.
Phase 2: append the phase-1 winning tail to the seed and free-search from there (30GB, NO
reference machinery: floor/prune/choreo off), win = level flip. No timeouts (directive).
"""
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

# ---- phase 1
exitLine, win = run("stage50.dig2.jaffar", f"{HERE}/stage50.dig2.log")
print(f"PHASE1: {exitLine} win={win}", flush=True)
if not win:
    print("phase1 FAILED -- stopping", flush=True)
    raise SystemExit(1)
shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.dig2.win.sol")
winLines = [l.rstrip("\n") for l in open(f"{HERE}/stage50.dig2.win.sol") if l.strip()]
seed = [l.rstrip("\n") for l in open(f"{HERE}/stage50.dig2.initial.sol") if l.strip()]
open(f"{HERE}/stage50.free.initial.sol", "w").write("\n".join(seed + winLines) + "\n")
print(f"PHASE1 WIN: {len(winLines)} steps; phase2 seed = {len(seed)+len(winLines)} frames", flush=True)

# ---- phase 2 config: free search, no reference machinery
cfg = json.load(open(f"{HERE}/stage50.dig2.jaffar"), object_pairs_hook=collections.OrderedDict)
cfg["Emulator Configuration"]["Initial Sequence File Path"] = "stage50.free.initial.sol"
cfg["Engine Configuration"]["Reference Reward Prune"]["Enabled"] = False
cfg["Driver Configuration"]["Reference Reward Floor"]["Enabled"] = False
cfg["Driver Configuration"]["Max Steps"] = 513 - (len(seed) + len(winLines)) + 40
gc = cfg["Game Configuration"]
gc["Reference Pickup Order"] = []
for r in gc["Rules"]:
    if r.get("Label") == 1999:
        r["Description"] = "WIN: stage50 complete (level counter wraps to 1)"
        r["Conditions"] = [{"Property": "Current Level", "Op": "==", "Value": 1}]
open(f"{HERE}/stage50.free.jaffar", "w").write(json.dumps(cfg, indent=1) + "\n")

exitLine, win = run("stage50.free.jaffar", f"{HERE}/stage50.free.log")
print(f"PHASE2: {exitLine} win={win}", flush=True)
if win:
    shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.free.win.sol")
    n = sum(1 for l in open(f"{HERE}/stage50.free.win.sol") if l.strip())
    print(f"PHASE2 WIN: {n} steps -> TOTAL {len(seed)+len(winLines)+n} frames (ref 513)", flush=True)
