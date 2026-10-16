#!/bin/bash
# join.sh N TAG OUT.sol : version 2, after segment.sh's search TAG has won: the level's lines so far (TAG.prefix.sol),
# the search's (TAG.sol) and the old solution's after the join (~/pop2tas/v1-work/lNNcut.sol past the line in
# TAG.join) as NN00/OUT.sol, and tools/reuse.py on it from the join on: the next difference, or the level good.
set -e
N=$1; TAG=$2; OUT=$3
S=$HOME/jaffarPlus-run/examples/sdlpop2; V1=$HOME/pop2tas/v1-work; SEED=7881336
NN=$(printf %02d $N); D=$S/${NN}00
nums() { for i in $(seq 1 $1); do printf %02d $i; done; }
CHAIN=l$(nums $((N - 1)))cut.sol
cd $D
read ROLD WIN < $TAG.join
P=$(grep -c "^|" $TAG.prefix.sol); G=$(grep -c "^|" $TAG.sol)
cat $TAG.prefix.sol $TAG.sol > $OUT; tail -n +$((ROLD + 1)) $V1/l${NN}cut.sol >> $OUT
echo "$OUT: $P lines kept, $G searched (the old solution's $((ROLD - P))), $(($(grep -c "^|" $OUT) - P - G)) old lines after the join; $(grep -c "^|" $OUT) in all (the old $(grep -c "^|" $V1/l${NN}cut.sol))"
python3 ../tools/reuse.py prince2 $SEED $V1/$CHAIN $CHAIN $V1/l${NN}cut.sol $OUT $((P + G)) $ROLD
