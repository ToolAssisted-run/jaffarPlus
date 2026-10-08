#!/usr/bin/env python3
"""rewardcheck.py SCRIPT.jaffar SOLUTION.sol [MINDEPTH]

A script's reward along a known solution, to find where its magnets point the wrong way before a wide search does:
the solution is replayed through the script (jaffar-player --dumpReward; env PLAYER, default the tree's
build/jaffar-player) and every dip of the reward below its running maximum is listed (its first step, length and
depth; MINDEPTH, default 15, hides the small ones).

A search keeps the states of highest reward. Where the real way costs reward for a while (a magnet pulling off it),
a small database still carries those states along; a wide one fills with the states that stayed behind and drops
them (level 4 at 300 GB: 300 steps in room 26's alcove for a dip of 41 over 30 ticks). The solution need not be the
one the search will find, only one that goes the script's way; run it from the script's directory.
"""
import os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = os.environ.get("PLAYER", os.path.join(HERE, "../../../build/jaffar-player"))


def main():
    script, sol = sys.argv[1], sys.argv[2]
    mind = float(sys.argv[3]) if len(sys.argv) > 3 else 15
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "r.txt")
        cmd = "%s %s %s --unattended --disableRender --exitOnEnd --dumpReward %s" % (PLAYER, script, sol, out)
        subprocess.run(["script", "-qc", cmd, "/dev/null"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=dict(os.environ, TERM="xterm"))
        r = [float(l) for l in open(out) if l.strip()]
    print("%d steps, reward %.0f -> %.0f" % (len(r) - 1, r[0], r[-1]))
    top = r[0]; start = None; low = 0; n = 0
    for i, v in enumerate(r + [float("inf")]):
        if v >= top:
            if start is not None and top - low >= mind:
                print("  dip from step %d: %d steps, %.0f deep (%.0f -> %.0f)" % (start, i - start, top - low, top, low)); n += 1
            top = v; start = None
        else:
            if start is None: start, low = i, v
            low = min(low, v)
    print("%d dips of %g or more" % (n, mind))


if __name__ == "__main__":
    main()
