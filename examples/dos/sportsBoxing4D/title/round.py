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


def main():
    a = argparse.ArgumentParser()
    a.add_argument("out")
    a.add_argument("--rows", required=True, help="the movie's rows to the round's start (Chimera's)")
    a.add_argument("--state", help="the machine's state after them (o4dbBench save), loaded instead of playing them")
    a.add_argument("--test", nargs=3, type=int, metavar=("POWER", "SPEED", "STAMINA"),
                   help="an experiment: the rows recorded at 22,000, the player's statistics poked in")
    a.add_argument("--cpu-speed", type=int, default=220000)
    a.add_argument("--steps", type=int, default=1900, help="a round's 3 minutes are 1800 steps")
    a.add_argument("--db", type=int, default=60000)
    a.add_argument("--hash", type=int, default=8000)
    a.add_argument("--pages", type=int, default=192)
    a.add_argument("--min-landed", type=int, default=1)
    a.add_argument("--distance-weight", type=float, default=1.0)
    a.add_argument("--distance-cap", type=float, default=700.0)
    a.add_argument("--center-weight", type=float, default=0.5)
    a.add_argument("--landed-weight", type=float, default=2000.0)
    a.add_argument("--landed-cap", type=float, default=3.0)
    o = a.parse_args()
    dirs = ["", "U", "D", "L", "R", "Q", "E", "Z", "C"]
    inputs = [row(d + b) for b in ["", "N"] for d in dirs]
    emulator = {
        "Emulator Name": "QuickerOpen4DBoxing",
        "Game Path": "/home/jaffar/o4db-tas/disks",
        "Initial Sequence File Path": os.path.abspath(o.rows),
        "Max Delta Pages": o.pages,
        # (the pictures: the screen, the drawing buffers; tests/o4dbScramble: the game never reads them back)
        "Unsaved Memory": [["A0000", "AFFFF"], ["68000", "71FFF"], ["88000", "88FFF"], ["51000", "51FFF"]],
        "CPU Speed": o.cpu_speed,
        "Fast Fight": True,
    }
    if o.state: emulator["Initial State File Path"] = os.path.abspath(o.state)
    if o.test:
        # (the exhibition's red made The Champ of the Main Event's table, CHALLENG.DAT's first)
        emulator["Initial Speed"] = 22000
        emulator["Initial Pokes"] = pokes(*o.test) + opponent_pokes(0)
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
                                 "Player Pos X", "Player Pos Z", "Opponent Lean", "Player Thrown", "Player Landed", "Player Round Landed",
                                 "Opponent Round Landed", "Player Damage", "Player Wasted Punches", "Player Judge 1", "Opponent Judge 1", "Step Frames"],
            "Hash Properties": [],
            "Rules": [
                {"Label": 1000, "Conditions": [], "Satisfies": [], "Actions": [
                    {"Type": "Set Survival Magnet", "Distance Weight": o.distance_weight, "Distance Cap": o.distance_cap, "Center Weight": o.center_weight,
                     "Landed Weight": o.landed_weight, "Landed Cap": o.landed_cap}]},
                # (the round's end: won with his punches landed, else failed)
                {"Label": 1001, "Conditions": [{"Property": "Round Over", "Op": ">", "Value": 0},
                                               {"Property": "Player Round Landed", "Op": ">=", "Value": o.min_landed}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Win"}]},
                {"Label": 1002, "Conditions": [{"Property": "Round Over", "Op": ">", "Value": 0},
                                               {"Property": "Player Round Landed", "Op": "<", "Value": o.min_landed}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Fail"}]},
                # (never hit, never a punch that does not land, never floored, never warned)
                fail(1003, "Player Damage", ">", 0),
                fail(1004, "Player Wasted Punches", ">", 0),
                fail(1005, "Player Down", ">", 0),
                fail(1006, "Count Warnings", ">", 0),
            ],
            "Bypass Emulator State": False,
        },
        "Runner Configuration": {
            "Hash Step Tolerance": 0,
            "Show Empty Input Slots": True,
            "Show Allowed Inputs": False,
            "Store Input History": {"Type": "Raw", "Max Size": o.steps + 50},
            "Allowed Input Sets": [{"Description": "Moves and punches", "Conditions": [], "Inputs": inputs, "Stop Input Evaluation": False}],
            "Test Candidate Inputs": False,
            "Candidate Input Sets": [],
            "Bypass Hash Calculation": False,
            "Frameskip": {"Rate": 0, "Use Custom Input": False, "Custom Input": ""},
        },
    }
    json.dump(script, open(o.out, "w"), indent=2)


if __name__ == "__main__":
    main()
