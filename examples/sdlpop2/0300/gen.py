#!/usr/bin/env python3
"""Level 3 scripts, one per route (the big potions taken or not, the exit used).

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG]

A route is a list of steps; each step is a rule (conditions, all ANDed) that adds a reward and sets the X / Y magnets
of the rooms it names. --upto N makes the N-th step (1-based) the win, for solving the route in chunks.
"""
import argparse, copy, json, os

L, R = -200, 800     # X magnet positions beyond a room's left / right edge (col c stands at x ~ 154 + 32 c)
U, D = -50, 300      # Y magnet positions above row 0 / below row 2 (y 55, 119, 181)


def room(r, *extra):
    return [("Player Room", "==", r)] + list(extra)


def hp(n):
    return ("Player Max HP", "==", n)


def step(cond, *mags):
    """mags: (room, x or None, y or None[, x intensity, y intensity])"""
    return {"cond": cond, "mags": mags}


# Common start (routes 1..4): 1 -> 4 -> 16 -> 3 -> 2 -> 14
START = [
    step(room(1), (1, R, None)),
    step(room(4), (1, R, None), (4, R, D, 1.0, 1.0)),
    step(room(16), (4, R, D), (16, R, None)),
    step(room(3), (16, R, None), (3, R, D, 1.0, 1.0)),
    step(room(2), (3, R, None), (2, R, D, 1.0, 1.0)),
]

# 14 -> 10 potion 1 -> 14 (the first visit of 14 heads left on row 1)
POTION1 = [
    step(room(14, hp(3)), (2, R, D), (14, L, 119, 1.0, 2.0)),
    step(room(10, hp(3)), (14, L, None), (10, 378, 119, 1.0, 1.0)),
    step(room(10, hp(4)), (10, R, 119, 1.0, 1.0)),
    step(room(14, hp(4)), (10, R, None)),
]
NO_POTION1 = []

# 14 -> 15 -> 17 (climb to the plate at row 1 col 2, the gate at row 2 col 7 opens) -> 19 -> 22
def to22(maxhp, out22=(R, None)):
    return [
        step(room(14, hp(maxhp)), (14, R, D, 1.0, 1.0)),
        step(room(15), (14, R, None), (15, R, None)),
        step(room(17), (15, R, None), (17, R, None)),
        step(room(19), (17, R, None), (19, R, D, 1.0, 1.0)),
        step(room(22), (19, R, None), (22, out22[0], out22[1], 1.0, 1.0)),
    ]


def dedup(steps):
    """drop a step whose conditions repeat the previous one's (14 entered with max HP 3, then again)"""
    out = []
    for s in steps:
        if out and out[-1]["cond"] == s["cond"]:
            out[-1] = s
        else:
            out.append(s)
    return out


ROUTES = {}
# the most max HP a route may have (level 3's big potions: room 10 tile 18 and room 20 tile 9; the others are small)
MAXHP = {"both": 5, "p1": 4, "p2": 4, "right": 3, "left": 3}

# right exit: 22 -> 30 -> 31 -> 30 (top row, the plate at row 0 col 8 opens the door) -> 31 -> 30 (lower row) -> exit
# the prince steps into an open exit door (Up, standing by it: control.c control_standing_up, sequence 0x46); the
# level ends ~22 ticks later
ENTER = step([("Player Current Sequence", "==", 0x46)])


def right_exit():
    """The plate at row 2 col 1 opens the gate at row 2 col 8 (for good, in ~50 ticks); 31's top row leads back into
    30's top row, where the plate at col 8 opens the exit door (row 2 cols 4..5); then down 31 and back through the gate."""
    # the gate stays open (0xFF) only when something lies on its plate: the loose floor above it (row 0 col 1) dropped
    # onto it; pressed by the prince, it opens to 0xFA and shuts again (mobs.c anim_gate, ovl_button22)
    gate = [("Tile Modifier[30][29]", ">", 0), ("Tile[30][2]", "==", 0)]
    door = ("Tile Modifier[30][26]", ">", 0)
    row = lambda r: ("Player Current Row", "==", r)
    return [
        step(room(30), (22, L, None), (30, 186, D, 1.0, 1.0)),
        step(room(30, *gate), (30, R, D, 1.0, 1.0)),
        step(room(31, *gate), (31, 218, U, 1.0, 2.0)),                            # climb at col 2, twice
        step(room(31, *gate, row(0)), (31, L, U, 1.0, 1.0)),
        step([door], (30, R, None, 1.0, 1.0, "keep"), (31, 250, D, 1.0, 2.0, "keep")),
        step(room(31, door, row(2)), (31, L, None), (30, 298, D, 1.0, 1.0, "keep")),
        step(room(30, door, row(2)), (30, 298, D, 1.0, 1.0), (31, L, None, 1.0, 1.0, "keep")),
        ENTER,
    ]


# room 13: the loose floor at row 1 col 8 falls on the plate below it, which opens the exit door (row 1 cols 5..6)
EXIT13 = [
    step(room(13), (13, 410, 119, 1.0, 1.0)),
    step(room(13, ("Tile Modifier[13][17]", ">", 0)), (13, 330, 119, 1.0, 1.0)),
    ENTER,
]


# potion 2: 22 -> 18 -> 20 potion -> 21 -> 26 -> 27 -> 28 -> 25 -> 24 -> 23 -> 13 exit
def potion2(maxhp):
    return [
        step(room(18), (22, L, None), (18, 410, D, 1.0, 1.0)),   # (down col 8: the potion is on 20's top row, col 8)
        step(room(20, hp(maxhp)), (18, L, None), (20, 410, U, 1.0, 1.0)),
        step(room(20, hp(maxhp + 1)), (20, L, D, 1.0, 1.0)),
        step(room(21), (20, L, None), (21, L, None)),
        step(room(26), (21, L, None), (26, None, U)),
        step(room(27), (26, None, U), (27, L, None)),
        step(room(28), (27, L, None), (28, L, None)),
        step(room(25), (28, L, None), (25, None, U)),
        step(room(24), (25, None, U), (24, L, None)),
        step(room(23), (24, L, None), (23, L, None)),
        step(room(13), (23, L, None), (13, 410, 119, 1.0, 1.0)),
    ] + EXIT13[1:]


ROUTES["both"] = dedup(START + POTION1 + to22(4, (L, 119)) + potion2(4))   # (22 -> 18 on row 1)
ROUTES["p1"] = dedup(START + POTION1 + to22(4) + right_exit())
ROUTES["p2"] = dedup(START + NO_POTION1 + to22(3, (L, 119)) + potion2(3))
ROUTES["right"] = dedup(START + NO_POTION1 + to22(3) + right_exit())
# left exit: 1 -> 5 -> 7 -> 8 (the loose floor at row 1 col 8 falls onto room 11's plate at row 2 col 8, which opens
# 8's gate at row 2 col 3) -> 11 (through the hole at row 2 col 1) -> 13
ROUTES["left"] = [
    step(room(1), (1, L, None)),
    step(room(5), (1, L, None), (5, L, None)),
    step(room(7), (5, L, None), (7, 360, D, 1.0, 1.0)),
    step(room(8), (7, 360, D), (8, 410, 119, 1.0, 2.0)),
    step(room(8, ("Tile Modifier[8][24]", ">", 0)), (8, 186, D, 1.0, 1.0)),
    step(room(11), (8, 186, D), (11, R, D, 1.0, 1.0)),
    step(room(13), (11, R, None), (13, 410, 119, 1.0, 2.0)),
] + EXIT13[1:]


def build(route, upto, db, init, tag, maxsteps=3000):
    base = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../0100/script.jaffar")))
    j = copy.deepcopy(base)
    j["Driver Configuration"]["Save Intermediate Results"]["Best Solution Path"] = f"/tmp/jaffar.pop2l3.{tag}.best.sol"
    j["Driver Configuration"]["Save Intermediate Results"]["Worst Solution Path"] = f"/tmp/jaffar.pop2l3.{tag}.worst.sol"
    j["Driver Configuration"]["Max Steps"] = maxsteps
    j["Engine Configuration"]["State Database"]["Max Size (Mb)"] = db
    e = j["Emulator Configuration"]
    e["Start Level"] = 1
    e["Seed"] = 7881336   # the restart combo's level 1 seed (Chimera Random Seed 25)
    e["Initial Sequence File Path"] = init
    g = j["Game Configuration"]
    g["Print Properties"] = ["Current Level", "Next Level", "Player Room", "Player Pos X", "Player Pos Y", "Player Frame",
                             "Player Action", "Player Current Row", "Player Current Col", "Player Current HP", "Player Max HP"]
    g["Restart At Checkpoint"] = False
    # gates and exit doors: their openings are in the tile modifiers, which are not hashed otherwise
    g["Hash Tile Modifiers"] = [[3, 26], [10, 20], [17, 28], [20, 21], [24, 2], [23, 2], [8, 24], [30, 29], [30, 26], [30, 9], [13, 17]]
    rules = []
    steps = ROUTES[route]
    n = len(steps) if upto is None else upto
    prev = None   # the room of the previous step
    need = None   # the max HP a step asked for holds for every step after it (a later step would otherwise satisfy
                  # the potion's step through the chain without the potion)
    for i, s in enumerate(steps[:n]):
        hps = [c for c in s["cond"] if c[0] == "Player Max HP"]
        if hps: need = hps[0]
        elif need is not None: s = dict(s, cond=s["cond"] + [need])
        acts = [{"Type": "Add Reward", "Value": 1000.0}]
        own = s["cond"][0][2] if s["cond"] and s["cond"][0][0] == "Player Room" else prev
        mags = list(s["mags"])
        if prev is not None and own != prev and prev not in [m[0] for m in mags]: mags.append((prev, None, None))
        prev = own
        for m in mags:
            r, x, y = m[0], m[1], m[2]
            xi, yi = (m[3], m[4]) if len(m) > 3 else (1.0, 1.0)
            if r != own and len(m) < 6: x, y, xi = -5000, None, 1.0   # the room just left: going back costs more than any magnet gains
            if x is not None: acts.append({"Type": "Set Player Pos X Magnet", "Room": r, "Intensity": xi, "Position": x})
            if y is not None: acts.append({"Type": "Set Player Pos Y Magnet", "Room": r, "Intensity": yi, "Position": y})
        conds = [{"Property": p, "Op": o, "Value": v} for p, o, v in s["cond"]]
        rules.append({"Label": 3000 + i, "Conditions": conds, "Satisfies": [3000 + i - 1] if i else [], "Actions": acts})
        if upto is not None and i == n - 1:   # a chunk ends with the prince standing (not in a fall he may not survive)
            rules.append({"Label": 3899, "Conditions": conds + [{"Property": "Player Action", "Op": "==", "Value": 1}], "Satisfies": [],
                          "Actions": [{"Type": "Trigger Win"}]})
    if upto is None:
        rules.append({"Label": 3900, "Conditions": [{"Property": "Next Level", "Op": "==", "Value": 4}], "Satisfies": [], "Actions": [{"Type": "Trigger Win"}]})
    rules.append({"Label": 3901, "Conditions": [{"Property": "Player Is Alive", "Op": ">=", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3902, "Conditions": [{"Property": "Player Current HP", "Op": "==", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3903, "Conditions": [{"Property": "Player Room", "Op": "==", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3904, "Conditions": [{"Property": "Player Max HP", "Op": ">", "Value": MAXHP[route]}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    if route == "p2":   # potion 2 only: not potion 1
        rules.append({"Label": 3905, "Conditions": [{"Property": "Player Room", "Op": "==", "Value": 10}, {"Property": "Player Max HP", "Op": "==", "Value": 4}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    # rooms off the route fail: a room with no magnet would otherwise be free of any magnet cost
    on = {c[2] for st in steps for c in st["cond"] if c[0] == "Player Room"} | {m[0] for st in steps for m in st["mags"]}
    for r in range(1, 33):
        if r not in on:
            rules.append({"Label": 3910 + r, "Conditions": [{"Property": "Player Room", "Op": "==", "Value": r}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    g["Rules"] = rules
    return j


if __name__ == "__main__":
    a = argparse.ArgumentParser()
    a.add_argument("route", choices=list(ROUTES))
    a.add_argument("--upto", type=int)
    a.add_argument("--db", type=int, default=15000)   # (the solutions: 100000, Max Steps 650 / 900 / 1050 / 1250 for right / p1 / p2 / both)
    a.add_argument("--init", default="level0102.sol")
    a.add_argument("--out")
    a.add_argument("--tag")
    a.add_argument("--maxsteps", type=int, default=3000)
    o = a.parse_args()
    tag = o.tag or o.route
    json.dump(build(o.route, o.upto, o.db, o.init, tag, o.maxsteps), open(o.out or f"script-{o.route}.jaffar", "w"), indent=2)
