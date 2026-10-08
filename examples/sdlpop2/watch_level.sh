#!/bin/bash
# watch_level.sh TAG [N [D]]: waits for the search TAG (run_level.sh) to end, or to stall: over the last N steps
# (default 40) the best reward no higher than before them and the whole database within D of it (default 200). That
# is a search sitting in a pocket of its reward, which it does not leave (level 4 at 300 GB: 300 steps in room 26,
# every state within 77 of the best). A best reward standing still alone is no sign: level 4's holds for 225 steps
# of the slow fall, with the database 2700 wide. Prints the result line or the stall, and the best state.
T=$1; N=40; D=200; [ "$T" != "--test" ] && { N=${2:-40}; D=${3:-200}; }; L=~/pop2run/$T.log
stall() { grep -a "Current Reward (Best" $1 | awk -v n=$N -v d=$D '{b[NR]=$7+0; w[NR]=$9+0} END{if (NR>n) {for (i=NR-n+1;i<=NR;i++) if (b[i]>b[NR-n] || b[i]-w[i]>=d) exit; print "STALL: step " NR-1 ", best reward " b[NR] " (" b[NR-n] " " n " steps earlier), worst " w[NR]}}'; }
if [ "$T" = "--test" ]; then   # --test LOG: the first step a finished run's log would have been called a stall at
  grep -a "Current Reward (Best" $2 | awk -v n=$N -v d=$D '{b[NR]=$7+0; w[NR]=$9+0} END{for (k=n+1;k<=NR;k++) {s=1; for (i=k-n+1;i<=k;i++) if (b[i]>b[k-n] || b[i]-w[i]>=d) {s=0; break}; if (s) {print "stall at step " k-1; exit}}; print "no stall in " NR-1 " steps"}'; exit
fi
while :; do
  if grep -q " $T:" ~/pop2run/results.txt 2>/dev/null; then grep " $T:" ~/pop2run/results.txt | tail -1; break; fi
  S=$(stall $L 2>/dev/null); if [ -n "$S" ]; then echo "$S"; break; fi
  sleep 60
done
grep -a -E "Current Step #|Rule Status|\[Prince\]" $L | tail -3 | cut -c1-150
