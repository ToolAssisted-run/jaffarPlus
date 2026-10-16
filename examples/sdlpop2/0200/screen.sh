#!/bin/bash
# screen.sh DB_MB COND...: level 2 from the final run's level 1 (l01chain.sol: its 433 ticks and the 105 of the
# level-end music) under Random Seed settings that give each puzzle (COND = variant/tile:setting, from
# tools/seedscan.c), one search after another (run_level.sh): which puzzle is the fastest. Results in
# ~/pop2run/results.txt (tags l02_V_T_dDB).
cd ~/jaffarPlus-run/examples/sdlpop2/0200 || exit 1
DB=$1; shift
for c in "$@"; do
  p=${c%%:*}; s=${c##*:}; t=l02_${p/\//_}_d$DB
  python3 - "$s" "$t" <<'P'
import json, sys
s, t = int(sys.argv[1]), sys.argv[2]
j = json.load(open("script.jaffar")); e = j["Emulator Configuration"]
e["Seed"] = (s * 214013 + 2531011) % 2**32          # (the game advances the setting once at start-up)
e["Initial Sequence File Path"] = "l01chain.sol"
e.pop("Override RNG Enabled", None); e.pop("Override RNG Value", None)
json.dump(j, open("s-%s.jaffar" % t, "w"), indent=2)
P
  ../run_level.sh 0200 s-$t.jaffar $t $DB
done
echo "$(date +%F\ %T) screen done ($DB MB: $*)" >> ~/pop2run/results.txt
