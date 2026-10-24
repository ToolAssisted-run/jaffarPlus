#!/usr/bin/env python3
"""Experiments: the time to the first knockdown of the exhibition's opponent (The Champ) for a set of statistics of the
player's boxer, put into the fight by pokes (its fight parameters as 1519:3522 computes them from the statistics).

  gen.py NAME POWER SPEED STAMINA [--db MB] [--steps N] [--inputs basic|all] [--pages N] [--opponent INDEX]
         [--knockdowns K] [--fail-damage] [--damage-weight W] [--wasted-weight W] [--endurance-weight W]

writes script-NAME.jaffar (initial sequence: ../test/exhibition-round1.sol, the round about to start at 29000).
"""
import argparse
import json

M = "<>UDLRQEZCKON_BXT^!%&abcdefghijklmnopqrstuvwxyz0123456789AFGHIJMPSV"  # (the last ten the function keys F1-F10)


def row(keys, x=135, y=65):
    return "|%5d,%5d," % (x, y) + "".join(m if m in keys else "." for m in M) + "|"


def pokes(power, speed, stamina, frame=28990):
    sp = 0x5334 - 38 * speed      # the speed parameter (the record's +0x30, the fight block's +0x0E)
    pw = 0x2CCC + 38 * power      # the power factor (+0x32, +0x10): the punches' damage
    en = 0x2CCC + 38 * stamina    # the endurance's maximum (+0x36, +0x14)
    p = []
    for off, v in ((0x5E1A, sp), (0x5E1C, pw), (0x5E20, en),     # the blue boxer's fight block (DS:5E0C)
                   (0xA7B8, sp), (0xA7BA, pw), (0xA7BE, en),     # its copy for the fight (the boxer's word 1)
                   (0xBA26, sp), (0xBA24, en)):                  # the boxer's own: the speed, the endurance now
        p.append([frame, "%04X" % off, "%04X" % v])
    return p


def opponent_pokes(index, frame=28990):
    """The red boxer made the CHALLENG.DAT boxer at index (0 The Champ .. 49 the 50th): his fight block (the record's
    bytes 0x22..0x87) into DS:5E72 and its fight copy (DS:A810), his endurance now (the block's maximum), his speed
    parameter and time step in his structure (BB06 + 0x14, + 0x50)"""
    import struct
    d = open("/home/jaffar/o4db-tas/game/CHALLENG.DAT" if __import__("os").path.exists("/home/jaffar/o4db-tas/game/CHALLENG.DAT") else
             "/home/jaffar/jaffarPlus/4dboxDecomp/game/CHALLENG.DAT", "rb").read()
    blk = d[index * 0x88 + 0x22: index * 0x88 + 0x88]
    p = []
    for k in range(0x33):
        v = struct.unpack_from("<H", blk, 2 * k)[0]
        p.append([frame, "%04X" % (0x5E72 + 2 * k), "%04X" % v])
        p.append([frame, "%04X" % (0xA810 + 2 * k), "%04X" % v])
    w = lambda k: struct.unpack_from("<H", blk, 2 * k)[0]
    p.append([frame, "%04X" % (0xBB06 + 0x12), "%04X" % w(10)])                   # endurance now
    p.append([frame, "%04X" % (0xBB06 + 0x14), "%04X" % w(7)])                    # speed parameter
    p.append([frame, "%04X" % (0xBB06 + 0x50), "%04X" % ((w(7) * 40 + 0x2000) >> 14)])  # time step
    return p


def main():
    a = argparse.ArgumentParser()
    a.add_argument("name")
    a.add_argument("power", type=int)
    a.add_argument("speed", type=int)
    a.add_argument("stamina", type=int)
    a.add_argument("--db", type=int, default=4000)
    a.add_argument("--steps", type=int, default=400)
    a.add_argument("--inputs", default="basic")
    a.add_argument("--pages", type=int, default=192)
    a.add_argument("--opponent", type=int, default=-1, help="CHALLENG.DAT index of the red boxer (-1: The Champ, the exhibition's)")
    a.add_argument("--distance", type=float, default=0.5)
    a.add_argument("--range", type=float, default=300.0)
    a.add_argument("--knockdowns", type=int, default=1, help="the win: the opponent's knockdowns in the round")
    a.add_argument("--fail-damage", action="store_true", help="a state where the player lost health fails")
    a.add_argument("--damage-weight", type=float, default=0.5, help="the cost of a point of the player's health lost")
    a.add_argument("--wasted-weight", type=float, default=200.0, help="the cost of a punch blocked or absorbed")
    a.add_argument("--endurance-weight", type=float, default=1.0, help="the reward of a point of the opponent's endurance lost")
    a.add_argument("--cpu-speed", type=int, default=220000, help="the PC's speed (DOSBox's cycles); the initial sequence was recorded at 22000")
    a.add_argument("--full", action="store_true", help="the whole game, not the fight's logic alone")
    o = a.parse_args()
    buttons = ["", "N"] if o.inputs == "basic" else ["", "N", "!"]
    dirs = ["", "U", "D", "L", "R", "Q", "E", "Z", "C"]
    inputs = [row(d + b) for b in buttons for d in dirs]
    # while a punch reaches out its button stays down (let go, the punch is pulled back for nothing); the direction is
    # free (a new one queues the next punch for when this one has landed)
    punching = [row(d + "N") for d in dirs]
    script = {
        "Driver Configuration": {
            "End On First Win State": True,
            "Max Steps": o.steps,
            "Save Intermediate Results": {"Enabled": True, "Frequency (s)": 1.0,
                                          "Best Solution Path": "/tmp/jaffar.best.sol", "Worst Solution Path": "/tmp/jaffar.worst.sol"},
        },
        "Engine Configuration": {
            "State Database": {"Max Size (Mb)": o.db},
            "Hash Database": {"Max Store Count": 2, "Max Store Size (Mb)": max(200, o.db // 10), "Enabled": True},
        },
        "Emulator Configuration": {
            "Emulator Name": "QuickerOpen4DBoxing",
            "Game Path": "/home/jaffar/o4db-tas/disks",
            "Initial Sequence File Path": "../test/exhibition-round1.sol",
            "Max Delta Pages": o.pages,
            "CPU Speed": o.cpu_speed,
            "Initial Speed": 22000,
            "Fast Fight": not o.full,
            # (the pictures: the screen, the drawing buffers; tests/o4dbScramble: the game never reads them back; not
        # 51000-51FFF, which the fast fight reads: left unsaved, a search's states were not their solutions' replays)
            "Unsaved Memory": [["A0000", "AFFFF"], ["68000", "71FFF"], ["88000", "88FFF"]],
            "Initial Pokes": pokes(o.power, o.speed, o.stamina) + (opponent_pokes(o.opponent) if o.opponent >= 0 else []),
        },
        "Game Configuration": {
            "Game Name": "DOS / 4D Sports Boxing",
            "Frame Rate": 10.0,
            "Print Properties": ["Round Clock", "Player Punching", "Player Punch Definition", "Player Health", "Player Stamina", "Player Endurance", "Opponent Health",
                                 "Opponent Stamina", "Opponent Endurance", "Opponent Posture", "Opponent Damage", "Player Damage", "Player Thrown", "Player Landed",
                                 "Player Wasted Punches", "Opponent Round Knockdowns", "Count", "Count Warnings", "Boxer Distance", "Step Frames"],
            "Hash Properties": [],
            "Rules": [
                {"Label": 1000, "Conditions": [], "Satisfies": [], "Actions": [
                    {"Type": "Set Fight Magnet", "Knockdown Weight": 30000.0, "Health Weight": 1.0, "Range": o.range, "Range Weight": o.distance * 2,
                     "Count Range": 650.0, "Stamina Weight": 0.05, "Endurance Weight": o.endurance_weight, "Damage Weight": o.damage_weight,
                     "Wasted Weight": o.wasted_weight}]},
                {"Label": 1001, "Conditions": [{"Property": "Opponent Round Knockdowns", "Op": ">=", "Value": o.knockdowns}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Win"}]},
                # (the referee's warnings: the third is a disqualification)
                {"Label": 1003, "Conditions": [{"Property": "Count Warnings", "Op": ">", "Value": 0}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Fail"}]},
                # (the player floored: no use, and the fast fight is the game's only while he stands)
                {"Label": 1004, "Conditions": [{"Property": "Player Down", "Op": ">", "Value": 0}], "Satisfies": [],
                 "Actions": [{"Type": "Trigger Fail"}]},
            ] + ([{"Label": 1002, "Conditions": [{"Property": "Player Damage", "Op": ">", "Value": 0}], "Satisfies": [],
                   "Actions": [{"Type": "Trigger Fail"}]}] if o.fail_damage else []),
            "Bypass Emulator State": False,
        },
        "Runner Configuration": {
            "Hash Step Tolerance": 0,
            "Show Empty Input Slots": True,
            "Show Allowed Inputs": False,
            "Store Input History": {"Type": "Raw", "Max Size": o.steps + 50},
            "Allowed Input Sets": [
                {"Description": "A punch reaching out: its button held", "Conditions": [{"Property": "Player Punching", "Op": "!=", "Value": 0}],
                 "Inputs": punching, "Stop Input Evaluation": True},
                {"Description": "Far from him: forward", "Conditions": [{"Property": "Boxer Distance", "Op": ">", "Value": 600}],
                 "Inputs": [row(""), row("U"), row("Q"), row("E")], "Stop Input Evaluation": True},
                {"Description": "Moves and punches", "Conditions": [], "Inputs": inputs, "Stop Input Evaluation": False}],
            "Test Candidate Inputs": False,
            "Candidate Input Sets": [],
            "Bypass Hash Calculation": False,
            "Frameskip": {"Rate": 0, "Use Custom Input": False, "Custom Input": ""},
        },
    }
    json.dump(script, open("script-%s.jaffar" % o.name, "w"), indent=2)


if __name__ == "__main__":
    main()
