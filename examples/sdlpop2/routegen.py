"""Route scripts for the PoP2 levels (each level's gen.py describes its routes and calls main()).

A route is a list of steps. Each step is a rule: its conditions (all ANDed) add a reward and set the X / Y magnets of
the rooms it names.

- The room a step leaves gets a -5000 X magnet, so going back costs more than any magnet gains (a mag with a 6th
  element, "keep", is kept as written).
- A max HP condition holds for every later step. A later step would otherwise satisfy the potion's step through the
  chain without the potion.
- Rooms off the route fail. A room with no magnet would otherwise be free of any magnet cost.
- With the level's "penalize_ahead": every room of the route other than the step's own (and those it sets a magnet for)
  gets the -5000 X magnet too: a room the route enters later (or left earlier) scores 0 without a magnet, more than any
  state in the step's room, and the search drifted there (level 6: the head's push into room 7 while climbing past it).
- `--upto N` makes the N-th step (1-based) the win, with the prince standing, for solving a route in chunks.
"""
import argparse, copy, json, os

L, R = -200, 800     # X magnet positions beyond a room's left / right edge (col c stands at x ~ 154 + 32 c)
U, D = -50, 300      # Y magnet positions above row 0 / below row 2 (y 55, 119, 181)
ROW_Y = (56, 119, 182)


def col_x(c):
    return 154 + 32 * c


def room(r, *extra):
    return [("Player Room", "==", r)] + list(extra)


def hp(n):
    return ("Player Max HP", "==", n)


def row(r):
    return ("Player Current Row", "==", r)


def step(cond, *mags, opp=None, hpm=None):
    """mags: (room, x or None, y or None[, x intensity, y intensity[, "keep"]]); opp: the Opponent HP Magnet's intensity
    (per hit point the opponent lost); hpm: the Player HP Magnet's (per hit point the prince has). Both hold for the
    later steps until one sets them again."""
    return {"cond": cond, "mags": mags, "opp": opp, "hpm": hpm}


# the prince steps into an open exit door (Up, standing by it: control.c control_standing_up, sequence 0x46); the
# level ends ~22 ticks later
ENTER = step([("Player Current Sequence", "==", 0x46)])


def dedup(steps):
    """drop a step whose conditions repeat the previous one's"""
    out = []
    for s in steps:
        if out and out[-1]["cond"] == s["cond"]:
            out[-1] = s
        else:
            out.append(s)
    return out


def build(lv, route, upto, db, init, tag, maxsteps):
    here = os.path.dirname(os.path.abspath(__file__))
    j = copy.deepcopy(json.load(open(os.path.join(here, "0100/script.jaffar"))))
    j["Driver Configuration"]["Save Intermediate Results"]["Best Solution Path"] = f"/tmp/jaffar.pop2{lv['name']}.{tag}.best.sol"
    j["Driver Configuration"]["Save Intermediate Results"]["Worst Solution Path"] = f"/tmp/jaffar.pop2{lv['name']}.{tag}.worst.sol"
    j["Driver Configuration"]["Max Steps"] = maxsteps
    j["Engine Configuration"]["State Database"]["Max Size (Mb)"] = db
    e = j["Emulator Configuration"]
    e["Start Level"] = 1
    e["Seed"] = 7881336   # the restart combo's level 1 seed (Chimera Random Seed 25)
    e["Skip Story Scenes"] = True   # (Space skips them: after level 5 the sound stops, and level 6 starts with a draw)
    e["Initial Sequence File Path"] = init
    g = j["Game Configuration"]
    g["Print Properties"] = ["Current Level", "Next Level", "Player Room", "Player Pos X", "Player Pos Y", "Player Frame",
                             "Player Action", "Player Current Row", "Player Current Col", "Player Current HP", "Player Max HP"]
    g["Restart At Checkpoint"] = False
    # gates and exit doors: their openings are in the tile modifiers, which are not hashed otherwise
    g["Hash Tile Modifiers"] = lv["hash"]
    if lv.get("hash_visited"): g["Hash Visited Rooms"] = lv["hash_visited"]   # the visited flags a rule reads
    rules = []
    steps = lv["routes"][route]
    on = {c[2] for st in steps for c in st["cond"] if c[0] == "Player Room"} | {m[0] for st in steps for m in st["mags"]}
    n = len(steps) if upto is None else upto
    prev = None   # the room of the previous step
    need = None   # the max HP asked for so far
    for i, s in enumerate(steps[:n]):
        hps = [c for c in s["cond"] if c[0] == "Player Max HP"]
        if hps: need = hps[0]
        elif need is not None: s = dict(s, cond=s["cond"] + [need])
        acts = [{"Type": "Add Reward", "Value": 1000.0}]
        own = s["cond"][0][2] if s["cond"] and s["cond"][0][0] == "Player Room" else prev
        mags = list(s["mags"])
        if prev is not None and own != prev and prev not in [m[0] for m in mags]: mags.append((prev, None, None))
        if lv.get("penalize_ahead"): mags += [(r, None, None) for r in sorted(on) if r != own and r not in [m[0] for m in mags]]
        prev = own
        for m in mags:
            r, x, y = m[0], m[1], m[2]
            xi, yi = (m[3], m[4]) if len(m) > 3 else (1.0, 1.0)
            if r != own and len(m) < 6: x, y, xi = -5000, None, 1.0
            if x is not None: acts.append({"Type": "Set Player Pos X Magnet", "Room": r, "Intensity": xi, "Position": x})
            if y is not None: acts.append({"Type": "Set Player Pos Y Magnet", "Room": r, "Intensity": yi, "Position": y})
        if s.get("opp") is not None: acts.append({"Type": "Set Opponent HP Magnet", "Intensity": s["opp"]})
        if s.get("hpm") is not None: acts.append({"Type": "Set Player HP Magnet", "Intensity": s["hpm"]})
        conds = [{"Property": p, "Op": o, "Value": v} for p, o, v in s["cond"]]
        rules.append({"Label": 3000 + i, "Conditions": conds, "Satisfies": [3000 + i - 1] if i else [], "Actions": acts})
        if upto is not None and i == n - 1:   # a chunk ends with the prince standing still (frame 15), not in a fall he may not survive
            rules.append({"Label": 3899, "Conditions": conds + [{"Property": "Player Frame", "Op": "==", "Value": 15}], "Satisfies": [],
                          "Actions": [{"Type": "Trigger Win"}]})
    if upto is None:
        rules.append({"Label": 3900, "Conditions": [{"Property": "Next Level", "Op": "==", "Value": lv["next"]}], "Satisfies": [], "Actions": [{"Type": "Trigger Win"}]})
    rules.append({"Label": 3901, "Conditions": [{"Property": "Player Is Alive", "Op": ">=", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3902, "Conditions": [{"Property": "Player Current HP", "Op": "==", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3903, "Conditions": [{"Property": "Player Room", "Op": "==", "Value": 0}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    rules.append({"Label": 3904, "Conditions": [{"Property": "Player Max HP", "Op": ">", "Value": lv["maxhp"][route]}], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    for k, conds in enumerate(lv.get("fails", {}).get(route, [])):
        rules.append({"Label": 3905 + k, "Conditions": [{"Property": p, "Op": o, "Value": v} for p, o, v in conds], "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    for r in range(1, 33):
        if r not in on:
            rules.append({"Label": 3910 + r, "Conditions": [{"Property": "Player Room", "Op": "==", "Value": r},
                                                            {"Property": "Current Level", "Op": "==", "Value": lv["next"] - 1}],   # (not the next level's rooms)
                          "Satisfies": [], "Actions": [{"Type": "Trigger Fail"}]})
    g["Rules"] = rules
    return j


def main(lv):
    a = argparse.ArgumentParser()
    a.add_argument("route", choices=list(lv["routes"]))
    a.add_argument("--upto", type=int)
    a.add_argument("--db", type=int, default=15000)
    a.add_argument("--init", default=lv["init"])
    a.add_argument("--out")
    a.add_argument("--tag")
    a.add_argument("--maxsteps", type=int, default=3000)
    o = a.parse_args()
    tag = o.tag or o.route
    json.dump(build(lv, o.route, o.upto, o.db, o.init, tag, o.maxsteps), open(o.out or f"script-{o.route}.jaffar", "w"), indent=2)
