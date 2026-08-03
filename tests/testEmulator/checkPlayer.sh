#!/usr/bin/env bash
# Thorough exercise of the jaffar-player reproduction tool in unattended/headless mode. The player is
# interactive, but its batch flags (--reproduce --disableRender --unattended --exitOnEnd, plus the new
# --printFinalState oracle and --runCommand) make the main paths machine-checkable:
#   1. Headless reproduction reaches the expected final state (Win) and reports a stable final hash.
#   2. Determinism: the same config+solution reproduces to the identical final hash on a second run.
#   3. A partial (non-winning) solution reports a different, non-Win final state.
#   4. A solution containing a not-allowed input is detected and reported.
#   5. --runCommand q exits cleanly (return code 0).
#   6. --runCommand s writes a quicksave state file.
set -uo pipefail

JAFFAR="${1:?usage: checkPlayer.sh <jaffar binary> <player binary>}"
PLAYER="${2:?usage: checkPlayer.sh <jaffar binary> <player binary>}"

# Absolute config path so sub-tests can run from a scratch directory (the 's' quicksave command writes
# to the current working directory). A player-private config + solution path avoids racing the other
# tests that run `jaffar gridWalker.jaffar` concurrently (meson runs tests in parallel).
CONFIG="$PWD/gridWalker.player.jaffar"
SOL="/tmp/jaffar.gw.player.best.sol"
PARTIAL="/tmp/jaffar.gw.player.partial.sol"
NOTALLOWED="/tmp/jaffar.gw.player.notallowed.sol"

export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-100}"
export OMP_NUM_THREADS=1

# This test inspects the player's exit codes (clean quit, quicksave). The tools intentionally leave
# their top-level objects allocated at exit, so under an ASAN build LeakSanitizer would abort with a
# non-zero code and confound those checks. Disable leak detection (the CI sanitizers job already does
# this globally); harmless for the non-sanitizer build, which ignores ASAN_OPTIONS.
export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

fail() { echo "FAIL: $1"; exit 1; }

# Common batch flags for headless reproduction.
PLAY_FLAGS=(--reproduce --disableRender --exitOnEnd --unattended --printFinalState)

# Extracts the "Final State Hash" / "Final State Type" field from a captured player run.
finalHash() { printf '%s\n' "$1" | sed -n 's/.*Final State Hash: *//p' | tr -d '[:space:]'; }
finalType() { printf '%s\n' "$1" | sed -n 's/.*Final State Type: *//p' | tr -d '[:space:]'; }

# 0. Produce the canonical 8-move winning solution with the engine.
rm -f "$SOL"
"$JAFFAR" "$CONFIG" >/dev/null 2>&1
[[ -f "$SOL" ]] || fail "could not generate a solution to reproduce"

# 1. Headless reproduction must reach Win and report a final hash.
out1="$("$PLAYER" "$CONFIG" "$SOL" "${PLAY_FLAGS[@]}" 2>&1)"
type1="$(finalType "$out1")"
hash1="$(finalHash "$out1")"
[[ "$type1" == "Win" ]] || { printf '%s\n' "$out1" | tail -n 20; fail "reproduction final state type is '$type1', expected Win"; }
[[ -n "$hash1" ]] || fail "no final state hash printed"

# 2. Determinism: a second identical run must produce the identical final hash.
out2="$("$PLAYER" "$CONFIG" "$SOL" "${PLAY_FLAGS[@]}" 2>&1)"
hash2="$(finalHash "$out2")"
[[ "$hash1" == "$hash2" ]] || fail "non-deterministic reproduction: '$hash1' != '$hash2'"

# 3. A partial solution (first 4 of 8 moves) must NOT win and must land on a different state.
head -n 4 "$SOL" > "$PARTIAL"
outP="$("$PLAYER" "$CONFIG" "$PARTIAL" "${PLAY_FLAGS[@]}" 2>&1)"
typeP="$(finalType "$outP")"
hashP="$(finalHash "$outP")"
[[ "$typeP" != "Win" ]] || fail "partial solution unexpectedly reported a Win"
[[ "$hashP" != "$hash1" ]] || fail "partial solution produced the same final hash as the full solution"

# 4. A solution with a not-allowed input ("|.|" is parseable but not in the allowed input set) must be
#    detected and counted by the player. Prepending one such input to an all-allowed winning solution
#    must yield a non-zero not-allowed count.
naCount() { printf '%s\n' "$1" | sed -n 's/.*Not Allowed Input Count: *//p' | tr -d '[:space:]'; }
{ echo "|.|"; cat "$SOL"; } > "$NOTALLOWED"
outNA="$("$PLAYER" "$CONFIG" "$NOTALLOWED" "${PLAY_FLAGS[@]}" 2>&1)"
[[ "$(naCount "$outNA")" -ge 1 ]] || { printf '%s\n' "$outNA" | grep -i "not allowed" | head; fail "not-allowed input was not reported"; }

# 5. --runCommand q must exit cleanly (return code 0).
if ! "$PLAYER" "$CONFIG" "$SOL" --disableRender --unattended --runCommand q >/dev/null 2>&1; then
  fail "--runCommand q did not exit cleanly"
fi

# 6. --runCommand s must write a quicksave state file (written to the current directory).
QSDIR="$(mktemp -d)"
( cd "$QSDIR" && "$PLAYER" "$CONFIG" "$SOL" --disableRender --unattended --runCommand s >/dev/null 2>&1 )
[[ -s "$QSDIR/quicksave.state" ]] || { rm -rf "$QSDIR"; fail "--runCommand s did not write a non-empty quicksave.state"; }
rm -rf "$QSDIR"

# 7. Render-mode reproduction (no --disableRender) with a dummy video driver. This exercises the
#    Playback path that caches per-step renderer state (storeRendererState=true), which the headless
#    --disableRender runs above skip. Must auto-play to the end and exit cleanly (return code 0).
if ! SDL_VIDEODRIVER=dummy "$PLAYER" "$CONFIG" "$SOL" --reproduce --unattended --exitOnEnd </dev/null >/dev/null 2>&1; then
  fail "render-mode reproduction (renderer-state caching) did not exit cleanly"
fi

# 8. Compact engine log verbosity: a run with "Log Verbosity": "Compact" must emit the compact
#    status block (a handful of grep-stable lines) instead of the full report.
outC="$("$JAFFAR" "$PWD/gridWalker.compact.jaffar" 2>&1)"
[[ "$outC" == *"New States Processed"* && "$outC" == *"Checkpoint (Level/Tolerance/Cutoff)"* ]] \
  || { printf '%s\n' "$outC" | tail -n 20; fail "compact log verbosity did not emit the compact status block"; }

# 9. Full-fidelity mid-run state save (--saveStateStep + --saveStateFile) must write a state file
#    that a fresh player can then load as its Initial State File Path.
STATE="$(mktemp -u).state"
"$PLAYER" "$CONFIG" "$SOL" --saveStateStep 4 --saveStateFile "$STATE" --disableRender --unattended >/dev/null 2>&1
[[ -s "$STATE" ]] || fail "--saveStateStep/--saveStateFile did not write a non-empty state file"
rm -f "$STATE"

echo "PASS: player reproduces to Win deterministically (hash $hash1); partial=$typeP; not-allowed reported; runCommand q/s OK; render-mode OK; compact-log OK; state-save OK"
exit 0
