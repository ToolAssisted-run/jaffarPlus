#!/bin/bash
# watch_level.sh TAG [N [D [JSTEP]]]: waits for the search TAG (run_level.sh) to end, or to stall: over the last N steps
# (default 40) the best reward no higher than before them and the whole database within D of it (default 200). That
# is a search sitting in a pocket of its reward, which it does not leave (level 4 at 300 GB: 300 steps in room 26,
# every state within 77 of the best). A best reward standing still alone is no sign: level 4's holds for 225 steps
# of the slow fall, with the database 2700 wide. Prints the result line or the stall, and the best state.
# It also ends on a jump: the best state's rules done going up by 4 or more in one step (after step JSTEP, the fourth
# argument, default 0). A step marks those before it done, so a late step's condition met early by another way puts
# all their rewards on that way (level 5 at 300 GB: the trap's lift under the shut hatch, 7 -> 30 rules at step 279,
# a ride to his death that flooded the database). To be looked at; if it is the route's, watch on from that step.
# And on the rules standing: the best state's rules done the same as RN steps earlier (the fifth argument, default
# 300; 0: never). Level 7 at 300 GB sat at 4 of 57 for 1200 steps: a fail rule (a small potion's tile, there for
# "never drunk") was met by a loose floor falling on the potion, 18 ticks after the only way on. A long step looks
# the same (level 4's slow fall: 225 steps): to be looked at, then watched on with a larger RN.
# And on a room entered late: with a reference ~/pop2run/TAG.ref (mkref.sh: "# DIR CHAIN LATE", then "ROOM TICK" lines,
# the rooms of the best solution known and the ticks it enters them on), the best solution so far is replayed and
# its rooms' ticks compared in order; one entered more than LATE ticks (default 30) after the reference's ends the
# watch. Level 8 at 300 GB: no alarm above fired, the rules went on, and the solution was 88 ticks behind the old
# one from room 12 on (the database had dropped the two jumps there for states waiting further left). To be looked
# at; a chain's other random numbers can cost that honestly: then watch on without the reference, or a larger LATE.
T=$1; N=40; D=200; JS=0; RN=300; [ "$T" != "--test" ] && { N=${2:-40}; D=${3:-200}; JS=${4:-0}; RN=${5:-300}; }; L=~/pop2run/$T.log
rules() { grep -a "Rule Status" $1 | awk -v n=$RN '{x=$NF; c[NR]=gsub(/1/,"1",x)} END{if (n>0 && NR>n && c[NR]==c[NR-n]) print "RULES: step " NR-1 ": " c[NR] " rules done, as " n " steps earlier"}'; }
jump() { grep -a "Rule Status" $1 | awk -v s=$JS '{x=$NF; c[NR]=gsub(/1/,"1",x)} END{for (i=2;i<=NR;i++) if (i-1>s && c[i]-c[i-1]>=4) {print "JUMP: step " i-1 ": " c[i-1] " -> " c[i] " rules done"; exit}}'; }
late() {
  local R=~/pop2run/$T.ref B=/tmp/jaffar.run.$T.best.sol h DIR CHAIN LD; [ -f $R ] && [ -s $B ] || return
  read h DIR CHAIN LD < $R; cp $B /dev/shm/watch_$T.sol
  (cd ~/jaffarPlus-run/examples/sdlpop2/$DIR && ~/pop2tools-m/trs prince2 7881336 0 0 $CHAIN /dev/shm/watch_$T.sol 2>/dev/null) | awk 'NR>1 && $2 != r { print $2, $1; r = $2 }' > /dev/shm/watch_$T.rooms
  awk -v ld=${LD:-30} 'NR==FNR { if ($1 != "#") { n++; rr[n]=$1; rt[n]=$2 }; next } { for (k=j+1; k<=n && k<=j+4; k++) if (rr[k]==$1) { j=k; if ($2-rt[k] > ld) { print "LATE: room " $1 " entered at tick " $2 ", the reference at " rt[k]; exit }; break } }' $R /dev/shm/watch_$T.rooms
}
stall() { grep -a "Current Reward (Best" $1 | awk -v n=$N -v d=$D '{b[NR]=$7+0; w[NR]=$9+0} END{if (NR>n) {for (i=NR-n+1;i<=NR;i++) if (b[i]>b[NR-n] || b[i]-w[i]>=d) exit; print "STALL: step " NR-1 ", best reward " b[NR] " (" b[NR-n] " " n " steps earlier), worst " w[NR]}}'; }
if [ "$T" = "--test" ]; then   # --test LOG: the first step a finished run's log would have been called a stall at
  grep -a "Current Reward (Best" $2 | awk -v n=$N -v d=$D '{b[NR]=$7+0; w[NR]=$9+0} END{for (k=n+1;k<=NR;k++) {s=1; for (i=k-n+1;i<=k;i++) if (b[i]>b[k-n] || b[i]-w[i]>=d) {s=0; break}; if (s) {print "stall at step " k-1; exit}}; print "no stall in " NR-1 " steps"}'; exit
fi
while :; do
  if grep -q " $T:" ~/pop2run/results.txt 2>/dev/null; then grep " $T:" ~/pop2run/results.txt | tail -1; break; fi
  S=$(stall $L 2>/dev/null); if [ -n "$S" ]; then echo "$S"; break; fi
  S=$(jump $L 2>/dev/null); if [ -n "$S" ]; then echo "$S"; break; fi
  S=$(rules $L 2>/dev/null); if [ -n "$S" ]; then echo "$S"; break; fi
  S=$(late 2>/dev/null); if [ -n "$S" ]; then echo "$S"; break; fi
  sleep 60
done
grep -a -E "Current Step #|Rule Status|\[Prince\]" $L | tail -3 | cut -c1-150
