#!/usr/bin/env bash
# Reference-reward pruning ("Prune Below Reference"): with a known-optimal reference solution,
# every produced state below the reference trace (beyond Prune Tolerance) must be dropped in the
# engine workers -- the "Dropped States (Below Reference)" counter must be non-zero -- while the
# on-pace lineage survives pruning and the search still finds an 8-move optimal solution. Also
# exercises the prune-without-cancel form ("Enabled": false + "Prune Below Reference": true).
set -uo pipefail

JAFFAR="${1:?usage: checkReferencePrune.sh <jaffar binary>}"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-1}"
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-100}"

REF_SOL=/tmp/jaffar.gw.prune.ref.sol
SOL=/tmp/jaffar.gw.prune.best.sol

# Known-optimal 8-move reference for the 5x5 grid: (0,0) -> (4,4)
rm -f "$REF_SOL" "$SOL"
printf '|R|\n|R|\n|R|\n|R|\n|D|\n|D|\n|D|\n|D|\n' > "$REF_SOL"

out="$("$JAFFAR" gridWalker.prune.jaffar 2>&1)"

[[ "$out" == *"Reference pruning armed"* ]] || { printf '%s\n' "$out" | tail -n 20; echo "FAIL: pruning banner missing"; exit 1; }
[[ "$out" == *"Solution found"* ]] || { printf '%s\n' "$out" | tail -n 20; echo "FAIL: expected a solution under pruning"; exit 1; }
[[ "$out" == *"Dropped States (Below Reference)"* ]] || { echo "FAIL: below-reference drop counter line missing"; exit 1; }

# The counter must have actually fired (off-pace siblings exist from step 1: any move away from
# the goal is immediately below the reference trace).
dropped="$(printf '%s\n' "$out" | grep -oE 'Dropped States \(Below Reference\):\s+[0-9]+' | grep -oE '[0-9]+$' | sort -g | tail -1)"
if [[ -z "$dropped" || "$dropped" -eq 0 ]]; then
  echo "FAIL: expected below-reference drops > 0, got '${dropped:-none}'"
  exit 1
fi

len="$(tr '\0' '\n' < "$SOL" | grep -c .)"
if [[ "$len" -ne 8 ]]; then
  echo "solution: $(tr '\0' ' ' < "$SOL")"
  echo "FAIL: solution length $len != 8 under pruning"
  exit 1
fi
echo "PASS: pruning dropped $dropped off-pace states and still found the 8-move optimum"
