#!/usr/bin/env python3
"""T-back campaign: seed the reference minus its last BACK frames, search for a shorter finish.

Reuses the certified ladder machinery (per-rung choreo trimming, trace magnet, tol0/grace0
floor). Win-oriented: no bridge target, Max Steps = BACK + 20. On a win the solution lands in
stage50.s50.<K>.win.sol -- splice + rebase is a separate manual step.

Usage: tback.py <backFrames> <dbMb>
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage50Ladder as L

back, dbMb = int(sys.argv[1]), int(sys.argv[2])
K = len(L.REF) - back
print(f"T-{back}: K={K} of {len(L.REF)} @{dbMb}Mb", flush=True)
exitLine, win, bridged = L.runK(K, -1, dbMb)
print(f"T-{back} (K={K}) @{dbMb}Mb: {exitLine}  win={win} -> {'WIN' if win else 'FAILURE'}", flush=True)
