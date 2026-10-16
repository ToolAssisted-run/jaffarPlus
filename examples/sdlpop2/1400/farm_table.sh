#!/bin/bash
# farm_table.sh H0 [KMAX]: room 3's potions measured from H0 maximum hit points (level 14's room 4 start): for each
# k = H0+1 .. KMAX (11) a stage search (from the first state with k-1 to the first with k, chained) and a return
# search (from that state back onto room 4's middle row). Results: farm_H0.txt; solutions s<H0>_<k>.sol, r<H0>_<k>.sol
H0=$1; KMAX=${2:-11}
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
J=../../../build/jaffar; L=~/pop2l3; OUT=farm_$H0.txt
cp init_r4.sol cur_$H0.sol; : > $OUT; total=0
run() {   # run TAG: one search, the progress links kept on it
  ln -sfn $L/l14$1.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$1.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$1.jaffar /tmp/claude.jaffar
  env OMP_NUM_THREADS=256 $J $1.jaffar > $L/l14$1.log 2>&1
  grep -a "Exit Reason" $L/l14$1.log | tail -1 | sed 's/.*Exit Reason: //'
}
for k in $(seq $((H0+1)) $KMAX); do
  n=$((2*(k-H0)+2))
  # (the stage as part of the route to KMAX: two big potions may lie at once; only the return's route ends at k)
  python3 gen.py farm --hp $H0 --to $KMAX --upto $n --anyframe --db 200000 --maxsteps 700 --init cur_$H0.sol --out s${H0}_$k.jaffar --tag s${H0}_$k
  rs=$(run s${H0}_$k); cp /tmp/jaffar.pop2l14.s${H0}_$k.best.sol s${H0}_$k.sol; ls=$(wc -l < s${H0}_$k.sol)
  case "$rs" in Solution*) ;; *) echo "max $k: stage FAILED ($rs after $ls)" >> $OUT; break;; esac
  total=$((total+ls)); cat cur_$H0.sol s${H0}_$k.sol > cur_$H0.tmp; mv cur_$H0.tmp cur_$H0.sol
  python3 gen.py farm --hp $H0 --to $k --upto $((n+2)) --db 200000 --maxsteps 600 --init cur_$H0.sol --out r${H0}_$k.jaffar --tag r${H0}_$k
  rr=$(run r${H0}_$k); cp /tmp/jaffar.pop2l14.r${H0}_$k.best.sol r${H0}_$k.sol; lr=$(wc -l < r${H0}_$k.sol)
  case "$rr" in Solution*) echo "max $k: stage $ls ticks (reached at $total), back in room 4 after $lr more: $((total+lr))" >> $OUT;;
                *) echo "max $k: stage $ls ticks (reached at $total), return FAILED ($rr after $lr)" >> $OUT;; esac
done
echo "ALL DONE" >> $OUT
