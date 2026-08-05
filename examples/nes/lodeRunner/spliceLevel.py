#!/usr/bin/env python3
"""Splice a jaffar-solved level back into the resync candidate.

Usage: spliceLevel.py <level> <seedPrefix.sol> <winSol.sol> <out.sol>

  <level>       the level number that was re-solved (e.g. 9)
  <seedPrefix>  the initial sequence the jaffar search was seeded with (e.g. stage09.initial.sol);
                the win solution's inputs continue from its end
  <winSol>      jaffar's winning solution (inputs after the seed, ends when the level counter
                increments a few frames after climbing off the top)
  <out>         output candidate movie

The splice appends the reference movie's score-screen handling (a few nulls, Select, a few nulls,
Start -- sloppy timing is fine, the next level's scroll-skip delay search absorbs it) and then the
reference remainder starting from the transition gap after the spliced level. It also writes
<out>.editlog.json so resyncTool.py resumes with correct boundary offsets for later levels.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NULL = "|..|........|"
SELECT = "|..|.....s..|"
START = "|..|....S...|"

sys.path.insert(0, HERE)
from resyncTool import loadSol, saveSol, levelBounds, REFERENCE  # noqa: E402


def main():
    level = int(sys.argv[1])
    seed = loadSol(sys.argv[2])
    win = loadSol(sys.argv[3])
    outPath = sys.argv[4]

    ref = loadSol(REFERENCE)
    bounds = levelBounds(ref)
    kIdx = level - 1
    playEnd = bounds[kIdx][2]           # ref frame of the level's last input (the Start press)
    gapStart = playEnd + 1              # ref frame where the transition jingle begins

    # The win solution may already contain the seed as its prefix (jaffar solutions include the
    # initial sequence); use whichever interpretation matches.
    if len(win) > len(seed) and win[: len(seed)] == seed:
        solved = win
    else:
        solved = seed + win

    tail = [NULL] * 4 + [SELECT] + [NULL] * 4 + [START]
    candidate = solved + tail + ref[gapStart:]

    # Boundary shift for every level after the spliced one
    newPlayEnd = len(solved) + len(tail) - 1
    shift = newPlayEnd - playEnd

    # Merge with any existing edit log (the prefix edits that got us to this level)
    priorLogPath = os.path.join(HERE, "fullGameQuickerNES.sol.editlog.json")
    editLog = [tuple(e) for e in json.load(open(priorLogPath))] if os.path.isfile(priorLogPath) else []
    # Drop prior edits at/after this level's span (they are superseded by the splice)
    editLog = [e for e in editLog if e[0] < bounds[kIdx][1]]
    editLog.append((playEnd, shift))

    saveSol(candidate, outPath)
    json.dump(editLog, open(outPath + ".editlog.json", "w"))
    print(f"spliced level {level}: solved block {len(solved)} frames (ref play end {playEnd} -> "
          f"{newPlayEnd}, shift {shift:+d}); candidate {len(candidate)} frames")
    print(f"edit log ({len(editLog)} entries) -> {outPath}.editlog.json")


if __name__ == "__main__":
    main()
