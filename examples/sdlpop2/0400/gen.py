#!/usr/bin/env python3
"""Level 4 scripts, one per route: see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 4: the big potion is in room 11 (row 2 col 7), a slow-fall potion in room 6 (row 0 col 2). The exit door is in
room 22 (row 2 cols 3..4), opened by the plate in room 26 (row 1 col 8). Guards: 7, 13, 14, 17, 23, 26, 27, 28 (three).
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, ENTER, dedup, main  # noqa: E402

DOOR = ("Tile Modifier[22][25]", ">", 0)   # the exit door's opening (0 = shut)
OPP = None   # no Opponent HP Magnet: the skeletons (char id 4) fall to 0 HP and rise again, so it rewards staying by one


def tail():
    """10 -> 23 -> 22 -> 24 -> 25 -> 27 -> 28 -> 26 (the plate) -> 28 -> 27 -> 25 -> 24 -> 22 (the door)"""
    back = lambda r: (r, L, U, 1.0, 1.0, "keep")   # (the top row: the walls there can be slid through, over the guards)
    return [
        step(room(10), (10, None, D, 1.0, 1.0)),
        step(room(23), (23, None, D, 1.0, 1.0), opp=OPP),
        step(room(22), (22, R, D, 1.0, 1.0)),
        step(room(24), (24, R, D, 1.0, 1.0)),
        step(room(25), (25, R, D, 1.0, 1.0)),
        step(room(27), (27, R, D, 1.0, 1.0), opp=OPP),
        step(room(28), (28, R, D, 1.0, 1.0), opp=OPP),
        step(room(26), (26, col_x(8), ROW_Y[1], 1.0, 2.0), opp=OPP),
        # the door opens: everything leads back left to room 22
        step([DOOR], (26, L, U, 1.0, 1.0, "keep"), back(28), back(27), back(25), back(24),
             (22, col_x(4), D, 1.0, 1.0, "keep"), opp=OPP),
        step(room(28, DOOR), (28, L, U, 1.0, 1.0), back(27), back(25), back(24), (22, col_x(4), D, 1.0, 1.0, "keep"), opp=OPP),
        step(room(27, DOOR), (27, L, U, 1.0, 1.0), back(25), back(24), (22, col_x(4), D, 1.0, 1.0, "keep"), opp=OPP),
        step(room(25, DOOR), (25, L, U, 1.0, 1.0), back(24), (22, col_x(4), D, 1.0, 1.0, "keep")),
        step(room(24, DOOR), (24, L, U, 1.0, 1.0), (22, col_x(4), D, 1.0, 1.0, "keep")),
        step(room(22, DOOR), (22, col_x(4), D, 1.0, 1.0)),
        ENTER,
    ]


ROUTES = {
    # 1 -> 2 -> 4 -> 3 -> 7 -> 11 (the big potion) -> 7 -> 15 -> 10 -> tail
    "potion": dedup([
        step(room(1), (1, R, None)),
        step(room(2), (2, None, D, 1.0, 1.0)),
        step(room(4), (4, R, ROW_Y[1], 1.0, 1.0)),
        step(room(3), (3, R, D, 1.0, 1.0)),
        step(room(7, hp(3)), (7, R, D, 1.0, 1.0), opp=OPP),
        # the plate at col 1 holds the gates at cols 2 and 5 open: stand on it until col 5's is well open, then run
        # (that gate's modifier carries 0x400 above its opening)
        step(room(11, hp(3)), (11, col_x(1), D, 1.0, 1.0)),
        step(room(11, hp(3), ("Tile Modifier[11][26]", ">=", 0x400 + 0xA0)), (11, col_x(7), D, 1.0, 1.0)),
        step(room(11, hp(4)), (11, L, None)),
        step(room(7, hp(4)), (7, None, D, 1.0, 1.0), opp=OPP),
        step(room(15), (15, L, D, 1.0, 1.0)),
    ] + tail()),
    # 1 -> 2 -> 4 -> 6 (the slow-fall potion) -> 10 -> tail
    "slowfall": dedup([
        step(room(1), (1, R, None)),
        step(room(2), (2, None, D, 1.0, 1.0)),
        step(room(4), (4, None, D, 1.0, 1.0)),
        step(room(6), (6, col_x(2), D, 1.0, 1.0)),
    ] + tail()),
}
MAXHP = {"potion": 4, "slowfall": 3}

LEVEL = {
    "name": "l4", "next": 5, "init": "level010203.sol", "routes": ROUTES, "maxhp": MAXHP,
    # gates (2: 26, 4: 27, 11: 23 and 26, 24: 23) and the exit door (22: 25)
    "hash": [[2, 26], [4, 27], [11, 23], [11, 26], [24, 23], [22, 25]],
}

if __name__ == "__main__":
    main(LEVEL)
