#!/bin/bash
# run14.sh SEGMENT [DB_MB]: the final run's level 14, one segment's search on jaffanator2 (ROUTING.md "The final
# run"). Level 14 is no single route: the scene, room 3's potions, the ending in three. Each segment starts where the
# one before ended: the chain is 1400/l0102..13cut.sol (post_level.sh 13 TAG cut) and the segments found so far.
#   scene  room 1 to the scene with Jaffar at the window (the level loaded again: room 4)          l14scene.sol
#   farm   room 3's six big potions (5 -> 11) and back onto room 4's middle row, standing, 11/11  l14farm.sol
#          (as searched: l14farmA.sol, the first search's way to the fifth potion's tick, 10/10; farmB from there
#          to the end, the last potion's step paid as its pull up costs; l14farm.sol is the two)
#   e1     the eight turns: the spirit out of the body                                            l14e1.sol
#   drop   (a variation, Sergio) the eight turns on room 4's lower tile, from the farm's line FARMCUT (727: room 4
#          entered, in the climb): the body left over the tile's edge falls out of the room            l14drop.sol
#   r7     the spirit through room 6's Jaffars up into room 7 by the right platform               l14r7.sol
#   fight  Jaffar's round and the fireball's hit, from 10 ticks before r7's end (fightk.sh: the   l14fight.sol
#          game keeps the keys pressed in a climb, and r7 ends on whatever it pressed there)
# The level as the run plays it: l14scene + l14farm + l14e1 + l14r7 less its last 10 lines + l14fight, and the
# ticks to the win after the hit. One search at a time: it refuses while a jaffar runs. env WAIT: seconds before
# the first steps are printed (45); the watcher is watch_level.sh l14SEGMENT.
# Version 2 (Sergio's variation: the body dropped out of room 4) shares the scene and the farm's first FARMCUT (727)
# lines, then:
#   dc     the eight turns on room 4's lower tile, the body and the spirit down into room 3, the     l14dc.sol
#          spirit back up onto room 4's middle row, standing
#   r7d    as r7, from there                                                                        l14r7d.sol
#   fightd as fight, from 10 ticks before r7d's end                                                 l14fightd.sol
#   cut with env V2=1 takes those
# Version 3 (Sergio, 2026-10-11): the drop keeps the spirit the 3 hit points the body's lying down costs (6 of 6 for
# 3 of 3: control_dead_0307a2; checked in DOS), so 9 at the turns will do for 11 and two potions fewer:
#   x9     from the fourth potion's drink (the farm's line P4, 447: 9/9 on room 3's middle row) up the      l14x9.sol
#          shaft to room 4's entry; then l14farm9.sol = the farm's first P4 lines + l14x9.sol
#   dc, r7d, fightd and cut with env FARM=l14farm9.sol (FARMCUT its length) and SUF=9 (their files l14dc9.sol,
#   l14r7d9.sol, l14fightd9.sol; cut writes l14cut9.sol and no chain: the run stays the one it is until taken);
#   fightd with env SH=4 too (the spirit's hit points after the turns: the fail rules of the cast)
#   cut    the level from the segments: l14cut.sol (env TAIL: the empty lines after the fight's last, the ticks from
#          the fireball's hit to the final scene; 7) and the whole run's chain, ../1500/l0102..14cut.sol
set -e
SEG=$1; DB=${2:-300000}; FARM=${FARM:-l14farm.sol}; SUF=$SUF
S=$HOME/jaffarPlus-run/examples/sdlpop2; cd $S/1400
CH=l01020304050607080910111213cut.sol
[ -f $CH ] || { echo "no chain $CH"; exit 1; }
if [ "$SEG" = cut ]; then
  if [ -n "$V2" ]; then
    for f in l14scene.sol $FARM l14dc$SUF.sol l14r7d$SUF.sol l14fightd$SUF.sol; do [ -s $f ] || { echo "missing $f"; exit 1; }; done
    { cat l14scene.sol; head -n ${FARMCUT:-727} $FARM; cat l14dc$SUF.sol; head -n -10 l14r7d$SUF.sol; cat l14fightd$SUF.sol; for i in $(seq ${TAIL:-7}); do echo "|.|......|"; done; } > l14cut$SUF.sol
    if [ -n "$SUF" ]; then echo "l14cut$SUF.sol: $(grep -c '^|' l14cut$SUF.sol) lines (scene $(grep -c '^|' l14scene.sol), potions ${FARMCUT:-727}, drop and back $(grep -c '^|' l14dc$SUF.sol), to room 7 $(($(grep -c '^|' l14r7d$SUF.sol) - 10)), fight $(grep -c '^|' l14fightd$SUF.sol), ${TAIL:-7} to the final scene)"; exit 0; fi
    mkdir -p ../1500; [ -e ../1500/prince2 ] || ln -s $(readlink -f prince2) ../1500/prince2
    cat $CH l14cut.sol > ../1500/${CH%13cut.sol}1314cut.sol
    echo "l14cut.sol (version 2): $(grep -c '^|' l14cut.sol) lines (scene $(grep -c '^|' l14scene.sol), potions ${FARMCUT:-727}, drop and back $(grep -c '^|' l14dc.sol), to room 7 $(($(grep -c '^|' l14r7d.sol) - 10)), fight $(grep -c '^|' l14fightd.sol), ${TAIL:-7} to the final scene); the run: $(grep -c '^|' ../1500/${CH%13cut.sol}1314cut.sol)"
    exit 0
  fi
  for f in l14scene.sol l14farm.sol l14e1.sol l14r7.sol l14fight.sol; do [ -s $f ] || { echo "missing $f"; exit 1; }; done
  { cat l14scene.sol l14farm.sol l14e1.sol; head -n -10 l14r7.sol; cat l14fight.sol; for i in $(seq ${TAIL:-7}); do echo "|.|......|"; done; } > l14cut.sol
  mkdir -p ../1500; [ -e ../1500/prince2 ] || ln -s $(readlink -f prince2) ../1500/prince2
  cat $CH l14cut.sol > ../1500/${CH%13cut.sol}1314cut.sol
  echo "l14cut.sol: $(grep -c '^|' l14cut.sol) lines (scene $(grep -c '^|' l14scene.sol), potions $(grep -c '^|' l14farm.sol), turns $(grep -c '^|' l14e1.sol), to room 7 $(($(grep -c '^|' l14r7.sol) - 10)), fight $(grep -c '^|' l14fight.sol), ${TAIL:-7} to the final scene); the run: $(grep -c '^|' ../1500/${CH%13cut.sol}1314cut.sol)"
  exit 0
fi
if pidof jaffar > /dev/null; then echo "a jaffar is running: $(pidof jaffar)"; exit 1; fi
# (a search just stopped frees its database over a minute: the next one would die at once for want of memory)
F=$(free -m | awk '/^Mem:/ {print $7}'); [ $F -ge $((DB + 60000)) ] || { echo "only $F MB of memory available for a $DB MB database"; exit 1; }
need() { for f in "$@"; do [ -s $f ] || { echo "missing $f"; exit 1; }; done; }
I=init_run_$SEG.sol
case $SEG in
  scene) cp $CH $I; A="scene --upto 3 --anyframe --maxsteps 200";;
  farm)  need l14scene.sol; cat $CH l14scene.sol > $I; A="farm --to 11 --upto 22 --maxsteps 1400";;
  farmB) need l14scene.sol l14farmA.sol; cat $CH l14scene.sol l14farmA.sol > $I; A="farm --hp 10 --in3 --to 11 --upto 6 --maxsteps 500";;
  drop|drop2) need l14scene.sol l14farm.sol; cat $CH l14scene.sol <(head -n ${FARMCUT:-727} l14farm.sol) > $I; A="$SEG --upto 10 --anyframe --maxsteps 250";;
  x9)    need l14scene.sol l14farm.sol; cat $CH l14scene.sol <(head -n ${P4:-447} l14farm.sol) > $I; A="farm --hp 9 --in3 --to 9 --upto 2 --anyframe --maxsteps 300";;
  dc)    need l14scene.sol $FARM; cat $CH l14scene.sol <(head -n ${FARMCUT:-727} $FARM) > $I; A="dc ${SH:+--sh $SH} --upto $([ -n "$SH" ] && echo 12 || echo 13) --maxsteps 300";;
  r7d)   need l14scene.sol $FARM l14dc$SUF.sol; cat $CH l14scene.sol <(head -n ${FARMCUT:-727} $FARM) l14dc$SUF.sol > $I; A="r7R --upto 8 --maxsteps 420";;
  fightd) need l14scene.sol $FARM l14dc$SUF.sol l14r7d$SUF.sol
         cat $CH l14scene.sol <(head -n ${FARMCUT:-727} $FARM) l14dc$SUF.sol <(head -n -10 l14r7d$SUF.sol) > $I; A="fb8 --norng ${SH:+--sh $SH} --upto 7 --anyframe --maxsteps 320";;
  fightp) need l14scene.sol $FARM l14dc$SUF.sol l14r7d$SUF.sol   # (Sergio: a cast at once, the shadow draws Jaffar, room 7's potion, the second cast)
         cat $CH l14scene.sol <(head -n ${FARMCUT:-727} $FARM) l14dc$SUF.sol <(head -n -10 l14r7d$SUF.sol) > $I; A="fbp ${SH:+--sh $SH} --upto 6 --anyframe --maxsteps 320";;
  e1)    need l14scene.sol l14farm.sol; cat $CH l14scene.sol l14farm.sol > $I; A="end --upto 9 --anyframe --maxsteps 200";;
  r7)    need l14scene.sol l14farm.sol l14e1.sol; cat $CH l14scene.sol l14farm.sol l14e1.sol > $I; A="r7R --upto 8 --maxsteps 420";;
  fight) need l14scene.sol l14farm.sol l14e1.sol l14r7.sol
         cat $CH l14scene.sol l14farm.sol l14e1.sol <(head -n -10 l14r7.sol) > $I; A="fb8 --norng --upto 7 --anyframe --maxsteps 320";;
  *) echo "unknown segment $SEG"; exit 1;;
esac
TAG=l14$SEG$SUF
python3 gen.py $A --run --init $I --db $DB --out script-$SEG-run.jaffar --tag $TAG
echo "init $I: $(grep -c '^|' $I) lines; script script-$SEG-run.jaffar: $(grep -c '"Label"' script-$SEG-run.jaffar) rules"
grep -o -E '"Music On At Start": *[a-z]+|"Max Steps": *[0-9]+|"Hash Random Seed": *[a-z]+|"Max HP At Level"[^]]*\]' script-$SEG-run.jaffar | tr "\n" ";"; echo
[ -n "$GENONLY" ] && exit 0   # (the script only: tools/rewardcheck.py on it before a search)
nohup ../run_level.sh 1400 script-$SEG-run.jaffar $TAG $DB > /dev/null 2>&1 &
sleep ${WAIT:-45}
L=$HOME/pop2run/$TAG.log
ls -la /tmp/claude.log /tmp/claude.best.sol /tmp/claude.jaffar | sed -E "s/.* (\/tmp\/claude)/\1/"
grep -a "Current Step #" $L | tail -1
grep -a -E "\[Prince\]|Rule Status|Level:" $L | tail -3
