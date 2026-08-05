#!/bin/bash
# Sequential budgeted jaffar over top-scored stage01 boards. Fairness: the running best is
# ABSOLUTE (arm + win); each run's post-arm step cap = bestAbsolute - its own arm + margin.
cd /home/jaffar/jaffarPlus/examples/nes/bomberman
BESTABS=1330
for P in 017 037; do
  CFG=stage01.p${P}.jaffar
  SEED=$(python3 -c "import json;print(json.load(open('$CFG'))['Emulator Configuration']['Initial Sequence File Path'])")
  ARM=$(( $(wc -l < "$SEED") + 1 ))
  CAP=$(( BESTABS == 99999 ? 5000 : BESTABS - ARM + 1 + 50 ))
  if [ "$CAP" -le 100 ]; then echo "P=$P skipped (cap $CAP too small)" >> kSweep.results; continue; fi
  python3 - "$CFG" "$CAP" <<'PY'
import json,sys
c=json.load(open(sys.argv[1])); c['Driver Configuration']['Max Steps']=int(sys.argv[2]); json.dump(c,open(sys.argv[1],'w'),indent=1)
PY
  LOG=stage01.p${P}.log
  ln -sf "$PWD/$LOG" /tmp/jaffar.log; ln -sf "$PWD/$LOG" /tmp/claude.log; ln -sf "$PWD/$CFG" /tmp/claude.jaffar; ln -sf /tmp/jaffar.p${P}.best.sol /tmp/claude.best.sol
  nice /home/jaffar/jaffarPlus/build/jaffar "$CFG" > "$LOG" 2>&1
  W=$(sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' "$LOG" | grep -aoE "Step [0-9]+ - Exit Reason: Solution found" | grep -oE "[0-9]+" | tail -1)
  if [ -n "$W" ]; then
    ABS=$(( W + ARM - 1 ))
    echo "P=$P done: win=$W arm=$ARM absolute=$ABS" >> kSweep.results
    if [ "$ABS" -lt "$BESTABS" ]; then BESTABS=$ABS; fi
  else
    echo "P=$P done: no win (cap $CAP)" >> kSweep.results
  fi
done
echo "ALL DONE bestAbsolute=$BESTABS" >> kSweep.results
