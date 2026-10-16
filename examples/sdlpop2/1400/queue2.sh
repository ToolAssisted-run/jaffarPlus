#!/bin/bash
# queue2.sh: after the right-entry fight (running): the fight from the left entry, then room 6 to room 7's middle
# platform by the left one (r7C). One search at a time. Results: seg7.txt, fight.txt
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
J=../../../build/jaffar
run() {   # run TAG
  ln -sfn ~/pop2l3/l14$1.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$1.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$1.jaffar /tmp/claude.jaffar
  env OMP_NUM_THREADS=256 $J $1.jaffar > ~/pop2l3/l14$1.log 2>&1
  cp /tmp/jaffar.pop2l14.$1.best.sol $1.sol
  echo "$(grep -a 'Exit Reason' ~/pop2l3/l14$1.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $1.sol) ticks | $(tail -80 ~/pop2l3/l14$1.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)"
}
while pgrep -x jaffar >/dev/null; do sleep 15; done
cp /tmp/jaffar.pop2l14.fR.best.sol fR.sol
echo "entry R: $(grep -a 'Exit Reason' ~/pop2l3/l14fR.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < fR.sol) ticks | $(tail -80 ~/pop2l3/l14fR.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> fight.txt
# (Sergio: the two fights first, untouched; the middle-platform variant after them)
for s in L; do
  cat init_e1.sol r7$s.sol > init_f$s.sol
  python3 gen.py end --hp 3 --db 200000 --maxsteps 520 --init init_f$s.sol --out f$s.jaffar --tag f$s
  echo "entry $s: $(run f$s)" >> fight.txt
done
echo "ALL DONE" >> fight.txt
echo "r7C: $(run r7C)" >> seg7.txt
echo "r7C DONE" >> seg7.txt
