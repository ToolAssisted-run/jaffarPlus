#!/usr/bin/env python3
"""mvextend.py PREV.mov LEVEL.sol NEXT OUT.mov [AFTER...]

The run's Chimera movie one level further. PREV.mov ends on the row before the level's first tick (its row count is
the level's first row, rows counted from 0); LEVEL.sol is the level as the run plays it (tools/cutlevel.py), a row a
line; AFTER are the rows pressed once it has ended (the runner's letters; default "_", Space, which skips a story
scene; the copy protection after level 2 is "_ R . S"). Empty rows follow until level NEXT's first tick, found with
the core's runner (env RUNNER, default ../bin/run-native-a5ee, run from its game directory, e.g. ~/pop2render/l8).
Prints the level's first row and IGT there; OUT.mov ends on the row before it.

env CORE: the search core's trace of the level (trs GAMEDIR SEED 0 0 CHAIN.sol LEVEL.sol > CORE): the runner's room,
x, frame and random seed (the trace prints 9 digits of it) are compared with it on every tick of the level (the
core's tick on its line's row), which is the check that Chimera's core plays the level as the search did.

Without AFTER given, Space is tried first and nothing if the next level then starts on Space's row (no story scene
after the level).

A scene inside the level (level 8's sword scene, level 14's Jaffar scene: the search core plays none, in Chimera it
is skipped by Space and takes rows) shows as the runner's tick count standing still on one of the level's rows:
Space and empty rows are put in there, and of the few ways to lay them the one is taken that plays the rest of the
level as the core does (CORE is needed for it).

The runner's letters: U D L R S C, `_` Show Time / Space, `a` Restart Level, `m` Music On/Off, `.` nothing.
"""
import os, subprocess, sys, tempfile

RUN = os.environ.get("RUNNER", "../bin/run-native-a5ee")


def letters(s):
    return ({"R": "a", "M": "m", "K": "_"}.get(s[1], "") + "".join(ch for ch, i in zip("UDLRSC", [5, 6, 3, 4, 7, 8]) if s[i] != ".")) or "."


def play(rows):
    """the runner's trace, per row: (level, room, x, frame, tick count, IGT ticks, seed), each after the row's frame"""
    with tempfile.TemporaryDirectory() as d:
        mov, tr = os.path.join(d, "m.mov"), os.path.join(d, "t.txt")
        open(mov, "w").write("\n".join(rows) + "\n")
        subprocess.run([RUN, ".", "--movie", mov, "--frames", str(len(rows)), "--trace", tr, "--trace-props",
                        "Level,Kid.Room,Kid.X,Kid.Frame,Random Seed,Tick,IGT Ticks"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out = {}
        for l in open(tr):
            f = l.split()
            if f[0].isdigit(): out[int(f[0])] = tuple(int(float(f[i])) for i in (3, 4, 5, 6, 8, 9, 7))
    return out


def first_rows(tr):
    out = {}; prev = None
    for r in sorted(tr):
        if tr[r][0] != prev: out[tr[r][0]] = (r, tr[r][5]); prev = tr[r][0]   # row, IGT ticks
    return out


def core_trace(path):
    """trs: tick, room, x, y, frame, hp, seed, Llevel -> tick: (level, room, x, frame, seed)"""
    out = {}
    for l in open(path):
        f = l.split()
        if f and f[0].isdigit(): out[int(f[0])] = (int(f[7][1:]), int(f[1]), int(f[2]), int(f[4]), int(f[6]))
    return out


def differ(tr, core, rowof, lv):
    """the level's ticks on which the runner is not where the core is"""
    def same(r, c): return r is not None and r[:4] == c[:4] and abs(r[6] - c[4]) <= 8
    return [t for t in sorted(core) if core[t][0] == lv and t - 1 < len(rowof) and not same(tr.get(rowof[t - 1]), core[t])]


def main():
    prev, level, nxt, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    after = sys.argv[5:] or ["_"]; given = len(sys.argv) > 5
    core = core_trace(os.environ["CORE"]) if os.environ.get("CORE") else None
    rows = [l.strip() for l in open(prev) if l.strip()]
    start = len(rows)
    lev = [letters(l.strip()) for l in open(level) if l.strip().startswith("|")]
    n = len(lev)
    body = lev[:]; rowof = list(range(start, start + n))   # the level's rows, and each line's row
    scenes = []
    while True:
        tr = play(rows + body + after + ["."] * 400)
        lv = tr[start][0]
        # the tick count stands still on a row of the level (its last line apart: the next level's load): a scene
        i = next((i for i in range(1, n - 1) if rowof[i] > (scenes[-1] if scenes else 0) and tr[rowof[i]][4] == tr[rowof[i] - 1][4]), None)
        if i is None: break
        if len(scenes) > 4: sys.exit("more than 5 scenes inside the level: rows %s" % scenes)
        if core is None: sys.exit("a scene inside the level (line %d, row %d): CORE is needed to lay it" % (i + 1, rowof[i]))
        r = rowof[i]; found = None; tried = []
        for keep in (1, 0):   # Space on the row after the line's, or on the line's own row (the line after the scene)
            head = body[:r - start + keep] + ["_"]
            p = play(rows + head + ["."] * 400)
            q = next((x for x in range(r + keep + 1, r + keep + 400) if p[x][4] > p[r - 1][4]), None)   # the tick count moves again
            if q is None: continue
            for at in (q, q + 1, q - 1, q + 2):   # the row of the first line after the scene
                pad = at - (start + len(head))
                if pad < 0: continue
                b2 = head + ["."] * pad + body[r - start + keep:]
                ro2 = rowof[:i + keep] + [x + (len(b2) - len(body)) for x in rowof[i + keep:]]
                bad = differ(play(rows + b2 + after + ["."] * 400), core, ro2, lv)
                tried.append((len(bad), keep, at, bad[:3]))
                if not bad: found = (b2, ro2, keep, at); break
            if found: break
        if not found: sys.exit("scene on row %d (line %d): no layout plays as the core: %s" % (r, i + 1, sorted(tried)[:4]))
        body, rowof = found[0], found[1]
        scenes.append(r)
        print("a scene inside the level: line %d (row %d), Space on row %d, line %d on row %d (%d rows more)"
              % (i + 1, r, r + found[2], i + 1 + found[2], found[3], found[3] - r - found[2]))
    rows += body + after
    r = first_rows(tr)
    if core is not None:
        bad = differ(tr, core, rowof, lv)
        print("the runner against the core: %d ticks of level %d compared, %s" % (sum(1 for t in core if core[t][0] == lv), lv,
              "all equal (room, x, frame, seed)" if not bad else "%d DIFFER, from tick %d (row %d)" % (len(bad), bad[0], rowof[bad[0] - 1])))
        if bad: sys.exit(1)
    if nxt not in r: sys.exit("level %d not reached: %s" % (nxt, r))
    row, igt = r[nxt]
    if row < len(rows) and not given and after:
        # no story scene after the level: the next one starts on the row after its last line
        rows = rows[:len(rows) - len(after)]; after = []
        r = first_rows(play(rows + ["."] * 400)); row, igt = r[nxt]
        print("no scene after the level: nothing pressed there")
    if row < len(rows): sys.exit("level %d starts on row %d, before the rows end (%d)" % (nxt, row, len(rows)))
    rows += ["."] * (row - len(rows))
    open(out, "w").write("\n".join(rows) + "\n")
    print("level %d: rows %d..%d; level %d's first row %d (IGT %d); %s: %d rows" % (nxt - 1, start, row - 1, nxt, row, igt, out, len(rows)))


if __name__ == "__main__":
    main()
