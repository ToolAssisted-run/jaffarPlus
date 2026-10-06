#!/usr/bin/env python3
"""Level 12 scripts (Sergio's shortcut; the big potion is far out of the way: not taken): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 12, rooms:    .  4  .  21  .  .  .
                    2  5  1  3  11 13  .
                   10  9  8  19  7  12 20
                   17  6  16 15  14 22 18 24 23 25 26
Level 11's route (level0102030405060708091011.sol) starts level 12 in room 19 (row 2, x 200, facing left), max HP 6.
The exit door: room 7 (row 2 col 3), opened by room 14's top-row plate (col 8) or room 24's (row 2 col 3). Room 7's
bottom row is floor at cols 0..5, wall at cols 6..7, open at col 8, straight above room 14's plate.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

H0 = 6   # max HP at level 12's start (level 11's potion)
EXIT = ("Tile Opening[7][24]", ">", 0)   # the exit door (room 7 row 2 col 3) opening

GATE16 = ("Tile Opening[16][29]", ">", 0)  # room 16's gate (row 2 col 8), opened by room 15's plate (row 2 col 5)
GATE6 = ("Tile Opening[6][30]", ">", 0)    # room 6's gate (row 2 col 9), opened by room 16's plate (row 2 col 3); its
                                            # closers: room 16 row 2 cols 6 and 1, row 0 col 1
SHAKE10 = ("Tile Opening[10][26]", ">", 0)  # room 10's loose floor (row 2 col 5: room 17's ceiling) shaking
LOOSE10 = ("Tile[10][26]", "!=", 0x0B)      # ... gone
GATE8 = ("Tile Opening[8][30]", ">", 0)    # room 8's gate (row 2 col 9), opened by its top-row plate (col 4); its
                                            # closer: row 2 col 6, under the top row's hole
SEEN8 = ("Visited Room[8]", "==", 1)
TOP = ("Player Pos Y", "<", 100)

DOOR = [
    # (Sergio) 19 7 (right, through the bottom row's wall, cols 6..7, by a wall clip; down the col 8 hole) 14 (onto its
    # plate: the exit door)
    step(room(19), (19, R, ROW_Y[2], 1.0, 1.0)),
    step(room(7), (7, col_x(8), D, 1.0, 1.0)),
    step(room(14), (14, col_x(8), ROW_Y[0], 1.0, 1.0)),
    # 14, the door opening: its top row is wall left of the plate (cols 0..7): through it by a sword wall clip (Sergio),
    # out of its left end in room 15's shaft
    step(room(14, EXIT), (14, L, ROW_Y[0], 1.0, 1.0)),
]
ROUTES = {
    "door": DOOR,
    # (Sergio) ... 14 15 16 6 17 (up, room 10's loose floor broken from below) 10 9 (up to its top row) 8 19 7 (the door)
    "np": DOOR + [
        step(room(15, EXIT), (15, L, ROW_Y[2], 1.0, 1.0)),
        step(room(16, EXIT, GATE16), (16, col_x(3), ROW_Y[2], 1.0, 1.0)),
        step(room(16, EXIT, GATE6), (16, L, ROW_Y[2], 1.0, 1.0)),
        step(room(6, EXIT, ("Player Pos X", "<", col_x(8))), (6, L, ROW_Y[2], 1.0, 1.0)),
        step(room(17, EXIT), (17, col_x(5), U, 1.0, 2.0)),
        step(room(17, EXIT, SHAKE10), (17, col_x(5), U, 1.0, 2.0)),
        step(room(17, EXIT, LOOSE10), (17, col_x(5), U, 1.0, 2.0)),
        step(room(10, EXIT, LOOSE10), (10, R, ROW_Y[2], 1.0, 1.0)),
        step(room(9, EXIT, LOOSE10), (9, col_x(5), U, 1.0, 2.0)),
        step(room(9, EXIT, LOOSE10, TOP), (9, R, ROW_Y[0], 1.0, 1.0)),
        # 8: down its top row's hole (col 6) onto the closer under it (row 2 col 6: it shuts the gate at col 9, whatever
        # the top row's plate did): through the gate by a sword wall clip
        step(room(8, EXIT, LOOSE10), (8, col_x(6), D, 1.0, 2.0)),
        step(room(8, EXIT, LOOSE10, row(2)), (8, R, ROW_Y[2], 1.0, 1.0)),
        # 19 (Sergio): the sword put away at once, then a run (the sheathing gains no x for ~20 ticks: unrewarded, the
        # search walked the whole room sword in hand)
        step(room(19, EXIT, SEEN8), (19, R, ROW_Y[2], 1.0, 1.0)),
        step(room(19, EXIT, SEEN8, ("Player Sword Drawn", "==", 0)), (19, R, ROW_Y[2], 1.0, 1.0)),
        step(room(7, EXIT, SEEN8), (7, col_x(3), ROW_Y[2], 1.0, 1.0)),
        step(room(7, EXIT, SEEN8, ("Player Current Sequence", "==", 0x46)), (7, col_x(3) + 31, ROW_Y[2] - 26, 1.0, 1.0)),
    ],
}
# (a test against the clip left: its 2-row fall in room 15 costs a hit point and a ~42-tick hard landing) right off
# the plate's ledge (col 9) into room 22's left column, hanging first (a 1-row drop), back left along room 14's and
# room 15's bottom rows; the clip (room 15 entered above its bottom row) fails
SEEN22 = ("Visited Room[22]", "==", 1)
ROUTES["via22"] = DOOR[:3] + [
    step(room(14, EXIT), (14, R, ROW_Y[0], 1.0, 1.0), (22, L, ROW_Y[2], 1.0, 2.0, "keep")),
    step(room(22, EXIT), (22, L, ROW_Y[2], 1.0, 2.0), (14, R, D, 1.0, 2.0, "keep")),
    step(room(22, EXIT, row(2)), (22, L, ROW_Y[2], 1.0, 2.0)),
    step(room(14, EXIT, SEEN22, row(2)), (14, L, ROW_Y[2], 1.0, 1.0)),
] + ROUTES["np"][4:]
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"door": H0, "np": H0, "via22": H0}

# no potion is drunk (sequence 0x4E)
FAILS = {k: [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 12)]] for k in ("door", "np", "via22")}
FAILS["via22"].append([("Player Room", "==", 15), ("Player Pos Y", "<", 150), ("Current Level", "==", 12)])

LEVEL = {
    "name": "l12", "next": 13, "init": "level0102030405060708091011.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [[7, 24], [14, 9], [16, 29], [6, 30], [10, 26], [8, 30], [8, 25], [17, 27]],   # the exit door, its plate, the gates, room 10's loose floor
    "hash_visited": [8, 22],
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {"Max HP At Level": [11, 5]},   # (level 11 replays as searched: 5, its potion 6)
}

if __name__ == "__main__":
    main(LEVEL)
