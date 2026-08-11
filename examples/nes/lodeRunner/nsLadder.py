#!/usr/bin/env python3
"""Backward certification ladder for the newStartPoint 522 line (pin-protected, tol0/grace0).

Per rung K: initial = seed[:K]; ref tail = the certified line's remainder; per-rung choreography
trim, rung-indexed trace + pin hashes regenerated in dependency-safe order; hash-only pinning.
Ties certify; a win below the tie point rebases (reported, not auto-rebased). Stops on failure.
No timeouts (directive). Usage: nsLadder.py <Kstart> <Kstep> <minK>
"""
import json, os, shutil, subprocess, sys, collections

HERE   = os.path.dirname(os.path.abspath(__file__))
JAFFAR = os.path.join(HERE, "../../../build-lodeRunner/jaffar")
PLAYER = os.path.join(HERE, "../../../build-lodeRunner/jaffar-player")
SEED   = [l.rstrip("\n") for l in open(f"{HERE}/newStartPoint.sol") if l.strip()]
# certified line = seed + the 60-line winning tail (522 total); ref for rung K = line[K:]
LINE   = SEED + [l.rstrip("\n") for l in open(f"{HERE}/stage50.newstart.win8.sol") if l.strip()]
PICKS  = [(345, (22, 12)), (403, (11, 7)), (434, (6, 2)), (502, (2, 3))]

def runK(K):
    ini, ref = f"{HERE}/stage50.nsT.initial.sol", f"{HERE}/stage50.nsT.ref.sol"
    open(ini, "w").write("\n".join(LINE[:K]) + "\n")
    open(ref, "w").write("\n".join(LINE[K:]) + "\n")
    tie = len(LINE) - K
    cfg = json.load(open(f"{HERE}/stage50.nsT.jaffar"), object_pairs_hook=collections.OrderedDict)
    cfg["Emulator Configuration"]["Initial Sequence File Path"] = "stage50.nsT.initial.sol"
    cfg["Driver Configuration"]["Reference Reward Floor"]["Solution File"] = "stage50.nsT.ref.sol"
    cfg["Driver Configuration"]["Max Steps"] = tie + 20
    cfg["Game Configuration"]["Reference Pickup Order"] = [[x, y] for f, (x, y) in PICKS if f > K]
    # generation pass config: no floor/pin/trace (avoids the file-order trap)
    gen = json.loads(json.dumps(cfg), object_pairs_hook=collections.OrderedDict)
    gen["Driver Configuration"]["Reference Reward Floor"]["Enabled"] = False
    gen["Engine Configuration"].pop("Reference Pinning", None)
    gen["Game Configuration"]["Trace Magnet Intensity"] = 0.0
    gen["Game Configuration"]["Trace File"] = ""
    open(f"{HERE}/nsThash.tmp.jaffar", "w").write(json.dumps(gen, indent=1) + "\n")
    ram = f"{HERE}/nsT.rungtrace.ram"
    subprocess.run([PLAYER, "nsThash.tmp.jaffar", "stage50.nsT.ref.sol", "--reproduce", "--disableRender",
                    "--unattended", "--exitOnEnd", "--dumpRam", ram, "--dumpHashes", "stage50.nsT.refhashes.txt",
                    "--dumpHashesLookahead", "2"],
                   cwd=HERE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    data = open(ram, "rb").read(); R = 2048
    with open(f"{HERE}/stage50.nsT.trace.txt", "w") as fp:
        for f in range(len(data) // R): fp.write(f"{data[f*R+0x20]} {data[f*R+0x21]}\n")
    os.remove(ram)
    open(f"{HERE}/stage50.nsT.jaffar", "w").write(json.dumps(cfg, indent=1) + "\n")
    log = f"{HERE}/stage50.nsT.k{K:03d}.log"
    try: os.remove("/tmp/jaffar.winsolution.sol")
    except OSError: pass
    subprocess.run([JAFFAR, "stage50.nsT.jaffar"], cwd=HERE, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    exitLine = None; win = False
    for l in open(log, errors="replace"):
        if "Exit Reason" in l: exitLine = l.strip()
        if "Solution found" in l: win = True
    tail = None
    if win and os.path.isfile("/tmp/jaffar.winsolution.sol"):
        shutil.copy("/tmp/jaffar.winsolution.sol", f"{HERE}/stage50.nsT.k{K:03d}.win.sol")
        tail = sum(1 for l in open(f"{HERE}/stage50.nsT.k{K:03d}.win.sol") if l.strip())
    return exitLine, win, tail, tie

if __name__ == "__main__":
    K0, step, minK = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    K = K0
    while K >= minK:
        exitLine, win, tail, tie = runK(K)
        verdict = "BEAT" if (win and tail < tie) else ("TIE-certified" if win else "FAILED")
        print(f"K={K}: {verdict} (win={win} tail={tail} tie={tie}) [{exitLine}]", flush=True)
        if not win or (tail is not None and tail < tie): break
        K -= step
    print("ladder done", flush=True)
