#!/usr/bin/env python3
"""mkproject.py PACKAGE MAKE_PROJECT.py MOVIE OUT.chimeraProject [FRAME:TEXT ...]: the run so far as a Chimera project.

MOVIE is one row a frame in the core's letters (U D L R S C, _ Show Time / Space, a Restart Level, . nothing), from
the boot. The project is written by the core's own waterbox/tests/make-project.py (settings, the game's files and the
MT-32's ROMs pinned by SHA-1 as firmware, the input log), then given its title, description and a marker per level.
Settings: the Roland MT-32 (music roland, Sergio 2026-10-08) and the Random Seed that ties level 1 to level 2's sand
trick (25); everything else the core's default (release 1.1, cheats off, the in-game time counted from level 1)."""
import json, subprocess, sys
package, maker, movie, out = sys.argv[1:5]
marks = [(int(m.split(":", 1)[0]), m.split(":", 1)[1]) for m in sys.argv[5:]]
rows = [l.rstrip("\n") for l in open(movie) if l.strip()]
tmp = out + ".movie"
open(tmp, "w").write("\n".join("" if r == "." else r for r in rows) + "\n")
subprocess.check_call([sys.executable, maker, package, out, str(len(rows)), "--movie", tmp,
                       "--settings", json.dumps({"music": "roland", "random_seed": 25})])
import os; os.remove(tmp)
p = json.load(open(out))
p["id"] = "pop2-tas"
p["title"] = "Prince of Persia 2: The Shadow and the Flame (DOS 1.1) - TAS in progress"
p["description"] = ("JaffarPlus / QuickerSDLPoP2 searches, level by level (examples/sdlpop2/ROUTING.md). "
                    "In-game time counted from level 1. %d frames so far: %s."
                    % (len(rows), marks[-1][1] if marks else "boot"))
p["markers"] = [{"frame": f, "text": t} for f, t in sorted(marks)]
json.dump(p, open(out, "w"), indent="\t")
print(out, len(rows), "frames,", len(p["markers"]), "markers, core", p["core"]["version"][:10], "settings", {k: p["settings"][k] for k in ("version", "music", "random_seed", "cheats", "igt_from_level_1")}, len(p["firmware"]), "firmware pins")
