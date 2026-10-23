#!/usr/bin/env python3
"""A test's pokes file (tests/pokes.h: "FRAME OFFSET VALUE" lines) for the exhibition's round against The Champ: the
player's statistics and build as round.py's --test, --height and --weight put them in.

  pokesfile.py OUT POWER SPEED STAMINA [--height H] [--weight W]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "experiments"))
from gen import pokes, opponent_pokes  # noqa: E402
from round import build_pokes  # noqa: E402

a = argparse.ArgumentParser()
a.add_argument("out")
a.add_argument("power", type=int)
a.add_argument("speed", type=int)
a.add_argument("stamina", type=int)
a.add_argument("--height", type=int)
a.add_argument("--weight", type=int)
o = a.parse_args()
with open(o.out, "w") as f:
    for frame, off, value in pokes(o.power, o.speed, o.stamina) + opponent_pokes(0) + build_pokes(o.height, o.weight):
        f.write("%d %s %s\n" % (frame, off, value))
