#!/usr/bin/env python3
"""Full-level exploratory run at floor tol=1000, escalating DB: 5GB -> 10GB -> 300GB.
Waits for the K=0 certification rung (PID in WAIT_PID) to finish and verify BRIDGED first.
Failure (floor cancel) escalates to the next DB size; a win or any other exit stops the chain."""
import json, os, subprocess, sys, time, collections, shutil

HERE   = os.path.dirname(os.path.abspath(__file__))
JAFFAR = os.path.join(HERE, "../../../build-lodeRunner/jaffar")

waitPid   = int(os.environ.get("WAIT_PID", "0"))
ladderLog = os.environ.get("LADDER_LOG", "")

def pidAlive(pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False

if waitPid > 0:
    while pidAlive(waitPid):
        time.sleep(30)
    if ladderLog and "BRIDGED" not in open(ladderLog, errors="replace").read():
        print("K=0 rung did NOT bridge -- full run not launched")
        sys.exit(1)
    print("K=0 rung BRIDGED -- starting escalation")

REF = [l.rstrip("\n") for l in open(f"{HERE}/stage01.reference.current.sol") if l.strip()]

for dbMb in (5000, 10000, 300000):
    tag  = f"full.tol1000.{dbMb}"
    cfgP = f"{HERE}/stage01.{tag}.jaffar"
    log  = f"{HERE}/stage01.{tag}.log"
    cfg  = json.load(open(f"{HERE}/stage01.jaffar"), object_pairs_hook=collections.OrderedDict)
    cfg["Engine Configuration"]["State Database"]["Max Size (Mb)"] = dbMb
    cfg["Emulator Configuration"]["Initial Sequence File Path"] = "stage01.initial.sol"
    cfg["Driver Configuration"]["Reference Reward Floor"]["Solution File"] = "stage01.reference.current.sol"
    cfg["Driver Configuration"]["Reference Reward Floor"]["Tolerance"] = 1000.0
    cfg["Driver Configuration"]["Max Steps"] = len(REF) + 40
    open(cfgP, "w").write(json.dumps(cfg, indent=1) + "\n")
    for lnk in ("/tmp/jaffar.log", "/tmp/claude.log"):
        try:
            if os.path.islink(lnk) or os.path.exists(lnk): os.remove(lnk)
            os.symlink(log, lnk)
        except OSError: pass
    print(f"=== attempt db={dbMb}Mb ===", flush=True)
    subprocess.run([JAFFAR, os.path.basename(cfgP)], cwd=HERE, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    exitLine = winStep = None
    for l in open(log, errors="replace"):
        if "Exit Reason" in l: exitLine = l.strip()
        if "Solution found" in l: winStep = int(l.split("Step")[1].split("-")[0].strip())
    print(f"db={dbMb}: {exitLine} winStep={winStep}", flush=True)
    if winStep is not None:
        if os.path.isfile("/tmp/jaffar.winsolution.sol"):
            shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage01.{tag}.win.sol")
            print(f"WIN preserved: stage01.{tag}.win.sol", flush=True)
        break
    floorFail = exitLine is not None and ("below the reference" in exitLine or "below the worst" in exitLine)
    if not floorFail:
        print("non-floor exit -- stopping escalation", flush=True)
        break
print("escalation chain done", flush=True)
