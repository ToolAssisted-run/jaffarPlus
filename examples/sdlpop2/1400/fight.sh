#!/bin/bash
# fight.sh: the Jaffar fight from room 7's two entries (after r7R.sol / r7L.sol), one after another: results in fight.txt
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
: > fight.txt
for s in R L; do
  cat init_e1.sol r7$s.sol > init_f$s.sol
  python3 gen.py end --hp 3 --db 200000 --maxsteps 520 --init init_f$s.sol --out f$s.jaffar --tag f$s
  ln -sfn ~/pop2l3/l14f$s.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.f$s.best.sol /tmp/claude.best.sol; ln -sfn $PWD/f$s.jaffar /tmp/claude.jaffar
  env OMP_NUM_THREADS=256 ../../../build/jaffar f$s.jaffar > ~/pop2l3/l14f$s.log 2>&1
  cp /tmp/jaffar.pop2l14.f$s.best.sol f$s.sol
  echo "entry $s: $(grep -a 'Exit Reason' ~/pop2l3/l14f$s.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < f$s.sol) ticks | $(tail -80 ~/pop2l3/l14f$s.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> fight.txt
done
echo "ALL DONE" >> fight.txt
