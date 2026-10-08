#!/bin/bash
# run_level.sh DIR SCRIPT TAG [DB_MB]: one search of the final run (the tree ~/jaffarPlus-run on jaffanator2: the core
# built on SDLPoP2's latest, the level ends waiting for their music as in DOS), the whole machine, the state database
# DB_MB (default 300000). The script's database size and solution paths are set here; the log is ~/pop2run/TAG.log,
# the solution DIR/TAG.sol, a line in ~/pop2run/results.txt; /tmp/claude.{log,best.sol,jaffar} follow the run.
R=~/jaffarPlus-run; cd $R/examples/sdlpop2/$1 || exit 1
S=$2; T=$3; DB=${4:-300000}; mkdir -p ~/pop2run
python3 - "$S" "$T" "$DB" <<'P'
import json, sys
s, t, db = sys.argv[1], sys.argv[2], int(sys.argv[3])
j = json.load(open(s))
j["Engine Configuration"]["State Database"]["Max Size (Mb)"] = db
r = j["Driver Configuration"]["Save Intermediate Results"]
r["Best Solution Path"] = "/tmp/jaffar.run.%s.best.sol" % t; r["Worst Solution Path"] = "/tmp/jaffar.run.%s.worst.sol" % t
json.dump(j, open("run-%s.jaffar" % t, "w"), indent=2)
P
ln -sfn ~/pop2run/$T.log /tmp/claude.log; ln -sfn /tmp/jaffar.run.$T.best.sol /tmp/claude.best.sol; ln -sfn $PWD/run-$T.jaffar /tmp/claude.jaffar
env OMP_NUM_THREADS=256 $R/build/jaffar run-$T.jaffar > ~/pop2run/$T.log 2>&1
cp /tmp/jaffar.run.$T.best.sol $T.sol
echo "$(date +%F\ %T) $T: $(grep -a 'Exit Reason' ~/pop2run/$T.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $T.sol) ticks ($DB MB)" >> ~/pop2run/results.txt
