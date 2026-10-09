#!/usr/bin/env python3
"""waitcheck.py SCRIPT.jaffar TRACE.txt [MINTICKS]

Where a known solution stands in one place, and where the script pulls there: the places a wide search can lose it.

While the way stands still (a jump up, a climb's start, a wait for a gate, a blade) its reward stands too, and every
state the magnet scores higher in that time outscores it: left of it under a pull left, say. If those states are many
(sword stance at any x, with the rest of the level in as many states as the database holds ways to have got there),
they fill the database and the way is dropped. Level 8 at 300 GB: room 12's bottom row pulled left, the way jumps
up twice at x 414 (46 ticks), the floor's left end is x 393: the solution found came 88 ticks later than the old
one, which still plays on the run's chain. rewardcheck.py does not see this: the way's own reward never dips.

TRACE is the solution tick by tick (trl or trs: the exploration tree's or the run's, each on its own chain). Every
stretch of MINTICKS (default 12) or more within 4 pixels in one room is listed with the script's steps of that room
and where their magnets pull there; `*` marks a step pulling more than 6 pixels away along x. Which step is the one
in force is to be read from its conditions (the script's are not evaluated here). To be judged by eye: a pull away
is harmless where nothing can stand there (a wall, a hole), or where the stretch ends in a new step.
"""
import json, re, sys


def trace(path):
    out = []
    for l in open(path, errors="replace"):
        m = re.match(r"\s*(\d+) \|.*?room\s+(\d+) x\s*(-?\d+) y\s*(-?\d+) row\s*(-?\d+) frame\s*(\d+)", l)   # trl
        if m: out.append(tuple(int(v) for v in m.groups())); continue
        f = l.split()   # trs: tick room x y frame hp seed Llevel
        if len(f) >= 8 and f[0].isdigit() and f[7].startswith("L"): out.append((int(f[0]), int(f[1]), int(f[2]), int(f[3]), -1, int(f[4])))
    return out


def steps(script):
    out = []
    for r in json.load(open(script))["Game Configuration"]["Rules"]:
        acts = r["Actions"]
        if any(a["Type"] in ("Trigger Win", "Trigger Fail") for a in acts): continue
        rooms = [c["Value"] for c in r["Conditions"] if c["Property"] == "Player Room" and c["Op"] == "=="]
        if not rooms: continue
        room = rooms[0]; mag = {}
        for a in acts:
            if a["Type"] in ("Set Player Pos X Magnet", "Set Player Pos Y Magnet") and a.get("Room") == room:
                mag[a["Type"][15]] = (a["Position"], a["Intensity"])
        conds = ", ".join("%s %s %s" % (c["Property"].replace("Player ", ""), c["Op"], c["Value"]) for c in r["Conditions"] if c["Property"] != "Player Room")
        out.append((room, conds, mag))
    return out


def main():
    st = steps(sys.argv[1]); tr = trace(sys.argv[2]); mint = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    i = 0; n = 0
    while i < len(tr):
        t0, room, x0, y0, row, f0 = tr[i]; j = i
        while j + 1 < len(tr) and tr[j + 1][1] == room and abs(tr[j + 1][2] - x0) <= 4 and abs(tr[j + 1][3] - y0) <= 2: j += 1
        if j - i + 1 >= mint:
            xs = [tr[k][2] for k in range(i, j + 1)]; fr = [tr[k][5] for k in range(i, j + 1)]
            print("ticks %d..%d (%d): room %d x %d..%d y %d, frames %d..%d" % (t0, tr[j][0], j - i + 1, room, min(xs), max(xs), y0, fr[0], fr[-1]))
            for r, conds, mag in st:
                if r != room: continue
                x = mag.get("X"); y = mag.get("Y")
                away = x is not None and x[1] > 0 and min(abs(x[0] - v) for v in xs) > 6
                print("   %s step [%s]: x %s, y %s" % ("*" if away else " ", conds or "the room",
                      "%g (%g)" % x if x and x[1] > 0 else "-", "%g (%g)" % y if y and y[1] > 0 else "-"))
                n += away
            i = j + 1
        else: i += 1
    print("%d pulls away" % n)


if __name__ == "__main__":
    main()
