#!/bin/bash
# probe.sh N "inputs...": the first N lines of fR.sol, then the given lines (a count prefix "12x|.|......|" repeats one); compact trace of the added part
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
N=$1; shift; BASE=${BASE:-fR.sol}; INIT=${INIT:-init_fR.sol}
T=/dev/shm/probe.$$.sol; head -$N $BASE > $T
for a in "$@"; do if [[ $a == *x* ]]; then c=${a%%x*}; l=${a#*x}; for ((i=0;i<c;i++)); do echo "$l" >> $T; done; else echo "$a" >> $T; fi; done
HP14=${HP14:-3} CHARS=1 MOBS=1 ${TRL:-$HOME/pop2tools/trl} prince2 7881336 11 5 $INIT $T 2>/dev/null | tail -n +${FROM:-$((N+1))} | perl -ne '
my ($t,$in)=/^\s*(\d+) (\S+)/; my ($r,$x,$y,$row,$f)=/room\s+(\d+) x\s*(-?\d+) y\s*(-?\d+) row (-?\d+) frame\s+(\d+)/; my ($seq,$hp)=/seq\s+(\d+) hp (\d+)/; my ($dir)=/dir (-?\d+)/; my ($dr)=/drawn (\d+)/; my ($nx)=/next (\d+)/;
my ($jx,$jrow,$jf,$jseq,$ja)=/\[id 6 x (-?\d+) row (-?\d+) f (\d+) seq (\d+) alive (-?\d+)/; my $m=""; while (/\[type (\d+) room (\d+) x (-?\d+) y (-?\d+) row (\d+) speed (-?\d+) w7 (-?\d+) wd (-?\d+)\]/g) { $m.=" mob[r$2 x$3 row$5 w7 $7 wd $8 hit ".($7+$3+113)."]"; }
printf "%4d %s k r%d x%4d y%4d row%2d f%3d s%3d d%2d hp%d | J x%4d row%2d f%3d s%3d a%d n%s%s\n",$t,$in,$r,$x,$y,$row,$f,$seq,$dir,$hp,$jx,$jrow,$jf,$jseq,$ja,$nx,$m;'
rm -f $T
