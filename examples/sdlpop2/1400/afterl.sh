#!/bin/bash
# afterl.sh: after the three entries fights: the right entry again with the top row free (route fb: room 7 col 9 too)
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
while ! grep -q "ALL DONE" fightk.txt 2>/dev/null; do sleep 20; done
./fightk.sh R 300 fb 7
echo "QUEUE DONE" >> fightk.txt
