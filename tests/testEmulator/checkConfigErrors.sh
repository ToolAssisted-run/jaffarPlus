#!/usr/bin/env bash
# Configuration checks made at load time instead of failing (or silently misbehaving) during the
# search. Each variant of gridWalker.jaffar is written to a private temporary directory.
#   Errors (exit code 8 and a message, no crash): a zero modulo divisor, an immediate that does not
#   fit the property, a bit test on a bool property or past its width, Hash Lookahead without an
#   input to advance with, an unknown Win State Collection property.
#   Accepted (must still find the optimal 8-step solution): cyclic and self-referencing 'Satisfies'
#   (they recursed without bound before), fractional bounds on an integer property (compared by value),
#   and Hash Lookahead with an explicit no-op 'Hash Lookahead Input'.
set -uo pipefail

JAFFAR="${1:?usage: checkConfigErrors.sh <jaffar binary>}"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-2}"
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-10}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# variant <name> <python statements editing the parsed config 'c'>
variant() {
  python3 - "$TMP/$1.jaffar" "$2" "$TMP" <<'PY'
import json, sys
c = json.load(open('gridWalker.jaffar'))
tmp = sys.argv[3]
s = c['Driver Configuration']['Save Intermediate Results']
s['Best Solution Path'] = tmp + '/best.sol'
s['Worst Solution Path'] = tmp + '/worst.sol'
for r in c['Game Configuration']['Rules']:
    for a in r['Actions']:
        if a['Type'] == 'Trigger Save Solution': a['Path'] = tmp + '/win.sol'
rules = {r['Label']: r for r in c['Game Configuration']['Rules']}
exec(sys.argv[2])
json.dump(c, open(sys.argv[1], 'w'), indent=1)
PY
}

expectError() { # <name> <message>
  local out rc
  out="$("$JAFFAR" "$TMP/$1.jaffar" 2>&1)"
  rc=$?
  if [[ $rc -ne 8 || "$out" != *"$2"* ]]; then
    printf '%s\n' "$out" | tail -n 15
    echo "FAIL: $1: expected exit code 8 with '$2', got $rc"
    exit 1
  fi
}

expectSolution() { # <name>
  local out
  rm -f "$TMP/best.sol"
  out="$("$JAFFAR" "$TMP/$1.jaffar" 2>&1)"
  if [[ "$out" != *"Step 8 - Exit Reason: Solution found."* ]]; then
    printf '%s\n' "$out" | tail -n 15
    echo "FAIL: $1: expected the optimal solution at step 8"
    exit 1
  fi
  [[ "$(tr '\0' '\n' < "$TMP/best.sol" | grep -c .)" -eq 8 ]] || { echo "FAIL: $1: best solution is not 8 inputs long"; exit 1; }
}

variant modzero    "rules[100]['Conditions'].append({'Property': 'Pos X', 'Op': '%0', 'Value': 0})"
variant range      "rules[100]['Conditions'][0]['Value'] = 257"
variant bitbool    "rules[100]['Conditions'].append({'Property': 'Goal Reached', 'Op': 'BitTrue', 'Value': 0})"
variant bitwidth   "rules[100]['Conditions'].append({'Property': 'Pos X', 'Op': 'BitTrue', 'Value': 8})"
variant lookahead  "c['Engine Configuration']['Hash Lookahead'] = 1"
variant wincollect "c['Driver Configuration']['Win State Collection'] = {'Enabled': True, 'Dedup Properties': ['Stepz'], 'Path Prefix': tmp + '/wc_'}"

expectError modzero    "modulo divisor"
expectError range      "out of range"
expectError bitbool    "needs an integer property"
expectError bitwidth   "Bit index"
expectError lookahead  "Hash Lookahead Input"
expectError wincollect "dedup property 'Stepz' not registered"

variant cycle      "rules[100]['Satisfies'] = [100]; c['Game Configuration']['Rules'] += [{'Label': 400, 'Conditions': [{'Property': 'Pos X', 'Op': '==', 'Value': 1}], 'Satisfies': [500], 'Actions': []}, {'Label': 500, 'Conditions': [{'Property': 'Pos X', 'Op': '==', 'Value': 3}], 'Satisfies': [400], 'Actions': []}]"
variant fractional "rules[100]['Conditions'][0] = {'Property': 'Pos X', 'Op': '>=', 'Value': 1.5}; rules[100]['Conditions'].append({'Property': 'Pos X', 'Op': '<', 'Value': 2.5})"
variant lookin     "c['Engine Configuration']['Hash Lookahead'] = 1; c['Engine Configuration']['Hash Lookahead Input'] = '|.|'; c['Runner Configuration']['Allowed Input Sets'][0]['Inputs'].insert(0, '|.|')"

expectSolution cycle
expectSolution fractional
expectSolution lookin

echo "PASS: configuration errors are reported at load time, and the accepted variants solve optimally"
exit 0
