#!/usr/bin/env bash
# Tests the "Candidate Input Sets" feature. The puzzle's allowed input set is right-only (|R|),
# which cannot reach the goal at (4,4); the down move (|D|) is supplied only as a *candidate*
# input. With candidate testing enabled the engine probes |D| per cell (keyed by GridWalker's
# getStateInputHash), its resulting states join the search, and the maze is solved optimally.
# With candidate testing disabled the same puzzle is unsolvable.
set -uo pipefail

JAFFAR="${1:?usage: checkCandidate.sh <jaffar binary>}"

# Private output files (concurrent runs of this test must not share them)
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/on" "$TMP/off"
CONFIG_ON="$(python3 privateConfig.py gridWalker.candidate.jaffar "$TMP/on")"
CONFIG_OFF="$(python3 privateConfig.py gridWalker.candidate_off.jaffar "$TMP/off")"
SOL_ON="$TMP/on/jaffar.gw.candidate.best.sol"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-1}"
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-100}"

solLen() { tr '\0' '\n' < "$1" | grep -c .; }
fail()   { echo "FAIL: $1"; exit 1; }

# Candidates ON: must solve, and optimally (8 moves: the union of allowed |R| and candidate |D|).
# (String matching is done with bash builtins rather than `... | grep -q`: the latter, under
# `set -o pipefail`, races -- grep -q closes the pipe on first match and the upstream writer gets
# SIGPIPE, which pipefail then reports as a spurious failure on large output.)
rm -f "$SOL_ON"
out="$("$JAFFAR" "$CONFIG_ON" 2>&1)"
[[ "$out" == *"Solution found"* ]] || { printf '%s\n' "$out" | tail -n 20; fail "candidates ON: no solution -- candidate inputs are not being probed/applied"; }
[[ "$(solLen "$SOL_ON")" -eq 8 ]] || fail "candidates ON: solution length $(solLen "$SOL_ON") != optimal 8"

# The end-of-run "Candidate Inputs" report must list the down move as a detected candidate
# (entry line format: "[J+]    +   <idx> |D|").
[[ "$out" =~ \+[[:space:]]+[0-9]+[[:space:]]\|D\| ]] || fail "candidates ON: |D| was not reported as a detected candidate input"

# Candidates OFF: the identical puzzle with candidate testing disabled is right-only and unsolvable.
outOff="$("$JAFFAR" "$CONFIG_OFF" 2>&1)"
if [[ "$outOff" == *"Solution found"* ]]; then
  printf '%s\n' "$outOff" | tail -n 20
  fail "candidates OFF: unexpectedly solved (right-only must not reach the goal)"
fi

echo "PASS: candidate |D| probed per-cell and reported, enables solving when ON, ignored when OFF"
exit 0
