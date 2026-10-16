#!/bin/bash
# The five level 3 routes, full scripts, 100 GB state database, one after another (log ~/pop2l3/big-ROUTE.log)
B=~/pop2l3; cd ~/jaffarPlus-pop2/examples/sdlpop2/0300
for r in left right p1 p2 both; do
  [ -f $B/big-$r.done ] && continue
  start=$(date +%s)
  nice -n 10 env OMP_NUM_THREADS=256 ../../../build/jaffar script-$r.jaffar > $B/big-$r.log 2>&1
  end=$(date +%s)
  echo "$r: $(grep -E 'Exit Reason' $B/big-$r.log | tail -1) ($((end-start)) s)" >> $B/big-summary.txt
  cp /tmp/jaffar.pop2l3.$r.best.sol $B/big-$r.sol
  touch $B/big-$r.done
done
echo ALL DONE >> $B/big-summary.txt
