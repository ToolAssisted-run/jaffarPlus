#!/usr/bin/env python3
"""mkmovie.py OUT.mov PART...: the run's Chimera movie (one row a frame in the core runner's letters) from its parts:
  boot:N          N rows with nothing pressed (the MT-32's start-up and the title, up to level 1's first row)
  FILE.sol        a level's inputs, one row a line: U D L R, S Shift, C Ctrl; the key column K -> _ (Show Time / Space),
                  R -> a (Restart Level), M -> m (Music On/Off)
  rows:A,B,...    rows given as letters ("." nothing): the Space that skips a story scene, the copy protection's
                  answer, the rows a level takes to load
Prints each part's first row."""
import sys
out, parts = sys.argv[1], sys.argv[2:]
def letters(s):
    return ({"R": "a", "M": "m", "K": "_"}.get(s[1], "") + "".join(ch for ch, i in zip("UDLRSC", [5, 6, 3, 4, 7, 8]) if s[i] != ".")) or "."
rows = []
for p in parts:
    first = len(rows)
    if p.startswith("boot:"): rows += ["."] * int(p[5:])
    elif p.startswith("rows:"): rows += p[5:].split(",")
    else: rows += [letters(l.strip()) for l in open(p) if l.strip()]
    print("%6d  %s" % (first, p if len(p) < 60 else p[:57] + "..."))
open(out, "w").write("\n".join(rows) + "\n")
print("%6d  rows" % len(rows))
