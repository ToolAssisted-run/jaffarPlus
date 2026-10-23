#!/usr/bin/env python3
"""A movie of chimera-core-open4dboxing's gate (waterbox/tests/plan2movie.py: a line a step, the mnemonics held,
"@X,Y" where the mouse moves; its first line a comment) as Chimera rows ("|  160,  100,<57 buttons>|"), the
JaffarPlus inputs of the QuickerOpen4DBoxing emulator.

  gate2rows.py MOVIE OUT [FIRST LAST]   (rows FIRST..LAST-1 only: the default all)
"""
import sys

MNEMONICS = "<>UDLRQEZCKON_BXT^!%&abcdefghijklmnopqrstuvwxyz0123456789"


def main():
    lines = open(sys.argv[1]).read().split("\n")
    if lines and lines[0].startswith("#"):
        lines = lines[1:]
    if lines and lines[-1] == "":
        lines = lines[:-1]
    x, y = 160, 100
    rows = []
    for line in lines:
        if "@" in line:
            line, at = line.split("@")
            x, y = (int(v) for v in at.split(","))
        held = set(line)
        rows.append("|%5d,%5d," % (x, y) + "".join(m if m in held else "." for m in MNEMONICS) + "|")
    first, last = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, len(rows))
    open(sys.argv[2], "w").write("\n".join(rows[first:last]) + "\n")


if __name__ == "__main__":
    main()
