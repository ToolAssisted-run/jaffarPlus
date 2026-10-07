#!/usr/bin/env python3
"""Level 14 scripts: see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 14 (the final one), rooms:    .  .  .  7  8      (10 above 8)
                                    4  5  6
                                    3  9
                                    1
                                    2
Level 13's route starts level 14 in room 1 (row 1, x 306, facing right, 7/7): a ledge of five tiles (row 1 cols
1..5) over room 2's three (row 0 cols 3..5). The prince in room 2, not falling, his right side at x <= 305 (final.c
kind6_kid): the scene with Jaffar at the window plays, the level is loaded again and he stands in room 4 (row 1 col 5,
facing right). Room 3 (under room 4): guards appear, a dead one may leave a potion. Rooms 6 (four Jaffars), 7 | 8
(Jaffar's round; the spirit's fireball kills him).
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

H0 = 7   # max HP at level 14's start (levels 11's and 13's potions)

ROUTES = {
    # 1 (down off the ledge's right end, col 5) 2 (its floor, leftwards: the scene) -> 4
    "scene": [
        step(room(1), (1, col_x(5) + 24, D, 1.0, 2.0)),
        step(room(2), (2, L, ROW_Y[0], 1.0, 1.0)),
        step(room(4), (4, R, ROW_Y[1], 1.0, 1.0)),
    ],
}
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"scene": H0}

# no potion is drunk (sequence 0x4E)
FAILS = {"scene": [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 14)]]}

LEVEL = {
    "name": "l14", "next": 15, "init": "level01020304050607080910111213.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [],
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {"Max HP At Level": [11, 5]},   # (level 11 replays as searched: 5, its potion 6, level 13's 7)
}

if __name__ == "__main__":
    main(LEVEL)
