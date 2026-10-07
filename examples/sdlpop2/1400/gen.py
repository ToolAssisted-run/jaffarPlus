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

# --hp N: the maximum hit points level 14 starts with (default 7: levels 11's and 13's potions after 5); the room 3
# measurements start from 3 (no big potion taken on the way)
H0 = 7
# --to N: the farm route's target maximum (room 3's guards: a dead one may leave a big potion)
HT = None
if "--to" in sys.argv:
    i = sys.argv.index("--to"); HT = int(sys.argv[i + 1]); del sys.argv[i:i + 2]
NORNG = "--norng" in sys.argv   # (the random seed not hashed: geometry tests)
if NORNG: sys.argv.remove("--norng")
if "--hp" in sys.argv:
    i = sys.argv.index("--hp"); H0 = int(sys.argv[i + 1]); del sys.argv[i:i + 2]

ROUTES = {
    # 1 (down off the ledge's right end, col 5) 2 (its floor, leftwards: the scene) -> 4
    "scene": [
        step(room(1), (1, col_x(5) + 24, D, 1.0, 2.0)),
        step(room(2), (2, L, ROW_Y[0], 1.0, 1.0)),
        step(room(4), (4, R, ROW_Y[1], 1.0, 1.0)),
    ],
    # (exploring) is room 4's middle row reachable from its lower tile (row 2 col 3)? down onto it, then up
    "ledge": [
        step(room(4), (4, None, D, 1.0, 2.0), (3, None, U, 1.0, 2.0, "keep")),
        step(room(4, ("Stood On Row[4][2]", "==", 1)), (4, col_x(4), U, 1.0, 2.0), (3, None, U, 1.0, 2.0, "keep")),
        step(room(4, ("Stood On Row[4][2]", "==", 1), ("Player Pos Y", "<", 125), ("Player Frame", "==", 15)), (4, col_x(4), U, 1.0, 2.0)),
    ],
    # (exploring) back up: room 3's bottom row -> room 4's middle row
    "up": [
        step(room(3), (3, col_x(3), U, 1.0, 2.0)),
        step(room(4), (4, col_x(4), U, 1.0, 2.0)),
        step(room(4, ("Player Pos Y", "<", 125)), (4, col_x(4), ROW_Y[1], 1.0, 2.0)),
    ],
    # (exploring) from room 4's start down into room 3, onto its bottom row
    "down": [
        step(room(4), (4, None, D, 1.0, 2.0)),
        step(room(3), (3, None, D, 1.0, 2.0)),
        step(room(3, row(2)), (3, None, D, 1.0, 2.0)),
    ],
}


def gm(damage, kill, dist):
    """the guards of his room: per hit point the live ones lost, per kill so far, per pixel to the nearest live one"""
    return {"Type": "Set Guard Magnet", "Damage Intensity": damage, "Kill Intensity": kill, "Distance Intensity": dist}


def pm(i):
    return {"Type": "Set Big Potion Magnet", "Intensity": i}


BIG = ("Room Big Potions", ">", 0)


def farm(h0, ht):
    """room 4's start, down into room 3; there, for each point of maximum: a guard killed where it leaves a big potion
    (a kill that leaves none fails: the random seed is hashed, the search times the kill), the potion drunk; then back up
    onto room 4's middle row. Sergio: a strong magnet on the guards' hit points (both, when two stand), a small one
    towards the nearest."""
    st = [step(room(4, hp(h0)), (4, None, D, 1.0, 2.0))]
    for h in range(h0, ht):
        st += [step(room(3, hp(h)), (3, None, None), acts=[gm(400.0, 3000.0, 1.0), pm(0.0)]),
               step(room(3, hp(h), BIG), (3, None, None), acts=[gm(400.0, 3000.0, 0.0), pm(2.0)])]
    # back up the shaft at col 3 (four climbs: each from under its ledge, x ~250, facing right; room 3's top row from col
    # 3, room 4's lower tile from room 3's top row). A row climbed must outweigh a kill (else killing on ranks above
    # leaving, and the climbers are pruned): 60 a pixel of height; a guard in the way may be fought off, not killed
    # (FAILS). (The first return search sat under room 4's ledge for 400 ticks: its x magnet was a
    # tile off, and the states walking to the right spot ranked below the ones jumping in place.)
    st += [step(room(3, hp(ht)), (3, 258, U, 1.0, 60.0), acts=[gm(400.0, 3000.0, 0.0), pm(0.0)]),
           step(room(4, hp(ht)), (4, 250, ROW_Y[1], 1.0, 60.0)),
           step(room(4, hp(ht), ("Player Pos Y", "<", 125)), (4, 250, ROW_Y[1], 1.0, 60.0))]
    return st


if HT is not None: ROUTES["farm"] = farm(H0, HT)
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {k: (HT if k == "farm" else H0) for k in ROUTES}

# no potion is drunk (sequence 0x4E)
FAILS = {k: [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 14)]] for k in ROUTES}
if "farm" in ROUTES:   # (potions are drunk there)
    # (Sergio) no guard is killed for nothing, on the way back up either: a guard in the way there is passed, or was the
    # last potion's carrier; nor one more than needed (a big potion appearing at the target maximum)
    FAILS["farm"] = [[("Wasted Kills", ">", 0), ("Current Level", "==", 14)],
                     [hp(HT), BIG, ("Current Level", "==", 14)]]
FAILS["ledge"].append([("Player Room", "==", 3), ("Player Pos Y", ">", 70)])   # (room 3 only hanging under room 4's tile)

LEVEL = {
    "name": "l14", "next": 15, "init": "level01020304050607080910111213.sol", "routes": ROUTES, "maxhp": MAXHP,
    "hash": [],
    "fails": FAILS,
    "penalize_ahead": True,
    # (level 11 replays as searched: 5, its potion 6, level 13's 7; level 14 then starts with H0)
    "emulator": {"Max HP At Level": [11, 5, 14, H0]},
    "game": {"Hash Random Seed": not NORNG, "Hash Stood On Rows": [[4, 2]]},
}

if __name__ == "__main__":
    main(LEVEL)
