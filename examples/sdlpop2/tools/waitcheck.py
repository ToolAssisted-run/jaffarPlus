#!/usr/bin/env python3
"""waitcheck.py SCRIPT.jaffar TRACE.txt [MINTICKS [MAP.txt]]

Where a known solution stands in one place, and where the script pulls there: the places a wide search can lose it.

While the way stands still (a jump up, a climb's start, a wait for a gate, a blade) its reward stands too, and every
state the magnet scores higher in that time outscores it: left of it under a pull left, say. If those states are many
(sword stance at any x, with the rest of the level in as many states as the database holds ways to have got there),
they fill the database and the way is dropped. Level 8 at 300 GB: room 12's bottom row pulled left, the way jumps
up twice at x 414 (46 ticks), the floor's left end is x 393: the solution found came 88 ticks later than the old
one, which still plays on the run's chain. rewardcheck.py does not see this: the way's own reward never dips.

TRACE is the solution tick by tick (trl or trs: the exploration tree's or the run's, each on its own chain). Every
stretch of MINTICKS (default 12) or more within 4 pixels in one room is listed with the script's steps of that room
that can be in force there and where their magnets pull: the last step whose conditions the trace shows met (room,
position, row, rooms seen; a step stays met), and the later ones of the room whose other conditions (tiles, gates,
hit points, sequences: not in the trace) may be. `*` marks a pull more than 6 pixels away along x, `!` one with no
pull up or down as well (a jump up gains nothing there). With MAP (the level's rooms, ~/pop2tools/lNNmap.txt) the
room's tiles are printed, `v` over the column he stands in. To be judged by eye: a pull away is harmless where
nothing can stand there (a wall, a hole), where the stretch is one every state must sit through (a fall, a scene),
or where it ends in a new step.
"""
import json, re, sys


def trace(path):
    out = []
    for l in open(path, errors="replace"):
        m = re.match(r"\s*(\d+) \|.*?room\s+(\d+) x\s*(-?\d+) y\s*(-?\d+) row\s*(-?\d+) frame\s*(\d+)", l)   # trl
        if m: out.append(tuple(int(v) for v in m.groups())); continue
        f = l.split()   # trs: tick room x y frame hp seed Llevel
        if len(f) >= 8 and f[0].isdigit() and f[7].startswith("L"):
            y = int(f[3]); out.append((int(f[0]), int(f[1]), int(f[2]), y, max(0, min(2, round((y - 56) / 63))), int(f[4])))
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
        out.append((room, r["Conditions"], mag))
    return out


OPS = {"==": lambda a, b: a == b, "!=": lambda a, b: a != b, "<": lambda a, b: a < b, "<=": lambda a, b: a <= b,
       ">": lambda a, b: a > b, ">=": lambda a, b: a >= b}


def holds(conds, t, seen):
    """1 met, 0 not, None: the conditions the trace shows are met, others are unknown"""
    tick, room, x, y, row, frame = t; unknown = False
    for c in conds:
        p, v = c["Property"], c["Value"]
        m = re.match(r"Visited Room\[(\d+)\]", p)
        val = {"Player Room": room, "Player Pos X": x, "Player Pos Y": y, "Player Current Row": row, "Player Frame": frame}.get(p)
        if m: val = 1 if int(m.group(1)) in seen else 0
        if val is None: unknown = True
        elif not OPS[c["Op"]](val, v): return 0
    return None if unknown else 1


def rooms_map(path):
    out = {}; cur = None
    for l in open(path):
        m = re.match(r"room\s+(\d+)\s", l)
        if m: cur = out.setdefault(int(m.group(1)), []); continue
        if cur is not None and len(cur) < 3 and l.startswith("    "): cur.append([w.split("/")[0] for w in l.split()])
    return out


def main():
    st = steps(sys.argv[1]); tr = trace(sys.argv[2]); mint = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    rmap = rooms_map(sys.argv[4]) if len(sys.argv) > 4 else {}
    # each step's state after every tick: 1 met (it stays), None may be met, 0 not yet
    state = [0] * len(st); hist = []; seen = set()
    for t in tr:
        seen.add(t[1])
        for k, (room, conds, mag) in enumerate(st):
            if state[k] == 1: continue
            h = holds(conds, t, seen)
            if h == 1: state[k] = 1
            elif h is None: state[k] = None
        hist.append(list(state))
    i = 0; n = 0
    while i < len(tr):
        t0, room, x0, y0, row, f0 = tr[i]; j = i
        while j + 1 < len(tr) and tr[j + 1][1] == room and abs(tr[j + 1][2] - x0) <= 4 and abs(tr[j + 1][3] - y0) <= 2: j += 1
        if j - i + 1 >= mint:
            xs = [tr[k][2] for k in range(i, j + 1)]; fr = [tr[k][5] for k in range(i, j + 1)]
            jump = any(67 <= f <= 80 or 87 <= f <= 99 for f in fr)
            sure = max((k for k in range(len(st)) if hist[j][k] == 1), default=-1)
            cand = [k for k in range(len(st)) if st[k][0] == room and (k == sure or (k > sure and hist[j][k] is None))]
            lines = []
            for k in cand:
                r, conds, mag = st[k]; x = mag.get("X"); y = mag.get("Y")
                away = x is not None and x[1] > 0 and min(abs(x[0] - v) for v in xs) > 6
                noy = away and jump and not (y and y[1] > 0 and abs(y[0] - y0) > 6)
                cs = ", ".join("%s %s %s" % (c["Property"].replace("Player ", ""), c["Op"], c["Value"]) for c in conds if c["Property"] != "Player Room")
                lines.append("   %s %s [%s]: x %s, y %s" % ("!" if noy else "*" if away else " ", "met  " if k == sure else "maybe", cs or "the room",
                             "%g (%g)" % x if x and x[1] > 0 else "-", "%g (%g)" % y if y and y[1] > 0 else "-"))
                n += away
            if any(l[3] in "*!" for l in lines) or not lines:
                print("ticks %d..%d (%d): room %d x %d..%d y %d row %d, frames %d..%d%s" % (t0, tr[j][0], j - i + 1, room, min(xs), max(xs), y0, row, fr[0], fr[-1], " (a jump up or a hang)" if jump else ""))
                if not lines: print("     no step of the room can be in force (one of another room is: %d)" % (st[sure][0] if sure >= 0 else 0))
                for l in lines: print(l)
                if room in rmap:
                    col = max(0, min(9, (xs[0] - 147) // 32))
                    print("        " + "   " * col + "v")
                    for rw in rmap[room]: print("        " + " ".join(rw))
            i = j + 1
        else: i += 1
    print("%d pulls away" % n)


if __name__ == "__main__":
    main()
