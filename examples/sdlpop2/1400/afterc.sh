#!/bin/bash
# afterc.sh: once segc.sh is through (r7C and its fight), the fight from the left entry (one search at a time)
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
while ! grep -q "SEGC DONE" fightk.txt 2>/dev/null; do sleep 20; done
./fightk.sh L 340 fb8 7
echo "ALL DONE" >> fightk.txt
