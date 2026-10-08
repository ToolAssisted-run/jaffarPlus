#!/usr/bin/env python3
"""mvextend.py PREV.mov LEVEL.sol NEXT OUT.mov [AFTER...]

The run's Chimera movie one level further. PREV.mov ends on the row before the level's first tick (its row count is
the level's first row, rows counted from 0); LEVEL.sol is the level as the run plays it (tools/cutlevel.py), a row a
line; AFTER are the rows pressed once it has ended (the runner's letters; default "_", Space, which skips a story
scene; the copy protection after level 2 is "_ R . S"). Empty rows follow until level NEXT's first tick, found with
the core's runner (env RUNNER, default ../bin/run-native-a5ee, run from its game directory, e.g. ~/pop2render/l8).
Prints the level's first row and IGT there; OUT.mov ends on the row before it.

The runner's letters: U D L R S C, `_` Show Time / Space, `a` Restart Level, `m` Music On/Off, `.` nothing.
"""
import os, subprocess, sys, tempfile

RUN = os.environ.get("RUNNER", "../bin/run-native-a5ee")


def letters(s):
    return ({"R": "a", "M": "m", "K": "_"}.get(s[1], "") + "".join(ch for ch, i in zip("UDLRSC", [5, 6, 3, 4, 7, 8]) if s[i] != ".")) or "."


def first_rows(rows):
    with tempfile.TemporaryDirectory() as d:
        mov, tr = os.path.join(d, "m.mov"), os.path.join(d, "t.txt")
        open(mov, "w").write("\n".join(rows) + "\n")
        subprocess.run([RUN, ".", "--movie", mov, "--frames", str(len(rows)), "--trace", tr, "--trace-props",
                        "Level,Kid.Room,Kid.X,Kid.Frame,Random Seed,Tick,IGT Ticks"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out = {}; prev = None
        for l in open(tr):
            f = l.split()
            if not f[0].isdigit(): continue
            if f[3] != prev: out[int(f[3])] = (int(f[0]), int(float(f[9]))); prev = f[3]   # row, IGT ticks
    return out


def main():
    prev, level, nxt, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    after = sys.argv[5:] or ["_"]
    rows = [l.strip() for l in open(prev) if l.strip()]
    start = len(rows)
    rows += [letters(l.strip()) for l in open(level) if l.strip().startswith("|")] + after
    r = first_rows(rows + ["."] * 400)
    if nxt not in r: sys.exit("level %d not reached: %s" % (nxt, r))
    row, igt = r[nxt]
    if row < len(rows): sys.exit("level %d starts on row %d, before the rows end (%d)" % (nxt, row, len(rows)))
    rows += ["."] * (row - len(rows))
    open(out, "w").write("\n".join(rows) + "\n")
    print("level %d: rows %d..%d; level %d's first row %d (IGT %d); %s: %d rows" % (nxt - 1, start, row - 1, nxt, row, igt, out, len(rows)))


if __name__ == "__main__":
    main()
