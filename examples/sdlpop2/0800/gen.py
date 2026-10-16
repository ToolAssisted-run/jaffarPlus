#!/usr/bin/env python3
"""Level 8 scripts, one per route (Sergio's two): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 8, rooms:   .  .  .  .  .  .  .  .  .  . 15  .
                  .  .  .  .  .  .  .  .  .  7 13 16
                 26 25 23 24  1  9 21 19 10  5  4  8
                  .  .  .  .  . 11 22 20 12  2  3 14
                  .  .  .  .  .  .  .  .  6 17 18  .
Level 7's route (level01020304050607.sol: its room 31 wall clip, no potion) starts level 8 in room 16 (row 1, x 296).
The heirloom sword: room 9 (row 2 col 5): the first pickup plays the sword scene (sequence 0xED, then story scene 6
and the level reloaded, ruins.c sword_scene), the second takes it. The exit: room 23's door (row 2 cols 1..2),
opened by the plate at its col 9. The big potion: room 18 (row 2 col 8). Room 17: gates at col 3 (rows 1 and 2;
plates at row 1 col 8 and row 2 col 2, closers at row 0 col 3 and room 18's row 2 col 7), the cutter (0x0C/0x0D) at
row 2 cols 6..7. Heads (charid 8) in rooms 1, 2, 21, 22, 23, 24; (charid 7) in 11, 18; the charger (charid 11) in
16, 20, 10.

"pot" cannot be played: room 18 (down a shaft from room 3, no way back up) leads out only through room 17's gate, and
its plate on the ledge above the cutter is grabbed from col 7, the cutter's right half, where the jump sets the cutter
off and it closes on him before he hangs (see ROUTING.md; an exhaustive search of room 17 found no opening).
"pot2" (Sergio's second) opens the gate from above: room 17's top-row loose floor dropped on its row-2 plate.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

H0 = 5   # max HP at level 8's start ("Max HP At Level", as level 7: the run reaches it with enough)
SWORD = ("Tile[9][26]", "!=", 0x16)       # the heirloom sword taken (room 9 row 2 col 5)
EXIT = ("Tile Opening[23][23]", ">", 0)   # room 23's exit door (row 2 col 2)
# room 2's gate (row 2 col 9, between rooms 3 and 2): open at the start (200), it slams ~3 ticks after he comes over room
# 3; room 3's plate (row 1 col 9, past the cutter at cols 7..8) opens it (4, 28 .. 196, then its timer 250 down), room
# 3's closers (row 1 col 5, row 2 col 2) shut it. The plate pressed: 0x800 set in its modifier while he is on it (the
# gate's value cannot tell: a fast route is in room 3 while it is still open from the start)
PLATE3 = ("Tile Modifier[3][20]", ">", 0x109002B)
GATE17 = ("Tile Opening[17][24]", ">", 0) # room 17's gate (row 2 col 3): its plates at row 1 col 8 (the ledge above the
                                          # cutter) and row 2 col 2 open it; room 18's closer (row 2 col 7) shuts it
LOOSE17 = ("Tile[17][3]", "!=", 0x0B)     # room 17's loose floor (row 0 col 2) gone: it falls on the plate below (row 2
                                          # col 2), which sticks (tile 0x0E) and opens the gate for good (mobs.c mob_land)
CLOSER18 = ("Tile Modifier[18][28]", ">", 0x1090030)   # room 18's closer (row 2 col 7) pressed (0x800 while on it)
CLOSER17 = ("Tile Modifier[17][4]", ">", 0x1090031)    # room 17's closer (row 0 col 3) pressed


def start():
    """16 (its middle row, left) -> 13 (down its gap) -> 4 -> down into 3"""
    return [
        step(room(16), (16, L, ROW_Y[1], 1.0, 1.0)),
        step(room(13), (13, col_x(4), D, 1.0, 1.0)),
        step(room(4), (4, col_x(3), D, 1.0, 1.0)),
    ]


def west():
    """2 -> 12 -> 20 -> 22 -> 11 (west), up from 11's top row into room 9 (its floor's gap at col 2)"""
    return [
        step(room(2), (2, L, None, 1.0, 1.0)),
        step(room(12), (12, L, None, 1.0, 1.0)),
        step(room(20), (20, L, None, 1.0, 1.0)),
        step(room(22), (22, L, None, 1.0, 1.0)),
        # 11: in on its middle row (from 22's), up to its top row at col 7 (the one gap above it), left, up through
        # room 9's floor gap (col 2)
        step(room(11), (11, col_x(7), U, 1.0, 2.0)),
        step(room(11, row(0)), (11, col_x(2), U, 1.0, 2.0)),
    ]


def sword_and_exit():
    """room 9: the sword (twice: the scene, then for good), west 1 -> 24 -> 23: its plate (col 9) opens the door
    (cols 1..2), into it"""
    return [
        step(room(9), (9, col_x(5), ROW_Y[2], 1.0, 1.0)),
        step(room(9, ("Player Current Sequence", "==", 0xED)), (9, col_x(5), ROW_Y[2], 1.0, 1.0)),
        # (the sword in every step after: a later step's rule marks the earlier ones done, so without it the route skips it)
        step(room(9, SWORD), (9, L, ROW_Y[2], 1.0, 1.0)),
        step(room(1, SWORD), (1, L, ROW_Y[2], 1.0, 1.0)),
        step(room(24, SWORD), (24, L, ROW_Y[2], 1.0, 1.0)),
        step(room(23, SWORD), (23, col_x(9), ROW_Y[2], 1.0, 1.0)),
        step(room(23, SWORD, EXIT), (23, col_x(2), ROW_Y[2], 1.0, 1.0)),
        # into the door (Up by it: sequence 0x46): its frames take him inside it (x ~col 2 + 31, y 26 above the floor)
        step(room(23, SWORD, ("Player Current Sequence", "==", 0x46)), (23, col_x(2) + 31, ROW_Y[2] - 26, 1.0, 1.0)),
    ]


ROUTES = {
    # 16 13 4 3 2 12 20 22 11 9 (the sword) 1 24 23
    "np": start() + [
        step(room(3), (3, col_x(9), ROW_Y[1], 1.0, 1.0)),           # the plate (past the cutter)
        step(room(3, PLATE3), (3, L, ROW_Y[2], 1.0, 1.0)),          # back left (over the closers), down, through the gate
    ] + west() + sword_and_exit(),
    # Sergio's (2026-10-04): 16 13 4 3 2 12 6 17 (its top row: the loose floor dropped on the gate's plate) 18 (the
    # potion, room 18's closer never pressed) 17 (back through the gate, up) 6 (top row) 12 20 22 11 9 1 24 23
    "pot2": start() + [
        step(room(3), (3, col_x(9), ROW_Y[1], 1.0, 1.0)),
        step(room(3, PLATE3), (3, L, ROW_Y[2], 1.0, 1.0)),
        step(room(2), (2, L, None, 1.0, 1.0)),
        step(room(12), (12, col_x(7), D, 1.0, 1.0)),                 # down its shaft (cols 6..7)
        step(room(6), (6, R, U, 1.0, 1.0)),                          # onto the top row's pillar (col 8), right
        step(room(17, row(0)), (17, col_x(2), ROW_Y[0], 1.0, 1.0)),  # (17's cols 0 and 6's col 9 empty) to the loose floor
        step(room(17, LOOSE17), (17, R, D, 1.0, 1.0)),               # down (not onto the closer at col 3)
        step(room(17, LOOSE17, GATE17), (17, R, ROW_Y[2], 1.0, 1.0)),   # right through the gate, the cutter, the tunnel
        step(room(18, hp(H0), LOOSE17), (18, col_x(8), ROW_Y[2], 1.0, 1.0)),   # the potion (over the closer at col 7)
        step(room(18, hp(H0 + 1), LOOSE17), (18, L, ROW_Y[2], 1.0, 1.0)),
        # back through the gate, up onto the row-1 gate tile (col 3, from col 2 facing right), up again (the closer:
        # the gate no longer matters), left along the top row into room 6, up into room 12 (col 8's floor)
        step(room(17, hp(H0 + 1)), (17, col_x(2), U, 1.0, 2.0)),
        step(room(17, hp(H0 + 1), row(1)), (17, col_x(3), U, 1.0, 2.0)),
        step(room(17, hp(H0 + 1), row(0)), (17, L, ROW_Y[0], 1.0, 1.0)),
        step(room(6, hp(H0 + 1), row(0)), (6, col_x(8), U, 1.0, 2.0)),   # (its bottom row has no way up)
        step(room(12, hp(H0 + 1)), (12, L, None, 1.0, 1.0)),
    ] + west()[2:] + sword_and_exit(),
    # 16 13 4 3 18 (the potion) 17 (its gate; the cutter) 6 (upper row) 12 20 22 11 9 1 24 23
    "pot": start() + [
        step(room(3), (3, col_x(4), D, 1.0, 1.0)),
        step(room(18, hp(H0)), (18, col_x(8), ROW_Y[2], 1.0, 1.0)),
        step(room(18, hp(H0 + 1)), (18, L, ROW_Y[2], 1.0, 1.0)),
        # 17: hanging from the ledge at col 8 (its plate opens the gate; grabbed from col 7, the cutter's right half, facing
        # right: under col 8 a jump only bumps the plate), back down past the cutter, through the gate, up at its far side
        # (col 1's pillar top) and left into room 6's middle row
        step(room(17, hp(H0 + 1)), (17, col_x(7) + 8, U, 1.0, 1.0)),
        step(room(17, hp(H0 + 1), GATE17), (17, col_x(1), ROW_Y[2], 1.0, 1.0)),
        step(room(17, hp(H0 + 1), GATE17, row(2), ("Player Current Col", "<=", 2)), (17, L, U, 1.0, 1.0)),
        step(room(6, hp(H0 + 1)), (6, col_x(7), U, 1.0, 2.0)),
        step(room(12, hp(H0 + 1)), (12, L, None, 1.0, 1.0)),
    ] + west()[2:] + sword_and_exit(),
}
HPM = 50   # a small Player HP Magnet: ties only (level 7: without it the search wore down to 1 hit point and waited)
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"np": H0, "pot": H0 + 1, "pot2": H0 + 1}

# the small potions (+1 hit point) are never drunk (rooms 8, 20, 24); in level 8 only (the next level's tiles)
SMALL = [(8, 21), (20, 24), (24, 24)]
FAILS = {k: [[("Tile[%d][%d]" % rt, "!=", 0x0A), ("Current Level", "==", 8)] for rt in SMALL] for k in ROUTES}
for k in ROUTES:   # room 11's bottom row is all wall (a clip from room 22's leads nowhere)
    FAILS[k].append([("Player Room", "==", 11), ("Player Current Row", "==", 2), ("Current Level", "==", 8)])
    # the level left without the sword (the win rule reads Next Level only)
    FAILS[k].append([("Next Level", "==", 9), ("Tile[9][26]", "==", 0x16), ("Current Level", "==", 8)])
# pot2: room 18's closer would shut the gate he came through (a closer slams even a stuck-open gate); room 17's top-row
# closer likewise, before the potion
FAILS["pot2"].append([CLOSER18, ("Current Level", "==", 8)])
# (and room 18 entered down room 3's shaft, the loose floor still up: room 17's gate shut for good)
FAILS["pot2"].append([("Player Room", "==", 18), ("Tile[17][3]", "==", 0x0B), ("Current Level", "==", 8)])
FAILS["pot2"].append([CLOSER17, GATE17, ("Player Max HP", "==", H0), ("Current Level", "==", 8)])

LEVEL = {
    "name": "l8", "next": 9, "init": "level01020304050607.sol", "routes": ROUTES, "maxhp": MAXHP,
    # the exit door, room 2's and 17's gates, room 9's gate
    "hash": [[23, 22], [23, 23], [2, 30], [17, 14], [17, 24], [9, 29], [12, 16], [17, 3]],
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {"Max HP At Level": [8, H0]},
}

if __name__ == "__main__":
    main(LEVEL)
