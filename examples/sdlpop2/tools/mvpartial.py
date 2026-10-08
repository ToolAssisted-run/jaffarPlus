#!/usr/bin/env python3
"""mvpartial.py PREV.mov PARTIAL.sol OUT.mov [TAIL]

The run's Chimera movie plus an unfinished level: PARTIAL.sol is a running search's best path so far (its
/tmp/jaffar.*.best.sol, copied: the search rewrites it), a row a line after PREV.mov's rows (which end on the row
before the level's first tick), and TAIL empty rows after (default 30). For a video of a search's progress
(render-run.sh NAME OUT.mov FIRSTROW); the level's own movie comes from mvextend.py once it is solved.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mvextend import letters


def main():
    prev, sol, out = sys.argv[1:4]
    tail = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    rows = [l.strip() for l in open(prev) if l.strip()]
    start = len(rows)
    level = [letters(l.strip()) for l in open(sol) if l.startswith("|")]
    open(out, "w").write("\n".join(rows + level + ["."] * tail) + "\n")
    print("first row %d, %d lines, %s: %d rows" % (start, len(level), out, start + len(level) + tail))


if __name__ == "__main__":
    main()
