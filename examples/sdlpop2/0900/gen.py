#!/usr/bin/env python3
"""Level 9 scripts, one per route (Sergio's two): see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 9, rooms:                                          22 23
                 16 15 14 13 12 11  2  8  6  4  5  1  9 10
                                    3 17 18  7 19 20 21
Level 8's route (level0102030405060708.sol: no potion) starts level 9 in room 10 (row 1, x 328). Rooms 2 and 11..16
are the desert (a bottom-row floor only). The level ends on room 8's top row: running left off it (frame 0x28) across
its left edge sets the next level (room.c kid_exit_dir) and the horse ride follows. The big potion: room 13 (row 2
col 3); small ones on the way in rooms 9, 5 and 4. The top row of rooms 5 4 6 8: platforms, gaps and loose floors.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

H0 = 5   # max HP at level 9's start ("Max HP At Level", as levels 7 and 8)
TOP = ("Player Pos Y", "<", 100)   # on the top row (its floor at y 56; row 1's at 119)
HEADS8 = {"Type": "Set Room Heads Magnet", "Room": 8, "Position": col_x(0), "Row": 2, "Intensity": 1.0}   # (pot)


def east_start():
    """10 -> 9 (row 1, over its gap at cols 3..5)"""
    return [
        step(room(10), (10, L, ROW_Y[1], 1.0, 1.0)),
        step(room(9), (9, L, ROW_Y[1], 1.0, 1.0)),
    ]


def top_row(*c):
    """1 (up from row 1 at col 7 onto the top row, cols 0..6) -> 5 -> 4 -> 6 -> 8 along the top row, off its left end"""
    return [
        step(room(1, *c), (1, col_x(7), U, 1.0, 2.0)),
        step(room(1, *c, row(0)), (1, L, ROW_Y[0], 1.0, 2.0)),
    ] + [step(room(r, *c, TOP), (r, L, ROW_Y[0], 1.0, 2.0)) for r in (5, 4, 6, 8)]


ROUTES = {
    # 10 9 1 (up) 5 4 6 8 (the top row; off its left end)
    "np": east_start() + top_row(),
    # 10 9 1 (down) 5 4 6 8 2 11 12 13 (the potion) 12 11 2 8 6 4 5 1 (up) 5 4 6 8 (the top row)
    "pot": east_start() + [
        step(room(1), (1, L, D, 1.0, 1.0)),
    ] + [step(room(r), (r, L, ROW_Y[2], 1.0, 1.0)) for r in (5, 4, 6, 8, 2, 11, 12)] + [
        step(room(13, hp(H0)), (13, col_x(3), ROW_Y[2], 1.0, 1.0)),
        step(room(13, hp(H0 + 1)), (13, R, ROW_Y[2], 1.0, 1.0)),
    ] + [step(room(r, hp(H0 + 1)), (r, R, ROW_Y[2], 1.0, 1.0)) for r in (12, 11, 2)] + [
        # (Sergio) room 8's two heads, woken on the way west, hunt him on the top row at the end (under the ledge he
        # lands on from room 6): on the way back he waits a little there, so that they come to its bottom left, then
        # runs on right. The Room Heads Magnet draws them there (their records, as he leaves them) to the level's end
        step(room(8, hp(H0 + 1)), (8, R, ROW_Y[2], 1.0, 1.0), acts=[HEADS8]),
    ] + [step(room(r, hp(H0 + 1)), (r, R, ROW_Y[2], 1.0, 1.0)) for r in (6, 4, 5)] + top_row(hp(H0 + 1)),
}
HPM = 50   # a small Player HP Magnet: ties only (level 7: without it the search wore down to 1 hit point and waited)
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {"np": H0, "pot": H0 + 1}

# the small potions (+1 hit point; rooms 9, 5, 4 on the way) are never drunk: a drink (sequence 0x4E) outside room 13
# fails (not their tiles: room 9's loose floor at row 1 col 2, under the landing past its gap, falls on its potion)
FAILS = {k: [[("Player Current Sequence", "==", 0x4E), ("Player Room", "!=", 13), ("Current Level", "==", 9)]] for k in ROUTES}
FAILS["pot"].append([("Next Level", "==", 10), ("Player Max HP", "==", H0), ("Current Level", "==", 9)])   # (the end before the potion)
# (Sergio) hit points: none lost before the potion (the drink's own tick aside: max HP up first); on the way back
# along the bottom row some bites are allowed (the woken heads of rooms 8, 4 and 5 follow him from room to room and fly
# faster than he runs: a probe found no way back unbitten), down to 3 left (the HP magnet still counts each one); none
# lost once he has stood on room 1's top row. And from there no drop from the top-row path (hanging from its ledges
# after a short jump, y ~119, and climbing back stays allowed)
FAILS["pot"].append([("Player Current HP", "<", "Player Max HP"), ("Player Max HP", "==", H0), ("Player Current Sequence", "!=", 0x4E), ("Current Level", "==", 9)])
FAILS["pot"].append([("Player Current HP", "<", 3), ("Current Level", "==", 9)])
FAILS["pot"].append([("Stood On Row[1][0]", "==", 1), ("Player Current HP", "<", "HP When Stood[1][0]"), ("Current Level", "==", 9)])
for r in (1, 5, 4, 6, 8):
    FAILS["pot"].append([("Stood On Row[1][0]", "==", 1), ("Player Room", "==", r), ("Player Pos Y", ">", 130), ("Current Level", "==", 9)])
# (Sergio) no fighting: the sword never drawn
FAILS["pot"].append([("Player Sword Drawn", "==", 1), ("Current Level", "==", 9)])

LEVEL = {
    "name": "l9", "next": 10, "init": "level0102030405060708.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [],
    "fails": FAILS,
    "penalize_ahead": True,
    "emulator": {"Max HP At Level": [9, H0]},
}

# pot: room 8's heads, woken on the way west, live in the room records while he is away: hashed, so that the search
# keeps the ways he leaves them (the room records' hash only there: it multiplies the states); the drop fail's flag too
if len(sys.argv) > 1 and sys.argv[1] == "pot": LEVEL["game"] = {"Hash Room Characters": True, "Hash Stood On Rows": [[1, 0]]}

if __name__ == "__main__":
    main(LEVEL)
