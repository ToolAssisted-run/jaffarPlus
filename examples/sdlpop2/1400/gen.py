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


def gm(damage, kill, dist, row=0.0):
    """the guards of his room: per hit point the live ones lost, per kill so far, per pixel to the nearest live one
    (row: per pixel of height to its row only)"""
    return {"Type": "Set Guard Magnet", "Damage Intensity": damage, "Kill Intensity": kill, "Distance Intensity": dist, "Row Intensity": row}


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


TOP0 = ("Player Pos Y", "<", 70)       # on a room's top row
JAFFAR = ("Opponent Char Id", "==", 6)
JDOWN = ("Opponent Current Row", ">=", 1)   # Jaffar off his top row (sent on his round)
JROW2 = ("Opponent Current Row", "==", 2)   # ... on the bottom row
SPIRIT = ("Player Char Id", "==", 1)   # the prince out of his body (eight standing turns in a row: spirit.c turn_count)


def end():
    """from room 4's middle row at 11/11 (Sergio): the eight turns at once (the illusions of room 6 strike only the
    prince in his body), then as the spirit: 5, 6 (the Jaffars cut down until the rest leave), up into 7 | 8, the
    fireball"""
    st = [step(room(4))]
    st += [step(room(4, ("Turn Count", ">=", k))) for k in range(1, 8)]
    st += [step([SPIRIT, ("Player Room", "==", 4)], (4, R, ROW_Y[1], 1.0, 1.0)),
           step(room(5, SPIRIT), (5, R, ROW_Y[1], 1.0, 1.0)),
           # 6: the four Jaffars (one cut each; they do not strike the spirit): the kills rewarded, a pull to the nearest
           step(room(6, SPIRIT), (6, None, None), acts=[gm(0.0, 3000.0, 1.0)]),
           # (room 6's list: the four and his body, 5; a Jaffar cut down leaves it a few ticks later; with three gone
           # the last one leaves by himself: 1. The level's own count: the kill counters start again with each search)
           step(room(6, SPIRIT, ("Room Chars[6]", "<=", 4)), (6, None, None), acts=[gm(0.0, 3000.0, 1.0)]),
           step(room(6, SPIRIT, ("Room Chars[6]", "<=", 3)), (6, None, None), acts=[gm(0.0, 3000.0, 1.0)]),
           step(room(6, SPIRIT, ("Room Chars[6]", "<=", 2)), (6, None, None), acts=[gm(0.0, 3000.0, 1.0)]),
           # up into room 7 from the top row's col 3: room 7's bottom row is open over it
           step(room(6, SPIRIT, ("Room Chars[6]", "<=", 1)), (6, col_x(3), U, 1.0, 20.0), acts=[gm(0.0, 3000.0, 0.0)]),
           # 7 | 8: Jaffar (he waits at room 8's top row, col 7, and walks his round away from the spirit: final.c): onto
           # his row, the fireball (Ctrl there; it costs 2 of the spirit's 3 hit points: one shot). The pull to him, the
           # kill rewarded; the level is won as his death ends ("Next Level" 15)
           # 7 | 8, Jaffar (final.c): he stands at room 8's top row (col 7) and walks a round of four waypoints, leaving one
           # as the spirit enters its zones; a fireball takes 11 ticks to leave and flies 8..16 px a tick, so it must be
           # sent along a row he is still crossing. The plan: the spirit on the top row sends him down to room 8's bottom
           # row (col 4); the spirit follows along the top row, down its gap (col 8) onto the middle row: that sends him
           # left along the bottom row (33 ticks, a turn, then up); left to the hole over where he stood (col 4), down
           # it, the fireball after him. (The spirit takes no damage from a fall, 1.x: kid.c land.)
           step(room(7, SPIRIT), (7, col_x(9), U, 1.0, 20.0), (8, None, None, 1.0, 1.0, "keep")),
           # (each step asks for Jaffar's place too: without that the search ran along room 7's bottom row into room 8's,
           # the later steps marking the earlier ones done, and shot with him still at his first waypoint)
           step(room(7, SPIRIT, TOP0, JDOWN), (7, R, ROW_Y[0], 1.0, 20.0), (8, col_x(8), ROW_Y[0], 1.0, 20.0, "keep")),
           step(room(8, SPIRIT, TOP0, JDOWN), (8, col_x(8), ROW_Y[1], 1.0, 1.0), (7, None, None, 1.0, 1.0, "keep")),
           step(room(8, SPIRIT, row(1), ("Player Pos X", ">", col_x(4) + 20), JROW2), (8, col_x(4), D, 1.0, 20.0), (7, None, None, 1.0, 1.0, "keep")),
           step(room(8, SPIRIT, row(2), JROW2), (8, None, None), (7, None, None, 1.0, 1.0, "keep")),
           # the cast begun with Jaffar on the spirit's row (its first frame can still be walked out of, the hit points
           # gone: FAILS), the fireball out (the only moving object up here) and drawn to him (Sergio), Jaffar struck
           # (sequence 0xF3); the level is won as his death ends ("Next Level" 15)
           step([SPIRIT, ("Room Live Guards On Row", ">=", 1), ("Player Current Sequence", "==", 0xF2), ("Current Level", "==", 14)],
                (8, None, None, 1.0, 1.0, "keep"), (7, None, None, 1.0, 1.0, "keep")),
           step([SPIRIT, ("Moving Object Count", ">=", 1), ("Room Live Guards On Row", ">=", 1), ("Current Level", "==", 14)],
                (8, None, None, 1.0, 1.0, "keep"), (7, None, None, 1.0, 1.0, "keep"), acts=[{"Type": "Set Fireball Magnet", "Intensity": 2.0}]),
           step([JAFFAR, ("Opponent Current Sequence", "==", 0xF3), ("Current Level", "==", 14)],
                (8, None, None, 1.0, 1.0, "keep"), (7, None, None, 1.0, 1.0, "keep"), acts=[{"Type": "Set Fireball Magnet", "Intensity": 0.0}])]
    return st


def fight():
    """7 | 8 from the spirit's entry (after r7R / r7L / r7C), the plan left to the search: the top row (Jaffar leaves his
    first waypoint), then his row, the cast, the fireball, Jaffar struck (--upto 5 --anyframe: the win at the hit).
    Measured (tools/probe.sh): C casts; the fireball appears 11 ticks after the cast begins, its box 35..69 px ahead of
    where the spirit stood (the cast moves it 14 forward), so a shot from nearer than ~30 px passes through him; the
    spirit's landing takes 9..13 ticks; Jaffar, at the waypoint under room 8's hole with the spirit on his row, turns
    (8), jumps (15) and climbs (6): off the row 30 ticks after the spirit came onto it."""
    K7 = (7, 0, 0, 0.0, 0.0, "keep"); K8 = (8, 0, 0, 0.0, 0.0, "keep")
    L14 = ("Current Level", "==", 14)
    # (steps that name no room: every magnet "keep", or routegen takes its room for one ahead and pushes out of it)
    return [step([SPIRIT, L14], (7, 0, U, 0.0, 20.0, "keep"), (8, 0, U, 0.0, 20.0, "keep")),
            step([SPIRIT, JDOWN, L14], K7, K8, acts=[gm(0.0, 0.0, 0.0, 5.0)]),
            step([SPIRIT, ("Room Live Guards On Row", ">=", 1), ("Player Current Sequence", "==", 0xF2), L14], K7, K8,
                 acts=[gm(0.0, 0.0, 0.0, 5.0)]),
            step([SPIRIT, ("Moving Object Count", ">=", 1), ("Room Live Guards On Row", ">=", 1), L14], K7, K8,
                 acts=[gm(0.0, 0.0, 0.0, 5.0), {"Type": "Set Fireball Magnet", "Intensity": 2.0}]),
            step([JAFFAR, ("Opponent Current Sequence", "==", 0xF3), L14], K7, K8,
                 acts=[gm(0.0, 0.0, 0.0, 0.0), {"Type": "Set Fireball Magnet", "Intensity": 0.0}])]


def fight_b():
    """7 | 8, the plan that the measurements leave (final.c; tools/probe.sh, trp.c). Jaffar walks a round of four
    waypoints (0: room 8's top row col 7; 1: room 8's bottom row col 4, under its middle row's hole; 2: room 7's bottom
    row col 5; 3: room 7's top row col 6) and, standing at one, leaves it as the spirit enters its zones. A fireball
    appears 11 ticks after the cast (C) begins and hits only from 35 px or more behind him (27 or more ahead): its box
    starts 35 px before where the spirit stood. From waypoint 0 the spirit on the top row sends him to 1 (49 ticks); there
    the spirit on his row sends him back up within 30 ticks, too few for a landing (9), a turn (8) and the cast from
    where room 8's hole lets it down, on top of him. But the spirit on room 8's middle row right of the hole (down the
    top row's gap at col 8) sends him on to waypoint 2, along the whole bottom row, and he turns round there: the spirit
    drops down the hole behind him, facing left, and the fireball catches him."""
    L14 = ("Current Level", "==", 14)
    JGONE = ("Opponent Pos X", "<", 262)   # (room 8's coordinates: Jaffar left of waypoint 1, on his way to 2)
    # (steps that name no room: every magnet "keep")
    return [# up: the top row (room 7's col 9 over its gap; or, fb8, room 8's col 3 from the middle row's col 2)
            step([SPIRIT, L14], (7, R, U, 1.0, 20.0, "keep"), (8, col_x(2) + 16, U, 1.0, 20.0, "keep")),
            # Jaffar on his way down: along the top row to room 8's col 8, down its gap onto the middle row
            step([SPIRIT, JDOWN, L14], (8, col_x(8) + 16, ROW_Y[1], 1.0, 1.0, "keep"), (7, R, ROW_Y[0], 1.0, 1.0, "keep")),
            # the middle row right of the hole: to the hole's edge (not down it before Jaffar has left waypoint 1: he
            # would go back up; FAILS)
            step(room(8, SPIRIT, row(1), ("Player Pos X", ">", col_x(5)), JDOWN, L14), (8, col_x(5), None, 1.0, 1.0)),
            # down the hole behind him
            step(room(8, SPIRIT, row(2), JROW2, JGONE, L14), (8, None, None)),
            # the cast (facing left, Jaffar on the row), the fireball, Jaffar struck
            step(room(8, SPIRIT, row(2), JROW2, JGONE, ("Player Current Sequence", "==", 0xF2), ("Player Direction", "==", -1), L14), (8, None, None)),
            step([SPIRIT, ("Moving Object Count", ">=", 1), ("Room Live Guards On Row", ">=", 1), JGONE, L14], (8, None, None),
                 acts=[{"Type": "Set Fireball Magnet", "Intensity": 2.0}]),
            step([JAFFAR, ("Opponent Current Sequence", "==", 0xF3), L14], (8, None, None), acts=[{"Type": "Set Fireball Magnet", "Intensity": 0.0}])]


def to7(side, no_row2=False, no_right=False):
    """(Sergio: the ending in segments) the spirit from room 4 through room 6's Jaffars up into room 7, by room 6's left
    top-row platform (cols 1..3: up through room 7's bottom-row gap at col 3) or its right one (cols 6..7: room 7's
    col 7 from under its right end). Three of the four are cut down (top row cols 3 and 7, middle row col 4, bottom row
    col 5), the fourth leaves; which three and in what order decides where he ends: no_row2 keeps him off the bottom
    row, no_right off the right platform (FAILS)."""
    k = [gm(0.0, 3000.0, 1.0)]
    up = (6, col_x(7) + 8, U, 1.0, 20.0) if side == "R" else (6, col_x(3), U, 1.0, 20.0)
    # (side "C", Sergio: from the left platform too, but facing right: up onto room 7's middle platform, col 4, not
    # its left one)
    there = {"L": ("Player Pos X", "<", col_x(3) + 20), "C": ("Player Pos X", ">=", col_x(3) + 20), "R": ("Player Pos X", ">=", col_x(5))}[side]
    return [step([SPIRIT, ("Player Room", "==", 4)], (4, R, ROW_Y[1], 1.0, 1.0)),
            step(room(5, SPIRIT), (5, R, ROW_Y[1], 1.0, 1.0)),
            step(room(6, SPIRIT), (6, None, None), acts=k),
            step(room(6, SPIRIT, ("Room Chars[6]", "<=", 4)), (6, None, None), acts=k),
            step(room(6, SPIRIT, ("Room Chars[6]", "<=", 3)), (6, None, None), acts=k),
            step(room(6, SPIRIT, ("Room Chars[6]", "<=", 2)), (6, None, None), acts=k),
            step(room(6, SPIRIT, ("Room Chars[6]", "<=", 1)), up, acts=[gm(0.0, 3000.0, 0.0)]),
            # (room 6's list empty: all four gone, the last one's leaving seen to its end. Without it the search climbed
            # out after one kill, the later step marking the earlier ones done: no Jaffar upstairs then)
            step(room(7, SPIRIT, there, ("Room Chars[6]", "==", 0)), (7, None, None))]


ROUTES["end"] = end()
ROUTES["fight"] = fight()
ROUTES["fb"] = fight_b()
ROUTES["fb8"] = fight_b()
ROUTES["r7R"] = to7("R", no_row2=True)          # (middle row's, the two of the top row; out by the right platform)
ROUTES["r7L"] = to7("L", no_row2=True)          # (the same three; back to the left platform)
ROUTES["r7L2"] = to7("L", no_right=True)        # (bottom, middle, top left: upwards; out by the left platform)
ROUTES["r7C"] = to7("C")                         # (out by the left platform onto room 7's middle one)
if HT is not None: ROUTES["farm"] = farm(H0, HT)
HPM = 50   # a small Player HP Magnet: ties only
ROUTES = {k: [dict(v[0], hpm=HPM)] + v[1:] for k, v in ((k, dedup(v)) for k, v in ROUTES.items())}
MAXHP = {k: (HT if k == "farm" else 11 if k in ("end", "fight", "fb", "fb8", "r7R", "r7L", "r7L2", "r7C") else H0) for k in ROUTES}

# no potion is drunk (sequence 0x4E)
FAILS = {k: [[("Player Current Sequence", "==", 0x4E), ("Current Level", "==", 14)]] for k in ROUTES}
# (the spirit has 3 hit points left and a cast takes 2 of them and needs 3: one lost any other way ends it)
FAILS["end"] = [[SPIRIT, ("Player Current HP", "==", 2), ("Current Level", "==", 14)],
                # the cast spent for nothing: no fireball in the air, Jaffar alive
                [SPIRIT, ("Player Current HP", "==", 1), ("Player Current Sequence", "!=", 0xF2), ("Moving Object Count", "==", 0),
                 ("Room Live Guards", ">=", 1), ("Current Level", "==", 14)],
                # Jaffar dead of anything but the fireball (a fall, held up by a change of rooms, killed him once: the
                # level is not won then)
                [("Opponent Char Id", "==", 6), ("Opponent Is Alive", ">=", 0), ("Opponent Current Sequence", "!=", 0xF3), ("Current Level", "==", 14)]]
FAILS["fight"] = FAILS["end"]
# (plan B) Jaffar going back up from waypoint 1 (he turns round there; later on the middle row with the spirit below)
FAILS["fb"] = FAILS["end"] + [[SPIRIT, ("Opponent Current Row", "==", 2), ("Opponent Pos X", ">", 262), ("Opponent Current Sequence", "==", 5),
                               ("Player Room", "==", 8), ("Player Current Row", "==", 2), ("Current Level", "==", 14)],
                              [SPIRIT, ("Opponent Current Row", "==", 1), ("Player Current Row", "==", 2), ("Current Level", "==", 14)]]
# (fb8: not by room 7's top row)
FAILS["fb8"] = FAILS["fb"] + [[SPIRIT, ("Player Room", "==", 7), TOP0, ("Current Level", "==", 14)]]
NO_ROW2 = [SPIRIT, ("Player Room", "==", 6), ("Player Pos Y", ">", 150), ("Current Level", "==", 14)]
NO_RIGHT = [SPIRIT, ("Player Room", "==", 6), TOP0, ("Player Pos X", ">", col_x(5)), ("Current Level", "==", 14)]
FAILS["r7R"] = [NO_ROW2]
FAILS["r7L"] = [NO_ROW2]
FAILS["r7L2"] = [NO_RIGHT]
# (not by room 6's right platform: that comes up at room 7's col 7)
# (and off room 6's bottom row, as the other two: without it the search cut the bottom row's Jaffar down and climbed
# two rows back, 321)
FAILS["r7C"] = [[SPIRIT, ("Player Room", "==", 7), ("Player Pos X", ">", col_x(6)), ("Current Level", "==", 14)], NO_ROW2]
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
