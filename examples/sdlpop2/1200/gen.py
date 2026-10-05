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

ROUTES = {
    # (Sergio) 19 7 (right, through the bottom row's wall, cols 6..7, by a wall clip; down the col 8 hole) 14 (onto its
    # plate: the exit door)
    "door": [
        step(room(19), (19, R, ROW_Y[2], 1.0, 1.0)),
        step(room(7), (7, col_x(8), D, 1.0, 1.0)),
        step(room(14), (14, col_x(8), ROW_Y[0], 1.0, 1.0)),
        step(room(14, EXIT), (14, col_x(8), ROW_Y[0], 1.0, 1.0)),
    ],
}
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"door": H0}

# no potion is drunk (sequence 0x4E)
FAILS = {"door": [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 12)]]}

LEVEL = {
    "name": "l12", "next": 13, "init": "level0102030405060708091011.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [[7, 24], [14, 9]],   # the exit door, its plate
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {"Max HP At Level": [11, 5]},   # (level 11 replays as searched: 5, its potion 6)
}

if __name__ == "__main__":
    main(LEVEL)
