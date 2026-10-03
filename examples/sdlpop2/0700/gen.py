#!/usr/bin/env python3
"""Level 7 scripts, one per route (Sergio's six): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 7 (the temple), rooms:   .  . 25 26 27  .  .  .
                               .  . 19 14 20 28 16  .
                              32  . 18 13 21  8  2  .
                              31  7 17 12  6  3 29  .
                              30 22 11 10  5  1  4 15
                               .  .  . 24  9  .  .  .
                               .  .  .  . 23  .  .  .
Level 6's left door starts level 7 in room 32 (level010203040506L.sol, the "r1" routes), its right door in room 4
(level010203040506R.sol, the "r2" routes). The exit: room 21's door (row 1 cols 7..8), opened by room 8's plate (row 1
col 6). Plates: room 22's (row 1 col 7) opens room 11's gate (row 0 col 0); room 17's (row 2 col 4) room 12's gate
(row 2 col 1); room 1's (row 2 col 8) room 1's gate (row 1 col 3), which its closer (row 1 col 1) shuts. Big potions:
room 7 (row 1 col 8) and room 24 (row 0 col 3). Heads (charid 7): rooms 7, 9, 10, 20, 22, 24; charid 8: rooms 2, 22;
charid 11 (beast.c): rooms 3, 20, 29, 31.

The solutions (../ROUTING.md has the table), each after its start (level010203040506L.sol for r1*, ...R.sol for r2*):
ROUTE-to2.sol (to standing on room 2's bottom row, where every route meets: --upto the route's room 2 step) then
ROUTE-end.sol (the common ending), for r1, r2, r1-via7, r1-pot, r2-pot, r1-pot2.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

# openings (the modifiers' low bytes: level 7's carry flags above them)
EXIT = ("Tile Opening[21][19]", ">", 0)    # room 21's exit door (row 1 col 8)
GATE11 = ("Tile Opening[11][1]", ">", 0)   # room 11's gate (row 0 col 0)
GATE12 = ("Tile Opening[12][22]", ">", 0)  # room 12's gate (row 2 col 1)
GATE1 = ("Tile Opening[1][14]", ">", 0)    # room 1's gate (row 1 col 3)
KILL2 = 100   # Opponent HP Magnet in room 2 (per hit point its head (charid 8, 6 HP) lost)


def tail():
    """17 (its plate opens room 12's gate) -> 12 (through the gate, over the hole, up to the middle row) -> 6 (up to the
    top row) -> 3 (the top row: blades at cols 7..8) -> 29 -> up into 2 -> the canonical way round (room 2's left wall
    cannot be clipped: its head's push from the middle row needs a climb its bites kill): 2 (the head killed: it is on
    him within ~17 ticks of his entry and a bite mid-climb is fatal; up the col 0 pillars) -> 16 -> 28 (their middle
    rows, left) -> 20 (its middle row's gap at col 7 (0x0F) and the hole below) -> 21 (he lands by the exit door) -> 8
    (its middle row, blades at cols 2..3; its plate (col 6) opens the exit) -> 21 (the door, row 1 cols 7..8)"""
    return [
        step(room(17), (17, col_x(4), D, 1.0, 1.0)),
        step(room(17, GATE12), (17, R, D, 1.0, 1.0)),
        # room 12: across its hole (cols 2..5, down to room 10) to col 6: the running jump from room 17's floor falls
        # short, Shift (and Up) catches col 6's edge as he drops into room 10's top (x ~340) and he climbs onto it (as
        # level 6's potion); then up its pillar (col 7) to the middle row, right into 6
        step(room(12), (12, col_x(6), ROW_Y[2], 1.0, 1.0), (10, 337, -100, 1.0, 1.0, "keep")),
        step(room(12, row(2), ("Player Current Col", ">=", 6)), (12, col_x(7), U, 1.0, 2.0)),
        step(room(12, row(1)), (12, R, ROW_Y[1], 1.0, 1.0)),
        step(room(6), (6, R, U, 1.0, 1.0)),
        step(room(3), (3, R, ROW_Y[0], 1.0, 1.0)),
        # (he comes out of room 3's wall in sword stance: sheathe and jump up at once (two climbs); a stronger pull to
        # col 1 kept the sword-stance steps toward it over the sheathing, ~50 ticks lost)
        step(room(29), (29, col_x(1), U, 0.2, 2.0)),
        step(room(2), (2, 184, U, 1.0, 2.0), opp=KILL2),   # (the climbs start at col 1 facing left: col 0 has no ledge above)
        step(room(16), (16, L, ROW_Y[1], 1.0, 1.0), opp=0.0),
        step(room(28), (28, L, ROW_Y[1], 1.0, 1.0)),
        step(room(20), (20, col_x(7), D, 1.0, 1.0)),
        step(room(21), (21, R, ROW_Y[1], 1.0, 1.0)),
        step(room(8), (8, col_x(6), ROW_Y[1], 1.0, 1.0)),
        step(room(8, EXIT), (8, L, ROW_Y[1], 1.0, 1.0)),
        step(room(21, EXIT), (21, col_x(7) + 16, ROW_Y[1], 1.0, 1.0)),
        step(room(21, ("Player Current Sequence", "==", 0x46))),
    ]


def from32():
    """32 (down its hole, row 2 col 2) -> 31: off its top row's right end at col 6 down to its floor (a hit point; col
    5's 0x0E is no floor: falling there goes on into room 30, fatal), left past the charger (charid 11, col 2), down at
    col 0 -> 30 (its middle row, right) -> 22"""
    return [
        step(room(32), (32, col_x(2), D, 1.0, 1.0)),
        step(room(31), (31, col_x(6), D, 1.0, 1.0)),
        step(room(31, row(2)), (31, L, D, 1.0, 1.0)),
        step(room(30), (30, R, ROW_Y[1], 1.0, 1.0)),
    ]


def plate22():
    """22: its plate (row 1 col 7) opens room 11's gate; up to its top row (cols 8..9), right through the gate into 11,
    up its top row's gap (col 4) into 17"""
    return [
        step(room(22), (22, col_x(7), ROW_Y[1], 1.0, 1.0)),
        step(room(22, GATE11), (22, R, U, 1.0, 1.0)),
        step(room(11), (11, col_x(4), U, 1.0, 1.0)),
    ]


def potion7(n):
    """22 up into 7 through two loose floors: jumping up under 22's top-row loose floor (col 2) drops it, up onto the top
    row's left end (col 1); there, under room 7's loose floor (its bottom row, col 1), the same, and up through that
    hole into 7 (nothing else leads up: the floor above 22's top-row right end is wall, cols 5..7 have no edge to
    grab); the potion (row 1 col 8), then the head (charid 7) pushes him through the right wall into 17"""
    return [
        step(room(22, hp(n)), (22, col_x(2), U, 1.0, 2.0)),
        step(room(22, hp(n), row(0)), (22, 210, U, 1.0, 2.0)),   # (the jump up that catches room 7's floor: x ~201..210)
        step(room(7, hp(n)), (7, col_x(8), ROW_Y[1], 1.0, 1.0)),
        step(room(7, hp(n + 1)), (7, R, ROW_Y[1], 1.0, 1.0)),
    ]


def potion24(n):
    """5's bottom row: down a gap (0x0F, col 4 or 6) into 9's top row, left into 24, the potion (row 0 col 3), back
    through 9 up into 5, right along 5's bottom row (the low tunnel, 0x07) into 1, its plate (row 2 col 8) opens its
    gate (row 1 col 3): up to the middle row, left through the gate into 5's middle row"""
    return [
        step(room(5, hp(n)), (5, col_x(4), D, 1.0, 1.0)),
        step(room(9, hp(n)), (9, L, ROW_Y[0], 1.0, 1.0)),
        step(room(24, hp(n)), (24, col_x(3), ROW_Y[0], 1.0, 1.0)),
        step(room(24, hp(n + 1)), (24, R, ROW_Y[0], 1.0, 1.0)),
        step(room(9, hp(n + 1)), (9, col_x(5), U, 1.0, 1.0)),
        step(room(5, hp(n + 1)), (5, R, D, 1.0, 1.0)),
        step(room(1, hp(n + 1)), (1, col_x(8), D, 1.0, 1.0)),
        step(room(1, hp(n + 1), GATE1), (1, L, ROW_Y[1], 1.0, 1.0)),
    ]


def west(n):
    """5's middle row (blades at cols 2..3) left -> 10 (down to its floor) -> 11's bottom-right corner (cols 8..9; walls
    left of it): up to its middle row there, left along it to col 4, up its top row's gap into 17"""
    return [
        step(room(5, hp(n), row(1)), (5, L, ROW_Y[1], 1.0, 1.0)),
        step(room(10, hp(n)), (10, L, ROW_Y[1], 1.0, 1.0)),
        step(room(11, hp(n)), (11, col_x(9), ROW_Y[1], 1.0, 2.0)),
        step(room(11, hp(n), row(1)), (11, col_x(4), U, 1.0, 1.0)),
    ]


ROUTES = {
    # 32 31 30 22 (plate) 11 17 12 6 3 29 2 (the push) 8 21
    "r1": from32() + plate22() + tail(),
    # 32 31 30 22 7 (the push, no potion) 17 12 6 3 29 2 8 21
    "r1-via7": from32() + potion7(3)[:3] + [step(room(7), (7, R, ROW_Y[1], 1.0, 1.0))] + tail(),
    # 32 31 30 22 7 (potion; the push) 17 12 6 3 29 2 8 21
    "r1-pot": from32() + potion7(3) + tail(),
    # 32 31 30 22 7 (potion; the push) 17 11 10 (the head's push through the bottom row's right wall) 5 9 24 (potion)
    # 9 5 1 (the gate) 5 10 11 17 12 6 3 29 2 8 21
    "r1-pot2": from32() + potion7(3) + [
        step(room(17, hp(4)), (17, col_x(3), D, 1.0, 1.0)),
        step(room(11, hp(4)), (11, R, ROW_Y[1], 1.0, 1.0)),
        step(room(10, hp(4)), (10, R, ROW_Y[2], 1.0, 1.0)),
    ] + potion24(4) + west(5) + tail(),
    # 4 1 (the plate, the gate) 5 10 11 17 12 6 3 29 2 8 21
    "r2": [
        step(room(4), (4, L, D, 1.0, 1.0)),
        step(room(1), (1, col_x(8), D, 1.0, 1.0)),
        step(room(1, GATE1), (1, L, ROW_Y[1], 1.0, 1.0)),
    ] + west(3) + tail(),
    # 4 1 (its bottom row) 5 9 24 (potion) 9 5 1 (the gate) 5 10 11 17 12 6 3 29 2 8 21
    "r2-pot": [
        step(room(4), (4, L, D, 1.0, 1.0)),
        step(room(1, hp(3)), (1, L, D, 1.0, 1.0)),
    ] + potion24(3) + west(4) + tail(),
    # 4 1 5 9 24 (potion) 9 5 1 (the gate) 5 10 (the head follows) 11 (it pushes him through the left wall or the gate)
    # 22 7 (potion; the push) 17 12 6 3 29 2 8 21
    "r2-pot2": [
        step(room(4), (4, L, D, 1.0, 1.0)),
        step(room(1, hp(3)), (1, L, D, 1.0, 1.0)),
    ] + potion24(3) + [
        step(room(5, hp(4), row(1)), (5, L, ROW_Y[1], 1.0, 1.0)),
        step(room(10, hp(4)), (10, L, ROW_Y[1], 1.0, 1.0)),
        step(room(11, hp(4)), (11, L, ROW_Y[1], 1.0, 1.0)),
    ] + potion7(4) + tail(),
}
# the Player HP Magnet from the start: room 10's head follows him into room 11 and bites; room 2's head, the blades
HPM = 200
# probe (Sergio): room 10's bottom row into room 5 by the sword alone, no head bite (no hit point lost) (--upto 2)
ROUTES["probe-10to5"] = [step(room(10), (10, R, ROW_Y[2], 1.0, 1.0)), step(room(5, row(2)))]
# probe: room 29 (from route 1's arrival at its middle row) up into room 2, standing (--upto 2)
ROUTES["probe-29to2"] = [step(room(29), (29, col_x(1), U, 1.0, 2.0)), step(room(2))]
# probe: route 2 + both: from room 11's top row (after room 24's potion), through its gate (col 0, shut: it opens
# from room 22's plate) into room 22's top row, by the head's push (--upto 2)
ROUTES["probe-11to22"] = [step(room(11), (11, L, ROW_Y[0], 1.0, 1.0)), step(room(22))]
# probe: the same from room 10 (room 10's head following him into 11 and up to its top row) (--upto 5)
# (the Opponent Pos X / Y Magnets draw the head up through the top row's gap (col 4) and to his right on the top row:
# head y ~37 / 100 / 163 on rows 0 / 1 / 2)
ROUTES["probe-10to22"] = [step(room(10), (10, L, ROW_Y[2], 1.0, 1.0)), step(room(11), (11, col_x(9), ROW_Y[1], 1.0, 2.0)),
                          step(room(11, row(1)), (11, col_x(4), U, 1.0, 1.0), oppx=(0.5, 290), oppy=(0.5, 37)),
                          step(room(11, row(0)), (11, L, ROW_Y[0], 1.0, 1.0), oppx=(1.0, 230), oppy=(1.0, 37)),
                          step(room(22))]
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
# inside walls, where clips lead nowhere: room 11's bottom row (cols 0..7); rooms 8 and 21's bottom rows (all wall)
FAILS = {k: [[("Player Room", "==", 11), ("Player Current Row", "==", 2), ("Player Current Col", "<=", 7)],
             [("Player Room", "==", 8), ("Player Current Row", "==", 2)],
             [("Player Room", "==", 21), ("Player Current Row", "==", 2)]] for k in ROUTES}
FAILS["probe-10to5"].append([("Player Current HP", "<", 4)])
# route 2 + both: room 10's head knocked into room 11's bottom-row walls (cols 0..7) freezes there for good
FAILS["probe-10to22"].append([("Opponent Room", "==", 11), ("Opponent Pos Y", ">=", 150), ("Opponent Pos X", "<=", 400)])
MAXHP = {"probe-29to2": 3, "probe-10to22": 4, "probe-11to22": 4, "probe-10to5": 4, "r1": 3, "r1-via7": 3, "r1-pot": 4, "r1-pot2": 5, "r2": 3, "r2-pot": 4, "r2-pot2": 5}

LEVEL = {
    "name": "l7", "next": 8, "init": "level010203040506L.sol", "routes": ROUTES, "maxhp": MAXHP,
    # the exit door, the gates, the loose floors on the routes (their shaking)
    "hash": [[21, 18], [21, 19], [11, 1], [12, 22], [1, 14], [5, 23], [5, 24], [9, 5], [22, 3], [7, 22], [17, 23],
             [30, 4], [30, 6]],
    "fails": FAILS,
    "penalize_ahead": True,
}

if __name__ == "__main__":
    main(LEVEL)
