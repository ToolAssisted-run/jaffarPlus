#!/bin/bash
# seg7.sh: room 6's three variants, one after another (from the spirit in room 4): results in seg7.txt
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
: > seg7.txt
for t in r7R r7L r7L2; do
  ln -sfn ~/pop2l3/l14$t.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$t.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$t.jaffar /tmp/claude.jaffar
  env OMP_NUM_THREADS=256 ../../../build/jaffar $t.jaffar > ~/pop2l3/l14$t.log 2>&1
  cp /tmp/jaffar.pop2l14.$t.best.sol $t.sol
  echo "$t: $(grep -a 'Exit Reason' ~/pop2l3/l14$t.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $t.sol) ticks | $(tail -80 ~/pop2l3/l14$t.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> seg7.txt
done
echo "ALL DONE" >> seg7.txt
