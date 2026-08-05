#!/bin/bash
# Per-delay endgame re-search: each k forces the stage01 clear >= baseline+k, producing a
# distinct final-kill choreography and hence a distinct frozen RNG -> stage02 board option.
cd /home/jaffar/jaffarPlus/examples/nes/bomberman
for k in 00 03 06 09 12 15 18 21 24 27 30; do
  CFG=stage01.eg${k}.jaffar
  LOG=stage01.eg${k}.log
  ln -sf "$PWD/$LOG" /tmp/jaffar.log; ln -sf "$PWD/$CFG" /tmp/claude.jaffar
  OMP_NUM_THREADS=64 nice /home/jaffar/jaffarPlus/build/jaffar "$CFG" > "$LOG" 2>&1
  W=$(sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$LOG" | grep -aoE "Step [0-9]+ - Exit Reason: Solution found" | grep -oE "[0-9]+" | tail -1)
  echo "k=$k done: win=${W:-none}" >> egReseed.results
done
echo "ALL DONE" >> egReseed.results
