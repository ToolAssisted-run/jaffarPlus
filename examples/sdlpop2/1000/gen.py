#!/usr/bin/env python3
"""Level 10 scripts (Sergio's route; room 32's big potion is far out of the way: not taken): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 10 (the temple), rooms along the route:  22 11  8 14 12 30 31 ...
                                                              19  7 10 ...
Level 9's route (level010203040506070809.sol: no potion, the horse ride skipped) starts level 10 in room 22 (row 2,
x 310, facing right). Room 8's bottom row is walled at col 2: up to its middle row there (a small potion at col 8,
never drunk). Room 14: across to the right (a standing jump may end hanging from room 12's floor, his feet in room
19, a running jump not). Room 12: its bottom row's loose floor (col 1) dropped falls into room 19 onto the plate
under it (row 2 col 1), which opens the exit door (row 1 cols 4..5); then right and down into room 19 (col 7's loose
floor, 0x1A) onto its middle row, into the door.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

RUN = "--run" in sys.argv
H0 = 4 if RUN else 5   # max HP at level 10's start ("Max HP At Level", as levels 7 to 9); the final run (--run) comes
                       # with the 4 it has, no "Max HP At Level"
LOOSE12 = ("Tile[12][22]", "!=", 0x0B)    # room 12's loose floor (row 2 col 1) gone (falling, onto room 19's plate)
EXIT = ("Tile Opening[19][16]", ">", 0)   # room 19's exit door (row 1 cols 4..5) opening

ROUTES = {
    # 22 11 8 (up to its middle row) 14 12 (the loose floor dropped) 19 (the door)
    "np": [
        step(room(22), (22, R, ROW_Y[2], 1.0, 1.0)),
        step(room(11), (11, R, ROW_Y[2], 1.0, 1.0)),
        step(room(8), (8, R, ROW_Y[1], 1.0, 2.0)),
        # 14: across to room 12 (a short jump hangs from its floor's edge, his body in room 19: not penalized there,
        # drawn up, back into room 12)
        # (the hang's magnet stands just above it, y 30: the 18 ticks hanging and climbing score what the jump did)
        step(room(14), (14, R, None, 1.0, 1.0), (19, col_x(0), 30, 1.0, 1.0, "keep")),
        # 12: right, over the loose floor (col 1, on his way: it falls ~11 ticks later) and on to the room's right
        # end (pulled to the loose floor, the 11 ticks running on from it scored 145 less than standing on it); the
        # hang in room 19 pulls right as room 12 does
        step(room(12), (12, R, ROW_Y[2], 1.0, 1.0), (19, R, 30, 1.0, 1.0, "keep")),
        step(room(12, LOOSE12), (12, R, D, 1.0, 1.0)),
        # (room 19 by the door's side, where he drops in: under the loose floor, x ~186, it is no step)
        step(room(19, LOOSE12, ("Player Pos X", ">=", 250)), (19, col_x(5), ROW_Y[1], 1.0, 1.0)),
        step(room(19, EXIT, ("Player Pos X", ">=", 250)), (19, col_x(5), ROW_Y[1], 1.0, 1.0)),
        # into the door (Up by it: sequence 0x46): its frames take him inside it (x ~col 5 + 31, y 26 above the floor)
        step(room(19, ("Player Current Sequence", "==", 0x46)), (19, col_x(5) + 31, ROW_Y[1] - 26, 1.0, 1.0)),
    ],
}
HPM = 50   # a small Player HP Magnet: ties only (level 7: without it the search wore down to 1 hit point and waited)
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"np": H0}

# the small potions (+1 hit point; room 8's on the way) are never drunk: a drink (sequence 0x4E) fails
FAILS = {k: [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 10)]] for k in ROUTES}

LEVEL = {
    "name": "l10", "next": 11, "init": "level010203040506070809.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [[19, 15], [19, 16]],   # the exit door
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {} if RUN else {"Max HP At Level": [10, H0]},
}

if __name__ == "__main__":
    main(LEVEL)
