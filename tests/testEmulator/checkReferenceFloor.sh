#!/usr/bin/env bash
# Reference Reward Floor + Reference Pinning coverage test. Exercises:
#   1. The floor's "Solution File" form (per-step floor computed by replaying the reference internally).
#   2. Reference pinning: per-depth hash pins loaded from a player --dumpHashes file, pin hits reported.
#   3. The below-worst WARNING (warning-only; a hugely negative margin makes it fire deterministically).
#   4. The reference frame-count cap (End-On-First-Win=false search reaching the reference's length).
#   5. The player's headless screenshot loop on an emulator with no real renderer and no VRAM property
#      (the no-op saveScreenshot default and the tolerated missing-VRAM path).
set -uo pipefail

JAFFAR="${1:?usage: checkReferenceFloor.sh <jaffar binary> <player binary>}"
PLAYER="${2:?usage: checkReferenceFloor.sh <jaffar binary> <player binary>}"

CONFIG="gridWalker.floor.jaffar"
REF_SOL="/tmp/jaffar.gw.floor.ref.sol"
REF_HASHES="/tmp/jaffar.gw.floor.ref.hashes"
SHOT_DIR="/tmp/jaffar.gw.floor.shots"

export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-100}"

fail() { echo "FAIL: $1"; exit 1; }

# Known-optimal 8-move reference for the 5x5 grid: (0,0) -> (4,4)
rm -f "$REF_SOL" "$REF_HASHES"
rm -rf "$SHOT_DIR"; mkdir -p "$SHOT_DIR"
printf '|R|\n|R|\n|R|\n|R|\n|D|\n|D|\n|D|\n|D|\n' > "$REF_SOL"

# Per-depth reference hashes for pinning (exercises --dumpHashes)
out="$(OMP_NUM_THREADS=1 "$PLAYER" "$CONFIG" "$REF_SOL" --reproduce --disableRender --exitOnEnd --unattended --dumpHashes "$REF_HASHES" 2>&1)" \
  || { printf '%s\n' "$out" | tail -n 10; fail "player replay (dumpHashes) failed"; }
[[ -s "$REF_HASHES" ]] || fail "player did not produce the reference hash dump"

# Headless screenshot loop, separately (it is a one-shot terminal pass). TestEmulator has a no-op
# renderer and no VRAM property -- both must be tolerated without error.
out="$(OMP_NUM_THREADS=1 "$PLAYER" "$CONFIG" "$REF_SOL" --reproduce --disableRender --exitOnEnd --unattended --screenshotDir "$SHOT_DIR" --screenshotSteps 1,3 2>&1)" \
  || { printf '%s\n' "$out" | tail -n 10; fail "player headless screenshot pass failed"; }

# Floor + pinning search over the full step budget
out="$(OMP_NUM_THREADS=1 "$JAFFAR" "$CONFIG" 2>&1)"
[[ "$out" == *"Reference pinning enabled"* ]]        || { printf '%s\n' "$out" | tail -n 20; fail "pinning banner missing"; }
[[ "$out" == *"Reference Reward (Ref / Best-Ref)"* ]] || fail "floor telemetry missing"
[[ "$out" == *"Reference Pin Hits"* ]]               || fail "pin statistics missing"
[[ "$out" == *"WARNING: reference"* ]]               || fail "below-worst warning did not fire (negative margin should force it)"
[[ "$out" == *"Reached reference frame count"* || "$out" == *"Solution found"* ]] \
  || { printf '%s\n' "$out" | tail -n 20; fail "run did not end via win or reference frame cap"; }

# The pinned reference lineage must have been tracked to depth >= 1
hits="$(printf '%s\n' "$out" | grep -oE 'Reference Pin Hits \(cumulative\): +[0-9]+' | grep -oE '[0-9]+$' | tail -n 1)"
[[ -n "$hits" && "$hits" -ge 1 ]] || fail "no reference pin hits recorded"

echo "PASS"
