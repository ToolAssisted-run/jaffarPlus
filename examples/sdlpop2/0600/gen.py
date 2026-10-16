#!/usr/bin/env python3
"""Level 6 scripts, one per route: see ../routegen.py.

  gen.py ROUTE [--upto N] [--db MB] [--init FILE] [--out FILE] [--tag TAG] [--maxsteps N]

Level 6 (the temple, level kind 4), rooms:            . 28 27 (start)
                                        24 15 13 12  2 14
                                     26 23 21  9  8  5 10 11
                                     25 22 16  7  4  6  3  1
                                                 17 18 19 20
The prince comes out of room 3's door (row 2 col 3..4; at first his room reads 27). Room 3's row 2 col 0 is a closer
for room 6's gate (row 2 col 9). Room 6's floor has a hole (row 2 cols 1..4) down into room 18; room 18's row 1 leads
left through a crawl tunnel (low tiles 7: room 18 col 0, room 17 cols 8..9) to the broken sword (room 17 row 1 col 5).
Room 17 leads up (row 0 col 1) into room 4: the first medusa head (charid 7/8 of heads.c) and a column of pillars (col
0) up through 8 and 12. Big potion: room 5 row 2 col 7. Room 7: two heads and a bad potion. Exit: room 16's two doors,
row 2 cols 1..2 (opened by the plate at col 7) and cols 5..6 (the plate at col 0).

The solutions (after level0102030405.sol; ../ROUTING.md has the table): sword-to4.sol (room 4 with the sword) then
sw-push-L.sol / sw-push-R.sol; nosword-to4.sol then ns-4to8.sol then ns-left-R.sol; sword-to4.sol then sw-4to8.sol then
sw-left-R.sol / sw-down7-R.sol; pot-to4.sol then pot-4to5.sol (the potion) then pot-push-L.sol / pot-push-R.sol.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from routegen import L, R, U, D, ROW_Y, col_x, room, hp, row, step, dedup, main  # noqa: E402

SWORD = ("Tile[17][16]", "!=", 0x16)   # the broken sword (room 17 row 1 col 5) taken
HPM = 200    # Player HP Magnet (per hit point): room 7's two heads and room 16's kill a prince with one left; room 13's
             # small potion (row 2 col 8, by the pillar he goes down) is worth its detour
KILL = 100   # Opponent HP Magnet in room 4 (per hit point the head lost): with the sword the head sits on the pillars
             # he climbs (it spots him as he comes up from room 17); it dies to three cuts or smashed into a wall


def to_sword():
    """27 (the start) -> 3 -> 6 (over room 3's closer) -> down into 18 -> the crawl tunnel -> 17 (the sword)"""
    return [
        step(room(27), (27, L, None)),
        step(room(3), (3, L, None)),
        step(room(6), (6, col_x(2), D, 1.0, 1.0)),
        step(room(18), (18, L, ROW_Y[1], 1.0, 1.0)),
        step(room(17), (17, col_x(5), ROW_Y[1], 1.0, 1.0)),
        step(room(17, SWORD), (17, col_x(1), U, 1.0, 1.0)),
    ]


def to4(sword):
    """the start to room 4: with the sword (the crawl tunnel) or straight across room 6 (a running jump short of its
    floor's far side, caught from room 18's top row, then up)"""
    if sword: return to_sword() + [step(room(4, SWORD), (4, col_x(0), U, 1.0, 2.0))]
    return [step(room(27), (27, L, None)), step(room(3), (3, L, None)), step(room(6), (6, L, None)),
            step(room(4), (4, col_x(0), U, 1.0, 2.0))]


def up(hpn, way, sword):
    """room 4's pillars (row 2 col 1, row 1 col 1, row 0 col 0) up through 8 and 12 to room 13's ledge (row 1), then
    down room 13's and room 9's pillars (col 7) to room 9's floor"""
    return [
        step(room(4, hp(hpn)), (4, col_x(0), U, 1.0, 2.0), opp=KILL if sword else None),
        step(room(8, hp(hpn)), (8, col_x(0), U, 1.0, 2.0), opp=0.0),
        step(room(12), (12, col_x(0), U, 1.0, 2.0)),
        step(room(12, row(0)), (12, L, U, 1.0, 1.0)),
        step(room(13), (13, col_x(7), D, 1.0, 1.0)),
        step(room(9), (9, col_x(7), D, 1.0, 1.0)),
        step(room(9, row(2)), (9, col_x(5), D, 1.0, 1.0) if way == "down7" else (9, L, D, 1.0, 1.0)),
    ]


def potion_push():
    """The potion and the push (Sergio's route): room 6's loose floor (row 2 col 5) must stay until the potion, the only
    thing that breaks the fall from it: the first way down into room 18 is the hole (a fall, a hit point), then the crawl
    tunnel, the sword, up through room 4 past the head (not killed: it pushes him later), 8 (its middle row), right
    into room 5: the running jump falls short of the potion's floor (row 2 cols 6..7) but Shift grabs room 6's wall top
    under it and Up climbs onto it; then off its left end onto the loose floor, down with it into room 18 again, the
    crawl tunnel, room 17, up into room 4 a second time, where the head pushes him through the left wall"""
    return to_sword() + [
        step(room(4, SWORD), (4, col_x(0), U, 1.0, 2.0)),
        step(room(8, hp(3)), (8, R, ROW_Y[1], 1.0, 1.0)),
        # (room 6 under the potion: the grab, x ~337, y 56; below that is falling past it)
        step(room(5, hp(3)), (5, col_x(7), ROW_Y[2], 1.0, 1.0), (6, 337, -100, 1.0, 1.0, "keep")),
        step(room(5, hp(4)), (5, col_x(5), D, 1.0, 1.0)),
        step(room(6, hp(4)), (6, col_x(5), D, 1.0, 1.0)),
        step(room(18, hp(4)), (18, L, ROW_Y[1], 1.0, 1.0)),
        step(room(17, hp(4)), (17, col_x(1), U, 1.0, 1.0)),
        # (the head, left idle on the middle row's pillar (col 1) by the climb, cannot fly down: the push is from there)
        step(room(4, hp(4)), (4, L, None, 1.0, 1.0)),
        step(room(7), (7, L, D, 1.0, 1.0)),
    ]


def down7():
    """room 9's floor down its hole (cols 5..6) into room 7 (two heads), left into room 16 (its plate at col 7 opens the
    left door)"""
    return [step(room(7), (7, L, D, 1.0, 1.0))]


def left():
    """room 9's floor left: 21 (blades) -> 23 -> down into 22 -> right into room 16 (its plate at col 0 opens the right
    door)"""
    return [
        step(room(21), (21, L, D, 1.0, 1.0)),
        step(room(23), (23, col_x(2), D, 1.0, 1.0)),
        step(room(22), (22, R, D, 1.0, 1.0)),
    ]


# the doors' openings (0x2A open): the low byte of their right halves' modifiers (0x2080000 above it, shut or open)
DOOR = {"L": ("Tile Opening[16][23]", ">", 0), "R": ("Tile Opening[16][27]", ">", 0)}
DOORX = {"L": col_x(1) + 16, "R": col_x(5) + 16}


def exit16(door):
    """room 16: the plate that opens the door, then into it"""
    plate = col_x(7) if door == "L" else col_x(0)
    return [
        step(room(16), (16, plate, D, 1.0, 1.0)),
        step(room(16, DOOR[door]), (16, DOORX[door], D, 1.0, 1.0)),
        # into that door (Up by it: sequence 0x46): the left one is cols 1..2, the right one cols 5..6
        step(room(16, ("Player Current Sequence", "==", 0x46),
                  ("Player Current Col", "<=", 3) if door == "L" else ("Player Current Col", ">=", 4))),
    ]


ROUTES = {"start": dedup(to_sword() + [step(room(4), (4, col_x(0), U, 1.0, 1.0))])}
MAXHP = {"start": 3}
for sw in (0, 1):
    for way in ("down7", "left"):
        for door in ("L", "R"):
            name = ("sw-" if sw else "ns-") + way + "-" + door
            steps = dedup(to4(sw) + up(3, way, sw) + (down7() if way == "down7" else left()) + exit16(door))
            ROUTES[name] = [dict(steps[0], hpm=HPM)] + steps[1:]
            MAXHP[name] = 3
    # the first head's push through room 4's left wall into room 7
    for door in ("L", "R"):
        name = ("sw-" if sw else "ns-") + "push-" + door
        ROUTES[name] = dedup(to4(sw) + [step(room(4), (4, L, None, 1.0, 1.0)), step(room(7), (7, L, D, 1.0, 1.0))] + exit16(door))
        MAXHP[name] = 3
for door in ("L", "R"):
    ROUTES["sw-pot-push-" + door] = dedup(potion_push() + exit16(door))
    MAXHP["sw-pot-push-" + door] = 4

# the "down7" routes go the long way, down from room 9, not by the head's push through room 4's left wall (the "push"
# routes): room 7 only after room 9 ("Visited Room", hashed). (Rules on where the push lands kept being dodged by pushes
# landing a little further left.) Room 9's hole drops three rows to room 7's floor: dropped from a hang, it costs a hit
# point (no rule on the hit points left there: the HP magnet).
FAILS = {k: [[("Player Room", "==", 7), ("Visited Room[9]", "==", 0)]] for k in ROUTES if "down7" in k}
# the potion and the push: room 6's loose floor stays until the potion; room 4's head lives (charid 7, in the Opponent
# slot while he is in its room or has left it)
# the other door is a fail (the win is only the next level: either door ends the level)
for k in ROUTES:
    if k.endswith("-L") or k.endswith("-R"):
        FAILS.setdefault(k, []).append([("Player Room", "==", 16), ("Player Current Sequence", "==", 0x46),
                                        ("Player Current Col", ">=", 4) if k.endswith("-L") else ("Player Current Col", "<=", 3)])
# a push from room 4's upper rows (climbing) lands inside room 7's top-row walls (cols 7..9), for good
for k in ROUTES:
    if "push" in k: FAILS[k].append([("Player Room", "==", 7), ("Player Current Row", "==", 0)])
for k in ROUTES:
    if "pot" in k:
        FAILS[k] += [[("Tile[6][26]", "==", 0), ("Player Max HP", "==", 3)],
                    [("Opponent Room", "==", 4), ("Opponent Char Id", "==", 7), ("Opponent Current HP", "==", 0)],
                    # the head left in the open on the bottom row by the climb, where it sees him and reaches him for
                    # the push on his way back (left on the middle row's pillar it cannot fly down; knocked into the
                    # left wall (x < 160) it never sees him again; it keeps its place while he is away)
                    [("Player Room", "==", 8), ("Opponent Room", "==", 4), ("Opponent Current Row", "!=", 2)],
                    [("Player Room", "==", 8), ("Opponent Room", "==", 4), ("Opponent Pos X", "<", 160)]]


LEVEL = {
    "name": "l6", "next": 7, "init": "level0102030405.sol", "routes": ROUTES, "maxhp": MAXHP,
    # room 6's loose floor and gate, room 16's exit doors
    "hash": [[6, 26], [6, 30], [16, 23], [16, 27]],   # (room 6's loose floor: its shaking)
    "fails": FAILS,
    "hash_visited": [9],   # (the "down7" fail rules read room 9's visited flag)
    "penalize_ahead": True,
}

if __name__ == "__main__":
    main(LEVEL)
