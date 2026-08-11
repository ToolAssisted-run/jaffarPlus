#!/usr/bin/env python3
"""Advanced-start ladder (stage01). Two modes:
  replicate: floor tol 0 vs the given reference -- failure isolates a reward/hash/alphabet lead.
  beat:      floor tol 1e6 (cancel disarmed, frame-cutoff armed) -- grow improvements from deeper starts.
Usage: segLadder.py <Kstart> <Kstep> <minK> <refFile> <tolerance>"""
import json, os, subprocess, sys, collections, shutil

HERE   = os.path.dirname(os.path.abspath(__file__))
JAFFAR = os.path.join(HERE, "../../../build-lodeRunner/jaffar")
SEED   = [l.rstrip("\n") for l in open(f"{HERE}/stage01.initial.sol") if l.strip()]

def runK(K, REF, refName, tol, dbMb=int(__import__("os").environ.get("SEGDB","4000"))):
    tag = f"seg{refName}.{K:03d}"
    ini = f"{HERE}/stage01.{tag}.initial.sol"
    ref = f"{HERE}/stage01.{tag}.reference.sol"
    cfgP= f"{HERE}/stage01.{tag}.jaffar"
    open(ini,"w").write("\n".join(SEED + REF[:K])+"\n")
    open(ref,"w").write("\n".join(REF[K:])+"\n")
    cfg = json.load(open(f"{HERE}/stage01.jaffar"), object_pairs_hook=collections.OrderedDict)
    cfg["Engine Configuration"]["State Database"]["Max Size (Mb)"] = dbMb
    cfg["Emulator Configuration"]["Initial Sequence File Path"] = os.path.basename(ini)
    cfg["Driver Configuration"]["Reference Reward Floor"]["Solution File"] = os.path.basename(ref)
    cfg["Driver Configuration"]["Reference Reward Floor"]["Tolerance"] = tol
    bridgeTo = int(os.environ.get("BRIDGE_TO","-1"))
    cfg["Driver Configuration"]["Max Steps"] = (bridgeTo - K + 8) if bridgeTo > K else (len(REF) - K + 40)
    open(cfgP,"w").write(json.dumps(cfg, indent=1)+"\n")
    log = f"{HERE}/stage01.{tag}.log"
    for lnk in ("/tmp/jaffar.log", "/tmp/claude.log"):
        try:
            if os.path.islink(lnk) or os.path.exists(lnk): os.remove(lnk)
            os.symlink(log, lnk)
        except OSError: pass
    subprocess.run([JAFFAR, os.path.basename(cfgP)], cwd=HERE,
                   stdout=open(log,"w"), stderr=subprocess.STDOUT)
    exitLine = winStep = None
    bridged = False
    for l in open(log, errors="replace"):
        if "Exit Reason" in l: exitLine = l.strip()
        if "Solution found" in l: winStep = int(l.split("Step")[1].split("-")[0].strip())
        if "Maximum step count reached" in l: bridged = True
    if winStep is not None and os.path.isfile("/tmp/jaffar.winsolution.sol"):
        shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage01.{tag}.win.sol")
    return exitLine, winStep, bridged

if __name__ == "__main__":
    K0, step, minK = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    refFile, tol = sys.argv[4], float(sys.argv[5])
    REF = [l.rstrip("\n") for l in open(f"{HERE}/{refFile}") if l.strip()]
    refName = refFile.replace("stage01.reference","r").replace(".sol","")
    K = K0
    bridgeTo = int(sys.argv[6]) if len(sys.argv) > 6 else -1
    while K >= minK:
        os.environ["BRIDGE_TO"] = str(bridgeTo)
        exitLine, winStep, bridged = runK(K, REF, refName, tol)
        flip = 429 + K + winStep if winStep is not None else None
        refFlip = 429 + len(REF)
        print(f"K={K}: {exitLine}  winStep={winStep} absFlip~{flip} (current ref flip {refFlip})", flush=True)
        if bridged:
            print(f"K={K}: BRIDGED to {bridgeTo} at tol 0 -- certified by composition", flush=True)
        if winStep is None and not bridged and tol == 0.0:
            print(f"FAILURE at K={K} -- reward/hash/alphabet lead", flush=True)
            break
        bridgeTo = K
        if flip is not None and flip < refFlip:
            winLines = [l.rstrip("\n") for l in open(f"{HERE}/stage01.seg{refName}.{K:03d}.win.sol") if l.strip()]
            REF = REF[:K] + winLines[:winStep]
            open(f"{HERE}/stage01.reference.current.sol","w").write("\n".join(REF)+"\n")
            print(f"REBASED: new reference flip {429+len(REF)} (improvement {refFlip-flip})", flush=True)
        K -= step
    print("ladder done", flush=True)
