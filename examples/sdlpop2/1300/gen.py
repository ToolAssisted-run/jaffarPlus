#!/usr/bin/env python3
"""Level 13 scripts (Sergio's route): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 13, rooms:   .  .  .  .  .  . 24  .  . 12  .  . 14
                   4 29 13 28  2 32 19 11 10  9 20  3  8
                   .  . 15  .  .  . 22 18 17 16  .  5  .
(more below: 27 30 25 under 22, 6 31 under 18, 7 under 17, 1 under 16.) Level 12's route starts level 13 in room 3
(row 1, x 264, facing left, 6/6). The way is the middle row, leftwards, to room 4 (the flame, row 1 col 1): its plate
(row 1 col 2) opens the three exit doors (room 28 row 1 cols 5..6, room 32 row 1 cols 1..2 and col 5); back right to
room 28's. In room 4 a prince on row 1 at x <= 218 burns (shadow13.c): he dies, his shadow rises and gives him his
life back. The big potion: room 29 (row 1 col 6), on the way.

- Room 20's gate (row 1 col 9, at room 3's left end) is opened by room 5's plate (row 1 col 7), under room 3's loose
  floor (row 1 col 7): dropped, it falls onto the plate.
- Room 10's plate (row 1 col 0, after its 2-tile hole) opens room 19's gate (row 1 col 1).
- Room 19: from room 11's left end (row 1 col 0) up onto its top row's right end (cols 8..9; col 7 a loose floor);
  the middle row is wall there (cols 8..9), open over the shaft (cols 3..7, down into rooms 22 27 30 25), its left end
  cols 0..1 (the gate), col 2 a pillar's top over the bottom row's floor.
- Holes in the middle row: room 9 col 6, room 10 cols 1..2, room 11 col 1, room 13 cols 2..4 (col 5 a loose floor),
  room 4 cols 3..5. Cutter: room 20 cols 3..4. Guards appear in rooms 10, 2 and 4.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

RUN = "--run" in sys.argv
H0 = 4 if RUN else 6   # max HP at level 13's start (level 11's potion); the final run (--run) comes with the 4 it has
LOOSE3 = ("Tile[3][18]", "!=", 0x0B)          # room 3's loose floor (row 1 col 7) gone (falling, onto room 5's plate)
GATE20 = ("Tile Opening[20][20]", ">", 0)    # room 20's gate (row 1 col 9) opening
GATE19 = ("Tile Opening[19][12]", ">", 0)    # room 19's gate (row 1 col 1) opening (room 10's plate)
PAST20 = ("Player Pos X", "<", col_x(8))    # ... and past it
EXIT = ("Tile Opening[28][17]", ">", 0)      # room 28's exit door (row 1 cols 5..6) opening (room 4's plate)


def way(*c):
    """3 20 9 10 (its plate) 11 19 32 2 28 13 -> 29"""
    return [
        # 3: right onto the loose floor (col 7): it falls onto room 5's plate, room 20's gate opens; back left, through it
        # (straight left the search dropped the same floor from afar, by three jumps' knocks: 140 ticks against 100)
        step(room(3, *c), (3, col_x(7), ROW_Y[1], 1.0, 1.0)),
        # (room 20 counts once he is past its gate, x < col 8; the ticks in it before that score as room 3 goes on,
        # x 120 there: left as a room ahead they scored 5000 less and a full database drops them)
        step(room(3, *c, LOOSE3), (3, L, ROW_Y[1], 1.0, 1.0), (20, 120, ROW_Y[1], 1.0, 1.0, "keep")),
        step(room(20, *c, GATE20, PAST20), (20, L, ROW_Y[1], 1.0, 1.0)),
        step(room(9, *c), (9, L, ROW_Y[1], 1.0, 1.0)),
        step(room(10, *c), (10, L, ROW_Y[1], 1.0, 1.0)),
        # 11: to its left end, up onto room 19's top row
        step(room(11, *c, GATE19), (11, col_x(0), U, 1.0, 1.0)),
        # 19: left along the top row, over the shaft to the middle row's left end, through the gate
        step(room(19, *c, GATE19), (19, L, None, 1.0, 1.0)),
        step(room(32, *c), (32, L, ROW_Y[1], 1.0, 1.0)),
        step(room(2, *c), (2, L, ROW_Y[1], 1.0, 1.0)),
        step(room(28, *c), (28, L, ROW_Y[1], 1.0, 1.0)),
        step(room(13, *c), (13, L, ROW_Y[1], 1.0, 1.0)),
    ]


def back(*c):
    """4 (its plate: the exit doors) 29 13 28 (the door)"""
    return [
        step(room(4, *c), (4, col_x(2) + 12, ROW_Y[1], 1.0, 1.0)),
        step(room(4, *c, EXIT), (4, R, ROW_Y[1], 1.0, 1.0)),
        step(room(29, *c, EXIT), (29, R, ROW_Y[1], 1.0, 1.0)),
        step(room(13, *c, EXIT), (13, R, ROW_Y[1], 1.0, 1.0)),
        step(room(28, *c, EXIT), (28, col_x(6), ROW_Y[1], 1.0, 1.0)),
        # into the door (Up by it: sequence 0x46)
        step(room(28, *c, EXIT, ("Player Current Sequence", "==", 0x46)), (28, col_x(6) + 31, ROW_Y[1] - 26, 1.0, 1.0)),
    ]


ROUTES = {
    "np": way() + [step(room(29), (29, L, ROW_Y[1], 1.0, 1.0))] + back(),
    # room 29's potion (row 1 col 6) on the way in
    "pot": way(hp(H0)) + [
        step(room(29, hp(H0)), (29, col_x(6), ROW_Y[1], 1.0, 1.0)),
        step(room(29, hp(H0 + 1)), (29, L, ROW_Y[1], 1.0, 1.0)),
    ] + back(hp(H0 + 1)),
}
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"np": H0, "pot": H0 + 1}

# a potion is drunk (sequence 0x4E) only in room 29, on the potion route
FAILS = {"np": [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 13)]],
         "pot": [[("Player Current Sequence", "==", 0x4E), ("Player Room", "!=", 29), ("Current Level", "==", 13)]]}

# the way back stays on the middle row: room 13's pit (cols 2..5 once its loose floor fell) is jumped and its far ledge
# caught; landing on the bottom row instead (cols 6..9 there, and on into room 28's bottom row) is a dead end under
# the exit door (the first search sat there for 600 ticks)
for k in FAILS:
    FAILS[k].append([("Player Room", "==", 28), ("Player Current Row", "==", 2), ("Current Level", "==", 13)])
    FAILS[k].append([("Player Room", "==", 13), ("Player Current Row", "==", 2), ("Player Frame", "==", 15), ("Current Level", "==", 13)])
    for seq in (17, 49):   # (landed there: the soft landing, getting up)
        FAILS[k].append([("Player Room", "==", 13), ("Player Current Row", "==", 2), ("Player Current Sequence", "==", seq), ("Current Level", "==", 13)])

LEVEL = {
    "name": "l13", "next": 14, "init": "level010203040506070809101112.sol", "routes": ROUTES, "maxhp": MAXHP,
    # the gates, the exit doors, the loose floors on the way, room 5's and room 4's plates
    "hash": [[20, 20], [19, 12], [28, 17], [32, 13], [32, 16], [3, 18], [5, 18], [11, 15], [19, 8], [13, 16], [4, 13]],
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {} if RUN else {"Max HP At Level": [11, 5]},   # (level 11 replays as searched: 5, its potion 6)
}

if __name__ == "__main__":
    main(LEVEL)
