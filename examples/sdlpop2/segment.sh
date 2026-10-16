#!/bin/bash
# segment.sh N ROUTE TAG CUR.sol S ROLD MAXSTEPS [DB_MB] : version 2, a stretch of level N searched anew on jaffanator2
# (ROUTING.md "Version 2"): the level's old solution (~/pop2tas/v1-work/lNNcut.sol) no longer plays the same after
# the new run before it (tools/reuse.py: a fight's random numbers), so the search starts after the first S lines of
# CUR.sol (the level so far: the old solution, or one already joined) and wins on the prince's state after the old
# solution's line ROLD (tools/trj's fields: routegen.py --winstate), a state past the fight that does not depend on
# it. join.sh then puts the old lines after ROLD behind it. One search at a time (run_level.sh).
set -e
N=$1; ROUTE=$2; TAG=$3; CUR=$4; SS=$5; ROLD=$6; MAX=$7; DB=${8:-300000}
S=$HOME/jaffarPlus-run/examples/sdlpop2; V1=$HOME/pop2tas/v1-work; SEED=7881336
NN=$(printf %02d $N); D=$S/${NN}00
nums() { for i in $(seq 1 $1); do printf %02d $i; done; }
CHAIN=l$(nums $((N - 1)))cut.sol
cd $D
if pidof jaffar > /dev/null; then echo "a jaffar is running: $(pidof jaffar)"; exit 1; fi
head -n $SS $CUR > $TAG.prefix.sol; cat $CHAIN $TAG.prefix.sol > init_$TAG.sol
WIN=$($HOME/pop2tools-m/trj prince2 $SEED 0 0 $V1/$CHAIN $V1/l${NN}cut.sol 2>/dev/null | awk -v r=$ROLD -F"|" '$1 ~ "^" r " " { split($2, a, " "); s = a[1]; for (i = 2; i <= 14; i++) s = s "," a[i]; print s }')
[ -n "$WIN" ] || { echo "no old line $ROLD"; exit 1; }
echo "$ROLD $WIN" > $TAG.join
# (env WINCOND: more conditions of the win, "Property,op,value" separated by ";": e.g. "Room Chars[26],==,0", the
# guard the old solution left behind gone as it was there)
MORE=(); IFS=";" read -ra CS <<< "$WINCOND"; for c in "${CS[@]}"; do MORE+=("--wincond=$c"); done
python3 gen.py $ROUTE --run --init init_$TAG.sol --db $DB --maxsteps $MAX --winstate=$WIN "${MORE[@]}" --out script-$TAG-run.jaffar
[ -n "$WINCOND" ] && echo "and: $WINCOND"
echo "start: after $SS lines of $CUR ($(grep -c '^|' init_$TAG.sol) with the chain); the win: the old line $ROLD's state $WIN; $(grep -c '"Label"' script-$TAG-run.jaffar) rules"
nohup ../run_level.sh ${NN}00 script-$TAG-run.jaffar $TAG $DB > /dev/null 2>&1 &
sleep ${WAIT:-60}
L=$HOME/pop2run/$TAG.log
grep -a "Current Step #" $L | tail -1
grep -a -E "\[Prince\]|Rule Status" $L | tail -2
