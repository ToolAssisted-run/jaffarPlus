#!/bin/bash
# fightk.sh ENTRY [MAXSTEPS [ROUTE UPTO]]: the Jaffar fight from room 7's entry ENTRY (R / L / C: after r7ENTRY.sol) to the
# fireball's hit, the random seed not hashed (nothing in 7 | 8 draws on it): ROUTE+ENTRY.sol, a line in fightk.txt
# (routes: fight, the plan left to the search, 5 steps; fb / fb8, plan B, 7)
cd ~/jaffarPlus-pop2/examples/sdlpop2/1400 || exit 1
s=$1; M=${2:-300}; r=${3:-fight}; u=${4:-5}; t=$r$s
cat init_e1.sol r7$s.sol > init_f$s.sol
python3 gen.py $r --hp 3 --norng --upto $u --anyframe --db 200000 --maxsteps $M --init init_f$s.sol --out $t.jaffar --tag $t
ln -sfn ~/pop2l3/l14$t.log /tmp/claude.log; ln -sfn /tmp/jaffar.pop2l14.$t.best.sol /tmp/claude.best.sol; ln -sfn $PWD/$t.jaffar /tmp/claude.jaffar
env OMP_NUM_THREADS=256 ../../../build/jaffar $t.jaffar > ~/pop2l3/l14$t.log 2>&1
cp /tmp/jaffar.pop2l14.$t.best.sol $t.sol
echo "$r entry $s: $(grep -a 'Exit Reason' ~/pop2l3/l14$t.log | tail -1 | sed 's/.*Exit Reason: //') after $(wc -l < $t.sol) ticks | $(tail -80 ~/pop2l3/l14$t.log | grep -a Prince | tail -1 | sed 's/.*\[Prince\] *//' | cut -c1-70)" >> fightk.txt
