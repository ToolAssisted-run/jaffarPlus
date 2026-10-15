#!/usr/bin/env bash
# Tests the hash-database deduplication logic on the GridWalker maze (optimal solution = 8 moves):
#   1. Disabled  : with the hash DB off there is no deduplication at all, yet BFS must still find
#                  the optimal solution -- i.e. dedup is a performance aid, not a correctness crutch.
#   2. Rolling   : with a deliberately tiny store size the sliding window fills and rolls (older
#                  hash stores are aged out and dropped), forcing states to be re-explored. The
#                  search must still find the optimal solution, and the run must show the window
#                  actually rolled (a hash store with id > 0) and that collisions were detected.
set -uo pipefail

JAFFAR="${1:?usage: checkHashDb.sh <jaffar binary>}"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-1}"
# Give the state DB ample room: with the hash DB disabled nothing is deduplicated, so every
# generated state is retained. Let the rolling config's own (tiny) hash store size take effect.
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=200
unset JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB

solLen() { tr '\0' '\n' < "$1" | grep -c .; }
fail()   { echo "FAIL: $1"; exit 1; }

# 1. Hash DB disabled: no deduplication, but the optimal solution must still be found.
SOL_OFF="/tmp/jaffar.gw.nohashdb.best.sol"
rm -f "$SOL_OFF"
outOff="$("$JAFFAR" gridWalker.nohashdb.jaffar 2>&1)"
[[ "$outOff" == *"Solution found"* ]] || { printf '%s\n' "$outOff" | tail -n 20; fail "hash DB disabled: no solution found"; }
[[ "$(solLen "$SOL_OFF")" -eq 8 ]] || fail "hash DB disabled: solution length $(solLen "$SOL_OFF") != optimal 8"

# 2. Tiny hash DB: the sliding window must roll, and the search must still solve optimally.
SOL_ROLL="/tmp/jaffar.gw.hashdb_rolling.best.sol"
rm -f "$SOL_ROLL"
outRoll="$("$JAFFAR" gridWalker.hashdb_rolling.jaffar 2>&1)"
[[ "$outRoll" == *"Solution found"* ]] || { printf '%s\n' "$outRoll" | tail -n 20; fail "tiny hash DB: no solution found"; }
[[ "$(solLen "$SOL_ROLL")" -eq 8 ]] || fail "tiny hash DB: solution length $(solLen "$SOL_ROLL") != optimal 8"

# Evidence the window actually rolled: store ids increment past 0 only when new stores are created.
reRolled='Id: [1-9]'
[[ "$outRoll" =~ $reRolled ]] || { printf '%s\n' "$outRoll" | grep -i "store" | tail -n 10; fail "tiny hash DB: the hash-store window did not roll (no store id > 0)"; }

# Evidence deduplication actually fired: at least one non-zero collision count is reported. With
# workers on one NUMA domain the single store reports 'Collision Count'; across several domains the
# two-tier store reports L1 'Local Hits' and L2 'Cross-Domain Hits' instead.
reCollision='Collision Count: [1-9]|Local Hits: [1-9]|Cross-Domain Hits: [1-9]'
[[ "$outRoll" =~ $reCollision ]] || fail "tiny hash DB: no collisions were detected (deduplication did not fire)"

echo "PASS: hash DB correct when disabled, and rolls its window + dedups while still solving optimally"
exit 0
