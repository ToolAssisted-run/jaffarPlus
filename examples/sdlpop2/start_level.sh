#!/bin/bash
# start_level.sh N ROUTE TAG MAXSTEPS [DB_MB] : the final run's level N launched on jaffanator2 (ROUTING.md "The final
# run"): the route's script made for the run (gen.py ROUTE --run: the run's hit points, the music on at the start, the
# win on the next level loaded) from the chain NN00/l0102..(N-1)cut.sol (post_level.sh's cut stage of the level
# before), and run_level.sh with it (one search at a time: it refuses while a jaffar runs). Prints the script's
# checks and the search's first steps; the watcher is watch_level.sh TAG.
set -e
N=$1; ROUTE=$2; TAG=$3; MAX=$4; DB=${5:-300000}
S=$HOME/jaffarPlus-run/examples/sdlpop2
NN=$(printf %02d $N); D=$S/${NN}00
nums() { for i in $(seq 1 $1); do printf %02d $i; done; }
CHAIN=l$(nums $((N - 1)))cut.sol; SCRIPT=script-$ROUTE-run.jaffar
cd $D
[ -f $CHAIN ] || { echo "no chain $D/$CHAIN"; exit 1; }
if pidof jaffar > /dev/null; then echo "a jaffar is running: $(pidof jaffar)"; exit 1; fi
# (env PREFIX: the level's first lines kept from an earlier solution (tools/reuse.py, version 2: a level replayed on
# another run before it, up to where it differs): the search starts after them, its solution TAG.sol is the rest,
# and the level's is TAG.prefix.sol + TAG.sol)
INIT=$CHAIN
if [ -n "$PREFIX" ]; then cat $CHAIN $PREFIX > init_$TAG.sol; cp $PREFIX $TAG.prefix.sol; INIT=init_$TAG.sol; fi
python3 gen.py $ROUTE --run --init $INIT --db $DB --maxsteps $MAX --out $SCRIPT
echo "chain $INIT: $(grep -c '^|' $INIT) lines; script $SCRIPT: $(grep -c '"Label"' $SCRIPT) rules"
grep -o -E '"Music On At Start": *[a-z]+|"Max Step Count": *[0-9]+|"Initial Sequence File Path": *"[^"]*"|"Max HP At Level"[^]]*\]' $SCRIPT | tr "\n" ";"; echo
nohup ../run_level.sh ${NN}00 $SCRIPT $TAG $DB > /dev/null 2>&1 &
sleep ${WAIT:-75}
L=$HOME/pop2run/$TAG.log
ls -la /tmp/claude.log /tmp/claude.best.sol /tmp/claude.jaffar | sed -E "s/.* (\/tmp\/claude)/\1/"
grep -a "Current Step #" $L | tail -1
grep -a -E "\[Prince\]|Rule Status|Level:" $L | tail -3
