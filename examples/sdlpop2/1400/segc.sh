#!/bin/bash
# segc.sh: room 6 by its left platform, facing right, up onto room 7's middle platform (r7C: Sergio), then the Jaffar
# fight from there (plan B, fightk.sh). One search at a time. Results: seg7.txt, fightk.txt
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
t=r7C
ln -sfn ~/pop2l3/l14$t.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$t.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$t.jaffar /tmp/claude.jaffar
env OMP_NUM_THREADS=256 ../../../build/jaffar $t.jaffar > ~/pop2l3/l14$t.log 2>&1
cp /tmp/jaffar.pop2l14.$t.best.sol $t.sol
echo "$t: $(grep -a 'Exit Reason' ~/pop2l3/l14$t.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $t.sol) ticks | $(tail -80 ~/pop2l3/l14$t.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> seg7.txt
grep -q "^r7C: Solution found" seg7.txt && ./fightk.sh C 300 fb8 7
echo "SEGC DONE" >> fightk.txt
