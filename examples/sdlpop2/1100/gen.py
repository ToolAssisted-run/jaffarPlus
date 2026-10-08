#!/usr/bin/env python3
"""Level 11 scripts (Sergio's two routes): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 11, rooms:   .  .  22 21 32  .
                   .  28 27 20 29  .
                   .  26 24 19  .  .
                   .  25 18 17  .  .
                  30  23 13  8 14  .
                   .  31  6  7 12 16
                   .  .  10  9  3 15
                   .  .   4  2  .  .
                   .  .  11  1  5  .
Level 10's route (level01020304050607080910.sol) starts level 11 in room 5 (row 2, x 298, facing left). Plates and
gates: room 1's top-left plate opens room 4's gate (row 2 col 9), its row 2 col 6 plate room 20's (row 2 col 0); room
17's plate (row 2 col 1) opens room 13's gates (row 1 cols 3 and 9; room 13's closer at row 1 col 8); room 24's plate
(row 2 col 9) and room 26's (row 0 col 2, row 1 col 3) open room 26's gate (row 1 col 9); room 29's plate (row 2
col 5) opens the exit door, room 24's (row 2 cols 4..5). Cutters in rooms 12 (row 0), 28 and 29 (row 2). The big
potion: room 3 (row 2 col 7), behind its gate (row 0 col 2: room 3's plates, row 1 cols 1 and 6, open it; its closers
at row 1 cols 3, 5) or a wall clip; small ones on the way in rooms 13 and 25 are never drunk.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

RUN = "--run" in sys.argv
H0 = 4 if RUN else 5   # max HP at level 11's start ("Max HP At Level", as levels 7 to 10); the final run (--run) comes
                       # with the 4 it has, no "Max HP At Level"
SEEN9 = ("Visited Room[9]", "==", 1)         # room 2 the second time: fallen into from room 9's jump
RIGHT9 = ("Player Pos X", ">", col_x(7))     # room 9's right side (cols 8..9), up from room 2's right ledge
LOOSE3 = ("Tile[3][2]", "!=", 0x0B)          # room 3's top-row loose floor (col 1) broken
SHAKE3 = ("Tile Opening[3][2]", ">", 0)      # ... or set shaking (bumped from below: its low byte counts up)
LOOSE_FIRST = "--loosefirst" in sys.argv       # (room 3: the loose floor before the potion)
if LOOSE_FIRST: sys.argv.remove("--loosefirst")
GATE6 = ("Tile Opening[6][20]", ">", 0)     # room 6's gate (row 1 col 9), opened from room 12's top row (col 4)
GATE26 = ("Tile Opening[26][20]", ">", 0)   # room 26's gate (row 1 col 9), opened from room 24's plate (row 2 col 9)
GATE20 = ("Tile Opening[20][21]", ">", 0)   # room 20's gate (row 2 col 0), opened from room 1's plate (row 2 col 6)
SHAKE22 = ("Tile Opening[22][26]", ">", 0)  # room 22's bottom-row loose floor at col 5 (room 27's ceiling) shaking
GATE13 = ("Tile Opening[13][14]", ">", 0)   # room 13's left gate (row 1 col 3), opened from room 17
SEEN25 = ("Visited Room[25]", "==", 1)       # room 18 the third time: from room 25 (its left, row 1)
EXIT = ("Tile Opening[24][26]", ">", 0)     # the exit door (room 24 row 2 cols 4..5), opened from room 29


def start(*c):
    """5 1 (up) 2 4 (up) 10 9 (a running jump right, short: down into room 2 from the top, onto its right ledge, col 8)
    2 (up its right side) 9 (its right side) -> 3 (Sergio)"""
    return [
        step(room(5, *c), (5, L, ROW_Y[2], 1.0, 1.0)),
        step(room(1, *c), (1, col_x(0), U, 1.0, 2.0)),
        step(room(2, *c), (2, L, ROW_Y[2], 1.0, 1.0)),
        # 4: its bottom row is wall but for cols 8..9 (the gate from room 2, opened by room 1's top-left plate): up its
        # right side (cols 7..8) into room 10 (the search also walks inside the bottom row's wall)
        step(room(4, *c), (4, col_x(7), U, 1.0, 2.0)),
        step(room(10, *c), (10, R, None, 1.0, 1.0)),
        step(room(9, *c), (9, R, None, 1.0, 1.0)),
        step(room(2, *c, SEEN9), (2, col_x(8), U, 1.0, 2.0)),
        step(room(9, *c, RIGHT9), (9, R, ROW_Y[1], 1.0, 1.0)),
    ]


def rest(*c, via19=False):
    """12 7 6 13 18 17 (its plate: room 13's gates) 18 13 23 25 18 (its left) 24 26 28 27 22 21 32 29 (its plate: the
    exit door) 20 27 28 26 24 (the door)"""
    return [
        # 12: up to its top row's plate (col 4, the cutter right of it): it opens room 6's gate (row 1 col 9), the way
        # from room 7 into room 6; then left along the top row into room 7
        step(room(12, *c), (12, col_x(4), U, 1.0, 2.0)),
        step(room(12, *c, GATE6), (12, L, ROW_Y[0], 1.0, 2.0)),
        step(room(7, *c, GATE6), (7, L, ROW_Y[1], 1.0, 1.0)),   # (the gate: else reaching room 7 marks the plate done)
        step(room(6, *c), (6, None, U, 1.0, 2.0)),
        step(room(13, *c), (13, None, U, 1.0, 2.0)),
        step(room(18, *c), (18, R, None, 1.0, 1.0)),
        step(room(17, *c), (17, col_x(1), ROW_Y[2], 1.0, 1.0)),
        # back before room 13's gate shuts again (~140 ticks after the plate): left along 17's and 18's bottom row, down
        # room 18's hole (cols 5..6) onto room 13's middle row, left through the gate (col 3)
        step(room(17, *c, GATE13), (17, L, ROW_Y[2], 1.0, 1.0)),
        step(room(18, *c, GATE13), (18, col_x(5) + 16, D, 1.0, 1.0)),
        step(room(13, *c, GATE13), (13, L, ROW_Y[1], 1.0, 1.0)),
        # 23: its guard (row 1 col 3; more from its spawn point at row 0 col 0) meets him: up at once at col 8 (by the
        # entry), the guard's hit points rewarded (hit or killed)
        step(room(23, *c), (23, col_x(8), U, 1.0, 2.0), opp=200),
        step(room(25, *c), (25, R, ROW_Y[1], 1.0, 1.0)),
        step(room(18, *c, SEEN25), (18, None, U, 1.0, 2.0)),
        # 24: right to its bottom row's plate (col 9: room 26's gate, row 1 col 9), back left, up onto its middle row (col
        # 0) and through the gate into room 26
        step(room(24, *c), (24, col_x(9), ROW_Y[2], 1.0, 1.0)),
        step(room(24, *c, GATE26), (24, L, ROW_Y[1], 1.0, 1.0)),
        step(room(26, *c, GATE26), (26, None, U, 1.0, 2.0)),
        step(room(28, *c), (28, R, ROW_Y[2], 1.0, 1.0)),
        # 27 (Sergio): straight up its pillars to the top row under the shaft (cols 4..5), its ceiling room 22's loose
        # floors: bumped from below they fall, then up through the shaft into room 22
        step(room(27, *c), (27, 338, U, 1.0, 2.0)),   # (x 338: the top row's edge by the shaft, where a jump bumps them)
        step(room(27, *c, SHAKE22), (27, col_x(5), U, 1.0, 2.0)),
        step(room(22, *c), (22, R, ROW_Y[2], 1.0, 1.0)),
        step(room(21, *c), (21, R, ROW_Y[2], 1.0, 1.0)),
        step(room(32, *c), (32, None, D, 1.0, 1.0)),
        # 29: its top row opens down only at col 7 (a loose floor): right, down onto the bottom row, left onto the plate
        # (col 5: the exit door)
        step(room(29, *c), (29, col_x(7), D, 1.0, 1.0)),
        step(room(29, *c, row(2)), (29, col_x(5), ROW_Y[2], 1.0, 1.0)),
        step(room(29, *c, EXIT), (29, L, ROW_Y[2], 1.0, 1.0)),
    ] + ([
        # (a test, not Sergio's route: room 20's bottom row has a 4-column gap, cols 2..5, and no ledge above it) down
        # the gap into room 19, onto its one floor (row 2 col 0, a ledge to catch), left into room 24 by its plate
        step(room(20, *c, EXIT), (20, col_x(2), D, 1.0, 1.0)),
        step(room(19, *c, EXIT), (19, col_x(0), ROW_Y[2], 1.0, 1.0)),
    ] if via19 else [
        # 20 (Sergio): a running jump left across the bottom row's gap (cols 2..5) onto its left side (col 1: hanging,
        # climbing); col 6's loose floor, run over, falls all the way down onto room 1's plate (row 2 col 6), which opens
        # room 20's gate (row 2 col 0): wait there for it, then left
        # (the hang under its floor's edge counts as room 19, below: not penalized there, drawn up)
        # (its magnet just above the hang, y 30: hanging and climbing score what the jump did, not 68 less)
        step(room(20, *c, EXIT), (20, col_x(1), ROW_Y[2], 1.0, 1.0), (19, col_x(1), 30, 1.0, 1.0, "keep")),
        step(room(20, *c, EXIT, ("Player Pos X", "<", col_x(2)), row(2)), (20, col_x(1), ROW_Y[2], 1.0, 1.0),
             (19, col_x(1), 30, 1.0, 1.0, "keep")),
        step(room(20, *c, EXIT, GATE20), (20, L, ROW_Y[2], 1.0, 1.0)),
        step(room(27, *c, EXIT), (27, L, ROW_Y[2], 1.0, 1.0)),
        step(room(28, *c, EXIT), (28, None, D, 1.0, 1.0)),
        step(room(26, *c, EXIT), (26, R, ROW_Y[1], 1.0, 1.0)),
    ]) + [
        step(room(24, *c, EXIT), (24, col_x(5), ROW_Y[2], 1.0, 1.0)),
        # into the door (Up by it: sequence 0x46): its frames take him inside it (x ~col 5 + 31, y 26 above the floor)
        step(room(24, *c, EXIT, ("Player Current Sequence", "==", 0x46)), (24, col_x(5) + 31, ROW_Y[2] - 26, 1.0, 1.0)),
    ]


ROUTES = {
    # 5 1 2 4 10 9 3 (up the left) 12 ...
    # (room 3 without its potion: under the loose floor, col 1, knocked down from below; then up at col 0. With the
    # magnet at col 0 from the start the search jumped there for 138 ticks and more)
    "np": start() + [step(room(3), (3, col_x(1), U, 1.0, 2.0)), step(room(3, LOOSE3), (3, col_x(0), U, 1.0, 2.0))] + rest(),
    # ... 3 (right, to the potion, row 2 col 7; then back up into room 12) 12 ...
    "pot": start() + [
        # 3: right to the potion (through row 1's wall at col 2: a wall clip), then (Sergio) back left the same way, under
        # the top row's loose floor (col 1): broken, it falls on the plate below and he climbs up into room 12
    ] + ([
        # (Sergio, 2026-10-05) first under the loose floor: bumped from below it starts shaking; right to the potion
        # (the wall clip) while it falls; back, up at col 0
        step(room(3, hp(H0)), (3, col_x(1), U, 1.0, 2.0)),
        step(room(3, hp(H0), SHAKE3), (3, col_x(7), ROW_Y[2], 1.0, 1.0)),
        step(room(3, hp(H0 + 1)), (3, col_x(1), ROW_Y[1], 1.0, 1.0)),
        step(room(3, hp(H0 + 1), LOOSE3), (3, col_x(0), U, 1.0, 2.0)),
    ] if LOOSE_FIRST else [
        step(room(3, hp(H0)), (3, col_x(7), ROW_Y[2], 1.0, 1.0)),
        step(room(3, hp(H0 + 1)), (3, col_x(1), ROW_Y[1], 1.0, 1.0)),
        step(room(3, hp(H0 + 1), LOOSE3), (3, col_x(0), U, 1.0, 2.0)),
    ]) + [
    ] + rest(),
}
ROUTES["pot19"] = ROUTES["pot"][:-len(rest())] + rest(via19=True)
HPM = 50   # a small Player HP Magnet: ties only (level 7: without it the search wore down to 1 hit point and waited)
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"np": H0, "pot": H0 + 1, "pot19": H0 + 1}

# the small potions (+1 hit point; rooms 13 and 25 on the way) are never drunk: a drink (sequence 0x4E) fails, but for
# room 3's on the potion route
FAILS = {"np": [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 11)]],
         "pot": [[("Player Current Sequence", "==", 0x4E), ("Player Room", "!=", 3), ("Current Level", "==", 11)]]}
# (down room 20's gap past its ledge: through room 19 into room 17 once the exit door opens; the catch itself dips to
# y ~205 in room 20, then hangs in room 19)
for k in ("np", "pot"): FAILS[k].append([("Player Room", "==", 17), EXIT, ("Current Level", "==", 11)])
FAILS["pot19"] = FAILS["pot"][:-1]

LEVEL = {
    "name": "l11", "next": 12, "init": "level01020304050607080910.sol", "routes": ROUTES, "maxhp": MAXHP,
    # the gates and the exit door
    "hash": [[4, 10], [4, 30], [10, 30], [6, 20], [3, 3], [3, 19], [13, 14], [13, 20], [18, 11], [26, 20], [20, 21],
             [29, 10], [24, 25], [24, 26], [3, 2], [22, 25], [22, 26]],   # (and the loose floors broken on the way)
    "fails": FAILS,
    "penalize_ahead": True,
    "hash_visited": [9, 25],   # (rules read them)
    "emulator": {} if RUN else {"Max HP At Level": [11, H0]},
}

if __name__ == "__main__":
    main(LEVEL)
