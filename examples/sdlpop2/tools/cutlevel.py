#!/usr/bin/env python3
"""cutlevel.py GAMEDIR SEED CHAIN.sol LEVEL.sol OUT.sol [NEWCHAIN.sol]

A level's solution as the final run plays it: Music On/Off (M) on the first tick the level-end music plays, which
ends the level's wait for it, and every line after the door's Up press emptied (inside the door the prince still
obeys the keys: a fresh backward press walks him out). OUT.sol ends on the tick the next level loads.

The music is queued, and started, in the tick after the one that leaves the prince on the stairs' frame 0xE0: M goes
two lines after that one (a line earlier it switches the music off before the piece starts, which then plays to its
end). What is left of the wait after it is the effect playing (169B:0541: caverns' drips hold a level's end a tick or
two), so the next level loads on the tick it is decided or a few later.

LEVEL.sol is a search's solution of either kind: with "Music On At Start" (the search presses M itself and wins on
the next level loaded: only the lines after Up change), or without (it ends on the tick the next level is decided:
M is put in, and empty lines are added until the next level loads). CHAIN.sol is the run up to the level (it replays
first). Checked with trs (env TRS, default ~/pop2tools-m/trs): the prince's ticks are those of LEVEL.sol, the next
level loads on OUT.sol's last line and not before. NEWCHAIN.sol, if given, is CHAIN.sol + OUT.sol.

The next level's search starts with the music off: its script takes "Music On At Start": true (M on its first tick).
A level that ends without stairs (2, 5, 9: no level-end music, the music stays on) is kept as it is and only checked.
"""
import os, subprocess, sys, tempfile

TRS = os.environ.get("TRS", os.path.expanduser("~/pop2tools-m/trs"))
EMPTY = "|.|......|"


def trace(game, seed, chain, lines):
    with tempfile.NamedTemporaryFile("w", suffix=".sol", delete=False) as f: f.write("\n".join(lines) + "\n")
    out = subprocess.run([TRS, game, seed, "0", "0", chain, f.name], capture_output=True, text=True, check=True).stdout
    os.unlink(f.name)
    rows = []
    for l in out.splitlines()[1:]:
        f = l.split()
        rows.append({"t": int(f[0]), "room": int(f[1]), "x": int(f[2]), "y": int(f[3]), "frame": int(f[4]),
                     "hp": int(f[5]), "seed": int(f[6]), "level": int(f[7][1:])})
    return rows


def main():
    game, seed, chain, level, out = sys.argv[1:6]
    lines = [l.rstrip("\n") for l in open(level) if l.startswith("|")]
    a = trace(game, seed, chain, lines)
    lv = a[0]["level"] if a[0]["level"] == a[1]["level"] else a[1]["level"]
    on = [r for r in a if r["level"] == lv]
    key = lambda r: (r["room"], r["x"], r["y"], r["frame"], r["hp"], r["level"])
    # the music key is pressed alone (Alt+M: with Alt held the arrows are not the prince's; Chimera's core drops them,
    # the search core takes both): a first line with more than the key is made the key alone, and checked
    alone = [l if l[1] != "M" else "|M|......|" for l in lines]
    more = [i + 1 for i, (l, m) in enumerate(zip(lines, alone)) if l != m]
    if not any(r["frame"] == 0xE0 for r in on):
        # no stairs (levels 2, 5 and 9 end otherwise, with no level-end music): the solution as it is
        n = len(lines)
        b = trace(game, seed, chain, alone) if more else a
        bad = [r["t"] for r, t in zip(a, b) if key(r) != key(t)]
        ok = not bad and b[n - 1]["level"] != lv and b[n - 2]["level"] == lv
        print("level %d: the solution %d lines, no stairs: kept as it is%s; the next level (%d) loads on tick %d, seed %u"
              % (lv, n, " (the music key alone on line %s)" % more if more else "", b[n - 1]["level"], n, b[n - 1]["seed"]))
        if bad: print("with the music key alone the run differs on ticks", bad[:10])
        if ok:
            with open(out, "w") as f: f.write("\n".join(alone) + "\n")
            if len(sys.argv) > 6:
                with open(sys.argv[6], "w") as f: f.write(open(chain).read() + "\n".join(alone) + "\n")
                print("chain:", sys.argv[6], sum(1 for l in open(sys.argv[6]) if l.startswith("|")), "lines")
        print("OK" if ok else "FAILED")
        sys.exit(0 if ok else 1)
    # the stairs: the last run of frames 0xD9.. that reaches 0xE0 (the music starts in the tick after)
    e0 = max(r["t"] for r in on if r["frame"] == 0xE0)
    first = e0
    while first > 1 and 0xD9 <= a[first - 2]["frame"] <= 0xE0: first -= 1
    up = max(i for i in range(1, first + 1) if lines[i - 1][5] == "U")
    m = e0 + 2
    cut = alone[:up] + [EMPTY] * max(0, m - up)
    cut[m - 1] = "|M|......|"
    b = trace(game, seed, chain, cut)
    while b[-1]["level"] == lv and len(cut) < m + 400:   # played on without input until the next level loads
        cut.append(EMPTY); b = trace(game, seed, chain, cut)
    n = len(cut)
    same = min(len(on), n - 1)
    bad = [r["t"] for r, s in zip(a[:same], b[:same]) if key(r) != key(s)]
    had = [i + 1 for i, l in enumerate(lines) if l[1] == "M" and i > 0]
    print("level %d: the solution %d lines%s; Up on line %d (stairs from tick %d), the music starts in tick %d, M on line %d"
          % (lv, len(lines), " (M on line %s)" % had if had else "", up, first, e0 + 1, m))
    if [i for i in more if i <= up]: print("the music key alone on line", [i for i in more if i <= up])
    print("the next level (%d) loads on tick %d, seed %u" % (b[-1]["level"], n, b[-1]["seed"]))
    ok = not bad and b[-1]["level"] != lv and b[-2]["level"] == lv
    if bad: print("the cut run differs from the solution on ticks", bad[:10])
    if ok:
        with open(out, "w") as f: f.write("\n".join(cut) + "\n")
        if len(sys.argv) > 6:
            with open(sys.argv[6], "w") as f: f.write(open(chain).read() + "\n".join(cut) + "\n")
            print("chain:", sys.argv[6], sum(1 for l in open(sys.argv[6]) if l.startswith("|")), "lines")
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
