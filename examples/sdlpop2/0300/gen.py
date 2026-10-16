#!/usr/bin/env python3
"""Level 3 scripts, one per route (the big potions taken or not, the exit used): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

The solutions: --db 100000, Max Steps 650 / 900 / 1050 / 1250 for right / p1 / p2 / both.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, room, hp, step, ENTER, dedup, main  # noqa: E402

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


ROUTES = {}
# the most max HP a route may have (level 3's big potions: room 10 tile 18 and room 20 tile 9; the others are small)
MAXHP = {"both": 5, "p1": 4, "p2": 4, "right": 3, "left": 3}

# right exit: 22 -> 30 -> 31 -> 30 (top row, the plate at row 0 col 8 opens the door) -> 31 -> 30 (lower row) -> exit
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


LEVEL = {
    "name": "l3", "next": 4, "init": "level0102.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [[3, 26], [10, 20], [17, 28], [20, 21], [24, 2], [23, 2], [8, 24], [30, 29], [30, 26], [30, 9], [13, 17]],
    "fails": {"p2": [[("Player Room", "==", 10), ("Player Max HP", "==", 4)]]},   # potion 2 only: not potion 1
}

if __name__ == "__main__":
    main(LEVEL)
