#!/bin/bash
# mkref.sh TAG N SOL [CHAIN [HP [LATE]]]: the reference of watch_level.sh's "room entered late" alarm for the search
# TAG of level N, ~/pop2run/TAG.ref: the rooms of an earlier solution SOL of the level and the ticks it enters them
# on. SOL is a file of the exploration tree's NN00 directory (~/jaffarPlus-pop2/examples/sdlpop2 on jaffanator2),
# replayed on its own chain (CHAIN, default level0102..(N-1).sol) with its own core (~/pop2tools/trl, built on that
# tree; "Max HP At Level" [N, HP], default 5): the old solutions do not replay on the run's chain or core.
# With SOL "-" the lines "ROOM TICK" are read from the standard input instead.
set -e
T=$1; N=$2; SOL=$3; NN=$(printf %02d $N)
nums() { for i in $(seq 1 $1); do printf %02d $i; done; }
CHAIN=${4:-level$(nums $((N - 1))).sol}; HP=${5:-5}; LATE=${6:-30}
O=$HOME/jaffarPlus-pop2/examples/sdlpop2/${NN}00; R=$HOME/pop2run/$T.ref
echo "# ${NN}00 l$(nums $((N - 1)))cut.sol $LATE" > $R
if [ "$SOL" = "-" ]; then cat >> $R
else (cd $O && $HOME/pop2tools/trl prince2 7881336 $N $HP $CHAIN $SOL 2>/dev/null) | awk -v lv=L$N 'NR>1 && $3==lv && $5 != r { print $5, $1; r = $5 }' >> $R; fi
tr "\n" ";" < $R; echo
