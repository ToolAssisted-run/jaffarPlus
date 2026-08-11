#!/usr/bin/env python3
"""Per-level frame-span report for a Lode Runner full-game movie.

Replays the movie from power-on with --dumpRam and reports, for each level found in the
movie's structure (levels are separated by >=100-frame null transition gaps):

  playStart   first input frame after the preceding gap (= the select scroll-skip press)
  skip        whether the select scroll-skip landed (resyncTool oracle)
  solved      gold reached 0 while genuinely playing the level (alive, play mode 1)
  gold0       frame where gold remaining first hit 0 during play
  lastIn      last non-null input frame within the level span (TAS-length metric)
  inc         frame where the level counter increments to k+1 (start of end transition)
  span        inc - playStart  (the per-level completion time to replicate/beat)
  st@inc      spawn timer $0053 at the increment frame (cross-level luck handoff)

Usage: levelReport.py <movie.sol> [maxLevels]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from resyncTool import NULL, PADDING, RAMSZ, RamDump, WORKDIR, loadSol, replay, saveSol, skipLanded


def movieBounds(lines):
    """[(k, playStart)] for each >=100-null gap -> following input block (k is 1-based)."""
    runs = []
    runStart, runLen = None, 0
    for i, line in enumerate(lines + ["X"]):
        if line == NULL:
            if runLen == 0:
                runStart = i
            runLen += 1
        else:
            if runLen >= 100:
                runs.append((runStart, i - 1))
            runLen = 0
    return [(k + 1, runs[k][1] + 1) for k in range(len(runs))]


def main():
    movie = sys.argv[1]
    maxLevels = int(sys.argv[2]) if len(sys.argv) > 2 else 50
    os.makedirs(WORKDIR, exist_ok=True)

    lines = loadSol(movie)
    bounds = movieBounds(lines)[:maxLevels]
    print(f"movie: {len(lines)} frames, {len(bounds)} level blocks detected", flush=True)

    ramPath = os.path.join(WORKDIR, "report.ram")
    solPath = os.path.join(WORKDIR, "report.sol")
    saveSol(lines + [NULL] * PADDING, solPath)
    if not replay(solPath, ramPath, timeout=600):
        print("FATAL: replay failed", flush=True)
        sys.exit(1)
    dump = RamDump(ramPath)

    print(f"{'lvl':>3} {'playStart':>9} {'skip':>4} {'solved':>6} {'gold0':>7} {'lastIn':>7} "
          f"{'inc':>7} {'span':>5} {'st@inc':>6}", flush=True)
    for idx, (k, playStart) in enumerate(bounds):
        nextStart = bounds[idx + 1][1] if idx + 1 < len(bounds) else dump.frames
        skip = skipLanded(dump, k, playStart)
        gold0 = inc = None
        for f in range(playStart, min(nextStart + 45, dump.frames)):
            if gold0 is None and dump.b(f, 0x00A6) == k and dump.b(f, 0x00DB) == 1 \
                    and dump.b(f, 0x0093) == 0 and dump.b(f, 0x009A) == 1:
                gold0 = f
            if dump.b(f, 0x00A6) == k + 1:
                inc = f
                break
        lastIn = None
        for f in range(min(nextStart, len(lines)) - 1, playStart - 1, -1):
            if lines[f] != NULL:
                lastIn = f
                break
        span = (inc - playStart) if inc is not None else None
        st = dump.b(inc, 0x0053) if inc is not None else None
        print(f"{k:>3} {playStart:>9} {'Y' if skip else 'MISS':>4} {'Y' if gold0 else 'NO':>6} "
              f"{gold0 if gold0 is not None else '-':>7} {lastIn if lastIn is not None else '-':>7} "
              f"{inc if inc is not None else '-':>7} {span if span is not None else '-':>5} "
              f"{st if st is not None else '-':>6}", flush=True)


if __name__ == "__main__":
    main()
