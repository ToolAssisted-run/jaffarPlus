#!/usr/bin/env python3
"""Level 5 scripts, one per route: see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 5 (rooms, left to right): 22 1 23 2 9 3 4 7 10 12 on top (21 above 12); 5 6 11 18 below 4 7 10 12; 24 8 25 20 17;
13 14 15 16; 19 below 13. Big potions: room 22 (row 2 col 4, behind the gate at row 2 col 9, whose plates are room
22's at row 1 col 6 and room 9's at row 2 col 6 under a loose floor) and room 6 (row 0 col 5). The exit: the hatch
("mouth", room 3 row 0 col 6), toggled by the plate at room 3 row 0 col 8, then crouching on the trap at room 3 row 2
col 3, which lifts the prince out through it (lever5.c). The rope bridge: rooms 7 | 10 | 12, row 1.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

MOUTH = ("Tile Modifier[3][7]", "==", 3)                # the hatch open (0 shut .. 3 open)
# crouched on the trap: lifted (seq 0x80); he rises through the open hatch in ~43 ticks and the level ends. The magnet
# keeps the rising states ahead of the others (at the trap's x 268, upwards)
LIFT = step([("Player Room", "==", 3), ("Player Current Sequence", "==", 0x80)], (3, 268, -200, 1.0, 4.0))


def finish():
    """room 3's top row (the hatch's plate at col 8), then the trap (row 2 col 3)"""
    return [
        step(room(3, row(0)), (3, col_x(8), U, 1.0, 2.0)),
        step(room(3, MOUTH), (3, 268, D, 1.0, 1.0)),
        # crouched (frame 109) on the trap's column: the lift starts unless the crouch was the plain stoop (sequence 0x11 or
        # 0x14; control.c control_crouched): a turn, then Down, does it
        step(room(3, MOUTH, ("Player Frame", "==", 109), ("Player Current Col", ">=", 3), ("Player Current Col", "<=", 4)), (3, 268, D, 1.0, 1.0)),
        LIFT,
    ]


LOOSE9 = ("Tile[9][17]", "==", 0)              # room 9's loose floor (row 1 col 6) fell onto the plate below: room 22's gate stays open
LOOSE2 = ("Tile[2][13]", "==", 0)              # room 2's loose floor (row 1 col 2) knocked down: falling onto the plate below
GATE2 = ("Tile Modifier[2][18]", "==", 0xFF)   # room 2's gate at row 1 col 7 jammed open: the loose floor above its plate (row 2 col 2) fell on it
GATE11 = ("Tile Modifier[11][10]", ">", 0)     # room 11's gate (row 0 col 9), held by its plate (row 0 col 2) or room 20's (row 0 col 7)


def to3(hpn):
    """1 -> 23 -> 2 -> 9 -> 3 (the top row's rooms, going right)"""
    return [
        step(room(1, hp(hpn)), (1, R, None)),
        step(room(23), (23, R, None)),
        step(room(2), (2, col_x(2), D, 1.0, 1.0)),          # under the loose floor: bump it down onto the plate
        # it falling: up at col 3 at once (the pull down, kept until the gate jammed, had him crouch ~20 ticks too long)
        step(room(2, LOOSE2), (2, col_x(3), U, 1.0, 1.0)),
        step(room(2, GATE2), (2, R, U, 1.0, 1.0)),
        step(room(9), (9, R, None)),
        step(room(3), (3, R, None)),
    ]


def potion1():
    """1 -> 23 -> 2 -> 9 (its loose floor onto the plate) -> 2 -> 23 -> 1 -> 22 (the potion) -> 1 ... 3"""
    back = lambda r: (r, L, None, 1.0, 1.0, "keep")
    return [
        step(room(1, hp(3)), (1, R, None)),
        step(room(23), (23, R, None)),
        step(room(2), (2, col_x(2), D, 1.0, 1.0)),
        step(room(2, LOOSE2), (2, col_x(3), U, 1.0, 1.0)),
        step(room(2, GATE2), (2, R, U, 1.0, 1.0)),
        step(room(9), (9, col_x(6), ROW_Y[1], 1.0, 1.0)),
        step(room(9, LOOSE9), (9, L, None, 1.0, 1.0, "keep"), back(2), back(23), back(1), back(22)),
        step(room(2, LOOSE9), (2, L, None), back(23), back(1), back(22)),
        step(room(23, LOOSE9), (23, L, None), back(1), back(22)),
        step(room(1, LOOSE9), (1, L, None), back(22)),
        # room 22: walls on row 2 (cols 5..6) keep the potion (row 2 col 4) from the gate: up to row 1 at col 6, over the
        # loose floors (cols 3..4: col 4's falling on the potion breaks it) to col 2, down at col 1, then the potion from
        # the left; back up at col 5
        step(room(22, LOOSE9), (22, col_x(6), U, 1.0, 2.0)),
        step(room(22, LOOSE9, row(1)), (22, col_x(1), D, 1.0, 1.0)),
        step(room(22, LOOSE9, row(2), ("Player Current Col", "<=", 3)), (22, col_x(3), D, 1.0, 1.0)),
        step(room(22, hp(4)), (22, col_x(5), U, 1.0, 2.0)),
        step(room(22, hp(4), row(1)), (22, R, D, 1.0, 1.0)),
        step(room(1, hp(4)), (1, R, None)),
        step(room(23, hp(4)), (23, R, None)),
        step(room(2, hp(4)), (2, R, None)),
        step(room(9, hp(4)), (9, R, None)),
        step(room(3, hp(4)), (3, R, None)),
    ]


# room 18 from below (row 2): onto the plate at row 1 col 5 (from col 4), up to row 0 col 7 (from col 6), then up into
# room 12's row 2 col 6
UP18 = [
    step(room(18), (18, col_x(4), U, 1.0, 2.0)),
    step(room(18, row(1)), (18, col_x(6), U, 1.0, 2.0)),
    step(room(18, row(0)), (18, col_x(7), U, 1.0, 2.0)),
]


def around(detour6):
    """3 -> 4 -> 5 -> 8 -> 13 -> 14 -> 15 -> 16 -> 17 -> 18 -> 12 -> 10 (the bridge) [-> down into 11 -> 6 (the potion)
    -> 11 (its plate at row 0 col 2 opens its gate at col 9) -> 18 (top row) -> 12 -> 10] -> 7 -> 4 (top row) -> 3 (top
    row: the hatch) -> the carpet"""
    out = [
        step(room(4), (4, col_x(4), D, 1.0, 1.0)),
        step(room(5), (5, None, D, 1.0, 1.0)),
        step(room(8), (8, None, D, 1.0, 1.0)),
        step(room(13), (13, R, D, 1.0, 1.0)),
        # room 14's small potion (row 2 col 6): climbing onto room 18's plate (row 1 col 5) costs a hit point
        step(room(14), (14, col_x(6), D, 1.0, 1.0)),
        step(room(14, ("Player Current HP", ">=", 2)), (14, R, None)),
        step(room(15), (15, R, None)),
        step(room(16), (16, None, U, 1.0, 1.0)),
        step(room(17), (17, None, U, 1.0, 1.0)),
    ] + UP18 + [
        step(room(12), (12, L, ROW_Y[1], 1.0, 1.0)),
    ]
    if detour6:
        out += [
            # the bridge collapses (bridge5.c) once the prince (cols 4..6) and the skeleton (cols 5..6) stood together on it
            # for 60 ticks: then down through the gap into room 11
            # for 60 ticks: then down through the gap into room 11. From the right, he holds col 6 and lets the skeleton
            # come to col 5
            step(room(10), (10, col_x(6), ROW_Y[1], 1.0, 1.0)),
            step(room(10, ("Bridge Count", ">=", 20)), (10, col_x(6), ROW_Y[1], 1.0, 1.0)),
            step(room(10, ("Bridge Count", ">=", 40)), (10, col_x(6), ROW_Y[1], 1.0, 1.0)),
            step(room(10, ("Bridge Count", ">=", 60), ("Opponent Current Col", ">=", 5)), (10, col_x(6), ROW_Y[1], 1.0, 1.0)),
            step(room(10, ("Bridge State", "<", 0)), (10, col_x(5), D, 1.0, 1.0)),
            step(room(11, hp(3)), (11, L, U, 1.0, 1.0)),
            step(room(6, hp(3)), (6, col_x(5), U, 1.0, 1.0)),
            step(room(6, hp(4)), (6, R, U, 1.0, 1.0)),
            step(room(11, hp(4)), (11, col_x(2), U, 1.0, 2.0)),
            step(room(11, hp(4), GATE11), (11, R, U, 1.0, 1.0)),
            step(room(18, hp(4)), (18, col_x(7), U, 1.0, 2.0)),
            step(room(12, hp(4)), (12, L, ROW_Y[1], 1.0, 1.0)),
        ]
    out += [
        step(room(10), (10, L, ROW_Y[1], 1.0, 1.0)),
        step(room(7), (7, col_x(1), U, 1.0, 2.0)),
        step(room(7, row(0)), (7, L, U, 1.0, 1.0)),
        step(room(4, row(0)), (4, L, U, 1.0, 1.0)),
    ] + finish()
    return out


ROUTES = {
    "none": dedup(to3(3) + around(False)),
    "p1": dedup(potion1() + around(False)),
    "p2": dedup(to3(3) + around(True)),
    "both": dedup(potion1() + around(True)),
}
MAXHP = {"none": 3, "p1": 4, "p2": 4, "both": 5}

POTION22_BROKEN = [("Tile[22][25]", "==", 0x0E), ("Player Max HP", "==", 3)]   # a loose floor fell on room 22's potion

LEVEL = {
    "name": "l5", "next": 6, "init": "level01020304.sol", "routes": ROUTES, "maxhp": MAXHP,
    # the hatch, the gates (22: 30; 2: 16, 18; 7: 14; 11: 10; 14: 13; 17: 1; 20: 1)
    "hash": [[3, 7], [22, 30], [2, 16], [2, 18], [7, 14], [11, 10], [14, 13], [17, 1], [20, 1]],
    "fails": {"p1": [POTION22_BROKEN, [("Player Room", "==", 6), ("Player Max HP", "==", 4)]],    # (potion 1 only: not room 6's)
              "p2": [[("Player Room", "==", 22), ("Player Max HP", "==", 4)]],                     # (potion 2 only: not room 22's)
              "both": [POTION22_BROKEN]},
}

if __name__ == "__main__":
    main(LEVEL)
