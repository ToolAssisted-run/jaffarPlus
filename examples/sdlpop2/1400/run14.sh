#!/bin/bash
# run14.sh SEGMENT [DB_MB]: the final run's level 14, one segment's search on jaffanator2 (ROUTING.md "The final
# run"). Level 14 is no single route: the scene, room 3's potions, the ending in three. Each segment starts where the
# one before ended: the chain is 1400/l0102..13cut.sol (post_level.sh 13 TAG cut) and the segments found so far.
#   scene  room 1 to the scene with Jaffar at the window (the level loaded again: room 4)          l14scene.sol
#   farm   room 3's six big potions (5 -> 11) and back onto room 4's middle row, standing, 11/11  l14farm.sol
#   e1     the eight turns: the spirit out of the body                                            l14e1.sol
#   r7     the spirit through room 6's Jaffars up into room 7 by the right platform               l14r7.sol
#   fight  Jaffar's round and the fireball's hit, from 10 ticks before r7's end (fightk.sh: the   l14fight.sol
#          game keeps the keys pressed in a climb, and r7 ends on whatever it pressed there)
# The level as the run plays it: l14scene + l14farm + l14e1 + l14r7 less its last 10 lines + l14fight, and the
# ticks to the win after the hit. One search at a time: it refuses while a jaffar runs. env WAIT: seconds before
# the first steps are printed (45); the watcher is watch_level.sh l14SEGMENT.
set -e
SEG=$1; DB=${2:-300000}
S=$HOME/jaffarPlus-run/examples/sdlpop2; cd $S/1400
CH=l01020304050607080910111213cut.sol
[ -f $CH ] || { echo "no chain $CH"; exit 1; }
if pidof jaffar > /dev/null; then echo "a jaffar is running: $(pidof jaffar)"; exit 1; fi
# (a search just stopped frees its database over a minute: the next one would die at once for want of memory)
F=$(free -m | awk '/^Mem:/ {print $7}'); [ $F -ge $((DB + 60000)) ] || { echo "only $F MB of memory available for a $DB MB database"; exit 1; }
need() { for f in "$@"; do [ -s $f ] || { echo "missing $f"; exit 1; }; done; }
I=init_run_$SEG.sol
case $SEG in
  scene) cp $CH $I; A="scene --upto 3 --anyframe --maxsteps 200";;
  farm)  need l14scene.sol; cat $CH l14scene.sol > $I; A="farm --to 11 --upto 22 --maxsteps 1400";;
  e1)    need l14scene.sol l14farm.sol; cat $CH l14scene.sol l14farm.sol > $I; A="end --upto 9 --anyframe --maxsteps 200";;
  r7)    need l14scene.sol l14farm.sol l14e1.sol; cat $CH l14scene.sol l14farm.sol l14e1.sol > $I; A="r7R --upto 8 --maxsteps 420";;
  fight) need l14scene.sol l14farm.sol l14e1.sol l14r7.sol
         cat $CH l14scene.sol l14farm.sol l14e1.sol <(head -n -10 l14r7.sol) > $I; A="fb8 --norng --upto 7 --anyframe --maxsteps 320";;
  *) echo "unknown segment $SEG"; exit 1;;
esac
python3 gen.py $A --run --init $I --db $DB --out script-$SEG-run.jaffar --tag l14$SEG
echo "init $I: $(grep -c '^|' $I) lines; script script-$SEG-run.jaffar: $(grep -c '"Label"' script-$SEG-run.jaffar) rules"
grep -o -E '"Music On At Start": *[a-z]+|"Max Steps": *[0-9]+|"Hash Random Seed": *[a-z]+|"Max HP At Level"[^]]*\]' script-$SEG-run.jaffar | tr "\n" ";"; echo
nohup ../run_level.sh 1400 script-$SEG-run.jaffar l14$SEG $DB > /dev/null 2>&1 &
sleep ${WAIT:-45}
L=$HOME/pop2run/l14$SEG.log
ls -la /tmp/claude.log /tmp/claude.best.sol /tmp/claude.jaffar | sed -E "s/.* (\/tmp\/claude)/\1/"
grep -a "Current Step #" $L | tail -1
grep -a -E "\[Prince\]|Rule Status|Level:" $L | tail -3
