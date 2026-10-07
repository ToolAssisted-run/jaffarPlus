#!/bin/bash
# q3.sh: after the left entry's fight: room 6 to room 7's middle platform again, kept off room 6's bottom row (r7C2);
# then the fights from 10 ticks before each entry's end (TRIM); then the right entry with room 7's top row free (fb).
# One search at a time. Results: seg7.txt, fightk.txt
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
while ! grep -q "ALL DONE" fightk.txt 2>/dev/null; do sleep 20; done
t=r7C2
python3 gen.py r7C --hp 3 --upto 8 --db 200000 --maxsteps 420 --init init_e1.sol --out $t.jaffar --tag $t
ln -sfn ~/pop2l3/l14$t.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$t.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$t.jaffar /tmp/claude.jaffar
env OMP_NUM_THREADS=256 ../../../build/jaffar $t.jaffar > ~/pop2l3/l14$t.log 2>&1
cp /tmp/jaffar.pop2l14.$t.best.sol $t.sol
echo "$t: $(grep -a 'Exit Reason' ~/pop2l3/l14$t.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $t.sol) ticks | $(tail -80 ~/pop2l3/l14$t.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> seg7.txt
grep -q "^r7C2: Solution found" seg7.txt && TRIM=10 ./fightk.sh C2 320 fb8 7
TRIM=10 ./fightk.sh R 320 fb8 7
TRIM=10 ./fightk.sh L 340 fb8 7
./fightk.sh R 300 fb 7
echo "QUEUE DONE" >> fightk.txt
