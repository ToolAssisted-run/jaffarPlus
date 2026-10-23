#!/usr/bin/env python3
"""The title fight against The Champ, a round at a time: a JaffarPlus script for one round's search, from the round's
start (the movie's rows played, or a saved state) to its end, the player never hit, never a punch of his that does
not land, never floored; at least MIN_LANDED of his punches landed in the round (the judges give a round 10-6 to the
boxer who landed while the other landed none, 0-0 when neither did).

  round.py OUT --rows ROWS [--state STATE] [--steps N] [--db MB] [--min-landed K] [weights...]
  round.py OUT --rows ../test/exhibition-round1.sol --test POWER SPEED STAMINA   (experiments: the exhibition's round
      against The Champ, recorded at 22,000, the player's statistics put in by pokes; no movie)
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "experiments"))
from gen import row, pokes, opponent_pokes  # noqa: E402


def build_pokes(height, weight, frame=28990):
    """The player's height and weight as the fight sees them (237b:07a2): the fight block's words +0x16 (weight x
    0xE3), +0x18 (height x 0x71 + 0x1A7C: the pose's scale, 1eb6:3510), +0x1A (the weight's word + 0x100), in the
    block (DS:5E0C) and its copy (DS:A7AA); the weights within 0x700 of The Champ's (1eb6:469a: else each the mean less
    or more 0x380)"""
    p = []
    if height is not None:
        h = height * 0x71 + 0x1A7C
        p += [[frame, "%04X" % (base + 0x18), "%04X" % h] for base in (0x5E0C, 0xA7AA)]
    if weight is not None:
        w, champ = weight * 0xE3, 76 * 0xE3
        if abs(w - champ) > 0x700:
            mean = (w + champ) >> 1
            w = mean - 0x380 if w < champ else mean + 0x380
            c = mean + 0x380 if w < champ else mean - 0x380
            p += [[frame, "%04X" % (base + 0x16), "%04X" % c] for base in (0x5E72, 0xA810)]
            p += [[frame, "%04X" % (base + 0x1A), "%04X" % ((c + 0x100) & 0xFFFF)] for base in (0x5E72, 0xA810)]
        p += [[frame, "%04X" % (base + 0x16), "%04X" % w] for base in (0x5E0C, 0xA7AA)]
        p += [[frame, "%04X" % (base + 0x1A), "%04X" % ((w + 0x100) & 0xFFFF)] for base in (0x5E0C, 0xA7AA)]
    return p


def main():
    a = argparse.ArgumentParser()
    a.add_argument("out")
    a.add_argument("--rows", required=True, help="the movie's rows to the round's start (Chimera's)")
    a.add_argument("--state", help="the machine's state after them (o4dbBench save), loaded instead of playing them")
    a.add_argument("--test", nargs=3, type=int, metavar=("POWER", "SPEED", "STAMINA"),
                   help="an experiment: the rows recorded at 22,000, the player's statistics poked in")
    a.add_argument("--height", type=int, help="(experiments) the player's height as the creation's byte (75-190; the default 90)")
    a.add_argument("--weight", type=int, help="(experiments) the player's weight as the creation's byte (60-78; the default 72)")
    a.add_argument("--cpu-speed", type=int, default=220000)
    a.add_argument("--steps", type=int, default=1900, help="a round's 3 minutes are 1800 steps")
    a.add_argument("--db", type=int, default=60000)
    a.add_argument("--hash", type=int, default=8000)
    a.add_argument("--pages", type=int, default=200,
                   help="a state's delta pages at most (a won round's states reach about 130 from its start; more: dropped and counted)")
    a.add_argument("--min-landed", type=int, default=1)
    a.add_argument("--reach", type=int, default=520, help="no punch thrown farther from him than this (it could not land)")
    a.add_argument("--no-defense", action="store_true", help="no right Shift (the footwork's button: steps, 420 or more from him)")
    a.add_argument("--distance-weight", type=float, default=1.0)
    a.add_argument("--distance-cap", type=float, default=700.0)
    a.add_argument("--center-weight", type=float, default=0.5)
    a.add_argument("--landed-weight", type=float, default=2000.0)
    a.add_argument("--landed-cap", type=float, default=3.0)
    a.add_argument("--lean-weight", type=float, default=0.0, help="the player's full lean's reward while he is nearer than --lean-range")
    a.add_argument("--lean-range", type=float, default=450.0)
    a.add_argument("--safe-weight", type=float, default=0.0, help="the reward at --safe-distance or more from him (he punches only nearer than 420)")
    a.add_argument("--safe-distance", type=float, default=420.0)
    a.add_argument("--max-thrown", type=int, default=0, help="no more punches thrown in the round than this (0: any; punching, the lean is let go)")
    a.add_argument("--min-distance", type=int, default=0, help="(experiments) never nearer to him than this (420: out of his punches' decisions)")
    o = a.parse_args()
    dirs = ["", "U", "D", "L", "R", "Q", "E", "Z", "C"]
    # (the right Shift, the controls' other button, 0x10: with a direction a footwork step, 1eb6:0668, standing and 420
    # or more from him)
    defense = [] if o.no_defense else ["!"]
    inputs = [row(d + b) for b in ["", "N"] + defense for d in dirs]
    moves = [row(d + b) for b in [""] + defense for d in dirs]
    emulator = {
        "Emulator Name": "QuickerOpen4DBoxing",
        "Game Path": "/home/jaffar/o4db-tas/disks",
        "Initial Sequence File Path": os.path.abspath(o.rows),
        "Max Delta Pages": o.pages,
        # (the pictures: the screen, the drawing buffers; tests/o4dbScramble: the game never reads them back; not
        # 51000-51FFF, which the fast fight reads: left unsaved, a search's states were not their solutions' replays)
        "Unsaved Memory": [["A0000", "AFFFF"], ["68000", "71FFF"], ["88000", "88FFF"]],
        "CPU Speed": o.cpu_speed,
        "Fast Fight": True,
    }
    if o.state: emulator["Initial State File Path"] = os.path.abspath(o.state)
    if o.test:
        # (the exhibition's red made The Champ of the Main Event's table, CHALLENG.DAT's first)
        emulator["Initial Speed"] = 22000
        emulator["Initial Pokes"] = pokes(*o.test) + opponent_pokes(0) + build_pokes(o.height, o.weight)
    fail = lambda label, prop, op, value: {"Label": label, "Conditions": [{"Property": prop, "Op": op, "Value": value}], "Satisfies": [],
                                           "Actions": [{"Type": "Trigger Fail"}]}
    script = {
        "Driver Configuration": {
            "End On First Win State": True,
            "Max Steps": o.steps,
            "Save Intermediate Results": {"Enabled": True, "Frequency (s)": 5.0, "Best Solution Path": "/tmp/jaffar.best.sol",
                                          "Worst Solution Path": "/tmp/jaffar.worst.sol"},
        },
        "Engine Configuration": {
            "State Database": {"Max Size (Mb)": o.db},
            "Hash Database": {"Max Store Count": 2, "Max Store Size (Mb)": o.hash, "Enabled": True},
        },
        "Emulator Configuration": emulator,
        "Game Configuration": {
            "Game Name": "DOS / 4D Sports Boxing",
            "Frame Rate": 10.0,
            "Print Properties": ["Round", "Rounds", "Round Clock", "Round Over", "Player Health", "Player Stamina", "Opponent Health", "Boxer Distance",
                                 "Player Pos X", "Player Pos Z", "Player Posture", "Player Guard", "Player Lean", "Opponent Lean", "Player Thrown", "Player Landed", "Player Round Landed",
                                 "Opponent Round Landed", "Player Damage", "Player Wasted Punches", "Player Judge 1", "Opponent Judge 1", "Step Frames"],
            "Hash Properties": [],
            "Rules": [
                {"Label": 1000, "Conditions": [], "Satisfies": [], "Actions": [
                    {"Type": "Set Survival Magnet", "Distance Weight": o.distance_weight, "Distance Cap": o.distance_cap, "Center Weight": o.center_weight,
                     "Landed Weight": o.landed_weight, "Landed Cap": o.landed_cap, "Lean Weight": o.lean_weight, "Lean Range": o.lean_range,
                     "Safe Weight": o.safe_weight, "Safe Distance": o.safe_distance}]},
                # (the round's end: won with his punches landed, else failed)
                {"Label": 1001, "Conditions": [{"Property": "Round Over", "Op": ">", "Value": 0},
                                               {"Property": "Player Round Landed", "Op": ">=", "Value": o.min_landed}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Win"}]},
                {"Label": 1002, "Conditions": [{"Property": "Round Over", "Op": ">", "Value": 0},
                                               {"Property": "Player Round Landed", "Op": "<", "Value": o.min_landed}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Fail"}]},
                # (never hit: a punch of his landed, his round's count, not the player's health, which his own punches cost
                # too; never a punch of his own that does not land, never floored, never warned)
                fail(1003, "Opponent Round Landed", ">", 0),
                fail(1004, "Player Wasted Punches", ">", 0),
                fail(1005, "Player Down", ">", 0),
                fail(1006, "Count Warnings", ">", 0),
            ] + ([fail(1007, "Player Round Thrown", ">", o.max_thrown)] if o.max_thrown else [])
              + ([fail(1008, "Boxer Distance", "<", o.min_distance)] if o.min_distance else []),
            "Bypass Emulator State": False,
        },
        "Runner Configuration": {
            "Hash Step Tolerance": 0,
            "Show Empty Input Slots": True,
            "Show Allowed Inputs": False,
            # (room for the engine's start-up calibration too: 200 steps of the first input)
            "Store Input History": {"Type": "Raw", "Max Size": max(o.steps + 50, 256)},
            "Allowed Input Sets": [
                {"Description": "Out of reach: moves", "Conditions": [{"Property": "Boxer Distance", "Op": ">", "Value": o.reach}], "Inputs": moves,
                 "Stop Input Evaluation": True},
                {"Description": "Moves and punches", "Conditions": [], "Inputs": inputs, "Stop Input Evaluation": False}],
            "Test Candidate Inputs": False,
            "Candidate Input Sets": [],
            "Bypass Hash Calculation": False,
            "Frameskip": {"Rate": 0, "Use Custom Input": False, "Custom Input": ""},
        },
    }
    json.dump(script, open(o.out, "w"), indent=2)


if __name__ == "__main__":
    main()
