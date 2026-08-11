#!/usr/bin/env python3
"""Advanced-start K-ladder for stage50 on the transplant state.

Seed = stage50.transplant.state + reference tail[:K] as the initial sequence; floor = tail[K:]
at tol 0 / grace 0. BRIDGE_TO semantics as in segLadder: a rung succeeds when it reaches the
previous rung's start (Max Steps = bridgeTo－K + 8) without a floor cancel.

Usage: stage50Ladder.py <Kstart> <Kstep> <minK> [bridgeToForFirstRung]
Env: SEGDB (Mb, default 30000)
"""
import json, os, subprocess, sys, collections, shutil

PLAYER = None  # set below

HERE   = os.path.dirname(os.path.abspath(__file__))
JAFFAR = os.path.join(HERE, "../../../build-lodeRunner/jaffar")
PLAYER = os.path.join(HERE, "../../../build-lodeRunner/jaffar-player")
REF    = [l.rstrip("\n") for l in open(f"{HERE}/stage50.reference.sol") if l.strip()]
WINSTEP = 527
# reference pickup rel-frames + positions (from the verified transplant replay)
PICKS = [(14,(0,12)),(81,(9,7)),(125,(12,3)),(135,(15,3)),(203,(25,2)),(209,(26,2)),(235,(27,2)),
         (257,(26,7)),(291,(26,10)),(300,(23,10)),(365,(22,12)),(413,(11,7)),(440,(6,2)),(497,(2,3))]


def runK(K, bridgeTo, dbMb):
    tag = f"s50.{K:03d}"
    ini = f"{HERE}/stage50.{tag}.initial.sol"
    ref = f"{HERE}/stage50.{tag}.reference.sol"
    cfgP = f"{HERE}/stage50.{tag}.jaffar"
    open(ini, "w").write("\n".join(REF[:K]) + "\n" if K > 0 else "")
    open(ref, "w").write("\n".join(REF[K:]) + "\n")
    cfg = json.load(open(f"{HERE}/stage50.jaffar"), object_pairs_hook=collections.OrderedDict)
    cfg["Engine Configuration"]["State Database"]["Max Size (Mb)"] = dbMb
    # Seed-aware choreography: the initial-sequence replay does not advance the tracker, so the
    # order list must contain only the pickups REMAINING after the seed's K inputs.
    remaining = [(x, y) for f, (x, y) in PICKS if f > K]
    gc = cfg["Game Configuration"]
    gc["Reference Pickup Order"] = [[x, y] for x, y in remaining]
    gc["Rules"] = [r for r in gc["Rules"] if not (2010 <= r["Label"] <= 2039)]
    cfg["Emulator Configuration"]["Initial Sequence File Path"] = os.path.basename(ini) if K > 0 else ""
    cfg["Driver Configuration"]["Reference Reward Floor"]["Solution File"] = os.path.basename(ref)
    cfg["Driver Configuration"]["Max Steps"] = (bridgeTo - K + 8) if bridgeTo > K else (len(REF) - K + 20)
    # Per-rung trace: replay the rung reference once with RAM dumping and record the player's
    # (x, y) per step -- the step-indexed trace magnet follows the reference's actual route.
    traceP = f"{HERE}/stage50.{tag}.trace.txt"
    if not os.path.isfile(traceP):
        tmpCfg = json.loads(json.dumps(cfg))
        tmpCfg["Driver Configuration"]["Reference Reward Floor"]["Enabled"] = False
        tmpP = f"{HERE}/stage50.{tag}.tmptrace.jaffar"
        open(tmpP, "w").write(json.dumps(tmpCfg, indent=1) + "\n")
        ramP = f"{HERE}/stage50.{tag}.trace.ram"
        subprocess.run([PLAYER, os.path.basename(tmpP), os.path.basename(ref), "--reproduce", "--disableRender",
                        "--unattended", "--exitOnEnd", "--dumpRam", ramP], cwd=HERE,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
        ram = open(ramP, "rb").read()
        R = 2048
        n = len(ram) // R
        with open(traceP, "w") as fp:
            for f in range(n):
                fp.write(f"{ram[f*R+0x20]} {ram[f*R+0x21]}\n")
        os.remove(ramP); os.remove(tmpP)
    gc["Trace Magnet Intensity"] = 10.0
    gc["Trace File"] = os.path.basename(traceP)
    open(cfgP, "w").write(json.dumps(cfg, indent=1) + "\n")
    log = f"{HERE}/stage50.{tag}.{dbMb}.log"
    for lnk in ("/tmp/jaffar.log", "/tmp/claude.log"):
        try:
            if os.path.islink(lnk) or os.path.exists(lnk): os.remove(lnk)
            os.symlink(log, lnk)
        except OSError: pass
    subprocess.run([JAFFAR, os.path.basename(cfgP)], cwd=HERE, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    exitLine = None; win = False; bridged = False
    for l in open(log, errors="replace"):
        if "Exit Reason" in l: exitLine = l.strip()
        if "Solution found" in l: win = True
        if "Maximum step count reached" in l: bridged = True
    if win and os.path.isfile("/tmp/jaffar.winsolution.sol"):
        shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.{tag}.win.sol")
    return exitLine, win, bridged


if __name__ == "__main__":
    K0, step, minK = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    bridgeTo = int(sys.argv[4]) if len(sys.argv) > 4 else -1
    dbMb = int(os.environ.get("SEGDB", "30000"))
    K = K0
    prevStart = bridgeTo if bridgeTo > 0 else -1
    while K >= minK:
        target = prevStart if prevStart > K else -1
        ok = False
        for db in (5000, 30000, 260000):
            exitLine, win, bridged = runK(K, target, db)
            ok = win or (target > 0 and bridged)
            print(f"K={K} @{db}Mb: {exitLine}  win={win} bridged={bridged} -> {'OK' if ok else 'FAILURE'}", flush=True)
            if ok: break
        if not ok:
            print(f"FAILURE at K={K} (5/30/260GB all failed) -- reward/hash/alphabet lead", flush=True)
            break
        prevStart = K
        K -= step
    print("ladder done", flush=True)
