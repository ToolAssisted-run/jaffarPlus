#!/usr/bin/env bash
# The full-database and multi-thread paths, which the other tests (one thread, roomy databases) never
# reach: slot stealing, replacing a worker's worst new state, dropping for lack of storage, the
# checkpoint cohort guard, the Trie input history, periodic saves and an explicit batch size.
#   1. gridWalker 250x250 with no hash DB and a 1 MB state DB at 8 threads, Raw and Trie: the database
#      fills, states are replaced, and the run reaches Max Steps with a best solution that replays to
#      exactly its length with every input allowed (checked with the player). Intermediate results are
#      saved every 0.1 s during the run.
#   2. The same with a Tolerance 0 checkpoint on a penalized milestone: the cohort guard must keep the
#      search alive to Max Steps (it used to run out of states when the milestone states were replaced).
#   3. An explicit Base State Batch Size solves the 5x5 grid optimally.
#   4. The Trie input history solves the 5x5 grid optimally and its solution replays to the win.
# Every variant is written to a private temporary directory.
set -uo pipefail

JAFFAR="${1:?usage: checkFullDatabase.sh <jaffar binary> <player binary>}"
PLAYER="${2:?usage: checkFullDatabase.sh <jaffar binary> <player binary>}"

unset JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB JAFFAR_DRIVER_OVERRIDE_DRIVER_MAX_STEP

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "FAIL: $1"; exit 1; }

python3 - "$TMP" <<'PY'
import json, sys
t = sys.argv[1]
def make(name, history, checkpoint=False, big=True, batch=None, steps=40):
    c = json.load(open('gridWalker.nohashdb.jaffar'))
    s = c['Driver Configuration']['Save Intermediate Results']
    s['Enabled'] = True
    s['Frequency (s)'] = 0.1
    s['Best Solution Path'] = t + '/' + name + '.best.sol'
    s['Worst Solution Path'] = t + '/' + name + '.worst.sol'
    s['Win Solution Path'] = t + '/' + name + '.win.sol'
    c['Driver Configuration']['Max Steps'] = steps if big else 64
    if big:
        c['Emulator Configuration']['Grid Width'] = 250
        c['Emulator Configuration']['Grid Height'] = 250
        c['Game Configuration']['Goal X'] = 249
        c['Game Configuration']['Goal Y'] = 249
        c['Engine Configuration']['State Database']['Max Size (Mb)'] = 1
    c['Runner Configuration']['Store Input History'] = {'Type': history, 'Max Size': 256}
    rules = c['Game Configuration']['Rules']
    for r in rules:
        for cond in r['Conditions']:
            if big and cond['Value'] == 4: cond['Value'] = 249
        for a in r['Actions']:
            if a['Type'] == 'Trigger Save Solution': a['Path'] = t + '/' + name + '.save.sol'
    if checkpoint:
        rules.append({'Label': 900, 'Conditions': [{'Property': 'Pos X', 'Op': '==', 'Value': 7}], 'Satisfies': [],
                      'Actions': [{'Type': 'Add Reward', 'Value': -1000.0}, {'Type': 'Trigger Checkpoint', 'Tolerance': 0}]})
    if batch is not None: c['Engine Configuration']['Base State Batch Size'] = batch
    json.dump(c, open(t + '/' + name + '.jaffar', 'w'), indent=1)
make('raw', 'Raw', steps=200)
make('trie', 'Trie')
make('checkpoint', 'Raw', checkpoint=True)
make('batch', 'Raw', big=False, batch=3)
make('trieOptimal', 'Trie', big=False)
PY

for v in raw trie; do
  steps=40
  [[ "$v" == raw ]] && steps=200
  out="$(OMP_NUM_THREADS=8 "$JAFFAR" "$TMP/$v.jaffar" 2>&1)"
  [[ "$out" == *"Step $steps - Exit Reason: Maximum step count reached."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "$v: expected to reach Max Steps $steps"; }
  # A state replaced by a better one means the database was full (its count stays just below 100%: the
  # workers' free-slot caches hold a few slots)
  replaced="$(printf '%s\n' "$out" | grep -oE 'Replaced By Better Ones: +[0-9]+' | tail -n 1 | grep -oE '[0-9]+$')"
  [[ -n "$replaced" && "$replaced" -gt 0 ]] || fail "$v: no next state was replaced by a better one (the database never filled)"
  # The final save writes one job- and step-suffixed copy; the 0.1 s periodic saves of the longer Raw run more
  if [[ "$v" == raw ]]; then
    copies="$(ls "$TMP/$v.best.sol".* 2>/dev/null | wc -l)"
    [[ "$copies" -ge 2 ]] || fail "$v: the periodic saver wrote no intermediate best solution ($copies copies)"
  fi
  replay="$(OMP_NUM_THREADS=1 "$PLAYER" "$TMP/$v.jaffar" "$TMP/$v.best.sol" --reproduce --disableRender --exitOnEnd --unattended --printFinalState 2>&1)" \
    || { printf '%s\n' "$replay" | tail -n 10; fail "$v: the best solution does not replay"; }
  [[ "$replay" == *"Final Step:                  $steps"* ]] || fail "$v: the best solution is not $steps inputs long"
  [[ "$replay" == *"Not Allowed Input Count:     0"* ]] || fail "$v: the best solution has inputs the search would not have tried"
done

out="$(OMP_NUM_THREADS=8 "$JAFFAR" "$TMP/checkpoint.jaffar" 2>&1)"
[[ "$out" == *"Step 40 - Exit Reason: Maximum step count reached."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "checkpoint: the search did not survive to Max Steps"; }

out="$(OMP_NUM_THREADS=4 "$JAFFAR" "$TMP/batch.jaffar" 2>&1)"
[[ "$out" == *"Step 8 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "batch: an explicit Base State Batch Size did not solve optimally"; }

# 4. Trie on the 5x5 grid: the optimal solution, written from the trie (it once came out as 8 x the first input)
out="$(OMP_NUM_THREADS=1 "$JAFFAR" "$TMP/trieOptimal.jaffar" 2>&1)"
[[ "$out" == *"Step 8 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "trieOptimal: Trie did not solve optimally"; }
[[ "$(tr '\0' '\n' < "$TMP/trieOptimal.best.sol" | grep -c .)" -eq 8 ]] || fail "trieOptimal: the Trie solution is not 8 inputs long"
replay="$(OMP_NUM_THREADS=1 "$PLAYER" "$TMP/trieOptimal.jaffar" "$TMP/trieOptimal.best.sol" --reproduce --disableRender --exitOnEnd --unattended --printFinalState 2>&1)"
[[ "$replay" == *"Final State Type:            Win"* ]] || fail "trieOptimal: the Trie solution does not replay to a win"

echo "PASS: full database (Raw and Trie), checkpoint guard, periodic saves, explicit batch size and Trie optimal"
exit 0
