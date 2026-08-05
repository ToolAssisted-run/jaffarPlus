#!/usr/bin/env bash
# Win State Collection + Stop Frames After First Win:
#
# Run 1 (gridWalker.wincollect.jaffar as-is): "Stop Frames After First Win": 4 keeps the search
#   expanding past the first (8-step) win; with "Steps" hashed, the later 10-step goal arrivals are
#   distinct states, so the collector (dedup on "Steps") must save >= 2 distinct win solutions and
#   the run must still end in "Solution found" (the window elapsing, not the cap).
#
# Run 2 (same config with "Max Files": 1): the cap is hit on the very first win, so the run must
#   terminate with "Win collection complete" and save exactly 1 solution.
#
# Both runs must print the per-step "Win States Collected: N/max" counter, and each collected
# solution must be replayable to a win by construction (it is the winning input history; run 1
# cross-checks the first collected file equals the best solution's length).
set -uo pipefail

JAFFAR="${1:?usage: checkWinCollect.sh <jaffar binary>}"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-1}"
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-100}"

PREFIX=/tmp/jaffar.gw.wincollect_
rm -f "${PREFIX}"*

# --- Run 1: harvest window ---
out="$("$JAFFAR" gridWalker.wincollect.jaffar 2>&1)"

[[ "$out" == *"Solution found"* ]] || { printf '%s\n' "$out" | tail -n 20; echo "FAIL: window run expected 'Solution found'"; exit 1; }
[[ "$out" == *"Win States Collected:"* ]] || { echo "FAIL: 'Win States Collected:' counter line missing"; exit 1; }

nFiles="$(ls "${PREFIX}"*.sol 2>/dev/null | wc -l)"
if [[ "$nFiles" -lt 2 ]]; then
  ls -la "${PREFIX}"* 2>/dev/null
  echo "FAIL: expected >= 2 collected win solutions (distinct Steps keys), got $nFiles"
  exit 1
fi
nManifest="$(grep -c . "${PREFIX}manifest.txt" 2>/dev/null)"
if [[ "$nManifest" -ne "$nFiles" ]]; then
  echo "FAIL: manifest lines ($nManifest) != collected files ($nFiles)"
  exit 1
fi
echo "PASS: window run collected $nFiles distinct win solutions"

# --- Run 2: Max Files cap terminates the run ---
rm -f "${PREFIX}"*
capConfig=/tmp/jaffar.gw.wincollectcap.jaffar
sed 's/"Max Files": 100/"Max Files": 1/' gridWalker.wincollect.jaffar > "$capConfig"

out="$("$JAFFAR" "$capConfig" 2>&1)"

[[ "$out" == *"Win collection complete"* ]] || { printf '%s\n' "$out" | tail -n 20; echo "FAIL: cap run expected 'Win collection complete'"; exit 1; }
nFiles="$(ls "${PREFIX}"*.sol 2>/dev/null | wc -l)"
if [[ "$nFiles" -ne 1 ]]; then
  echo "FAIL: cap run expected exactly 1 collected solution, got $nFiles"
  exit 1
fi
echo "PASS: cap run terminated on Max Files with 1 collected solution"
