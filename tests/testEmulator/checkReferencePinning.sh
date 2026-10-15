#!/usr/bin/env bash
# Reference pinning and the reference floor in the configurations the other tests do not cover:
#   1. Exact verification really compares: the volatile mask must not cover the whole state (the probe
#      used to serialize before the rules ran and without the step count, so every byte looked
#      volatile), and the reference is still pinned at every depth.
#   2. Hash Lookahead with an explicit no-op input: the pin hashes come from the player's lookahead dump,
#      exact verification compares the plain child, and the reference is pinned at every depth.
#   3. Frameskip: a reference produced with Rate 1 (two lines per step) gives a per-step floor; the
#      search must solve in 4 steps instead of stopping on a misaligned floor or frame cap.
# Every variant is written to a private temporary directory.
set -uo pipefail

JAFFAR="${1:?usage: checkReferencePinning.sh <jaffar binary> <player binary>}"
PLAYER="${2:?usage: checkReferencePinning.sh <jaffar binary> <player binary>}"

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-2}"
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB:-10}"
export JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB="${JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB:-10}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "FAIL: $1"; exit 1; }

printf '|R|\n|R|\n|R|\n|R|\n|D|\n|D|\n|D|\n|D|\n' > "$TMP/ref.sol"

python3 - "$TMP" <<'PY'
import json, sys
t = sys.argv[1]
def paths(c, name):
    s = c['Driver Configuration']['Save Intermediate Results']
    s['Best Solution Path'] = t + '/' + name + '.best.sol'
    s['Worst Solution Path'] = t + '/' + name + '.worst.sol'
    s['Win Solution Path'] = t + '/' + name + '.win.sol'
    for r in c['Game Configuration']['Rules']:
        for a in r['Actions']:
            if a['Type'] == 'Trigger Save Solution': a['Path'] = t + '/' + name + '.save.sol'
c = json.load(open('gridWalker.floor.jaffar'))
paths(c, 'pin')
c['Driver Configuration']['Reference Reward Floor']['Solution File'] = t + '/ref.sol'
c['Engine Configuration']['Reference Pinning']['Path'] = t + '/ref.hashes'
json.dump(c, open(t + '/pin.jaffar', 'w'), indent=1)
c['Engine Configuration']['Hash Lookahead'] = 1
c['Engine Configuration']['Hash Lookahead Input'] = '|.|'
c['Runner Configuration']['Allowed Input Sets'][0]['Inputs'].insert(0, '|.|')
c['Engine Configuration']['Reference Pinning']['Path'] = t + '/ref.lookahead.hashes'
paths(c, 'lookahead')
json.dump(c, open(t + '/lookahead.jaffar', 'w'), indent=1)
c = json.load(open('gridWalker.frameskip.jaffar'))
paths(c, 'frameskip')
c['Driver Configuration']['Reference Reward Floor'] = {'Enabled': True, 'Solution File': t + '/frameskip.ref.sol', 'Tolerance': 0.0}
json.dump(c, open(t + '/frameskip.jaffar', 'w'), indent=1)
PY

pinHits() { printf '%s\n' "$1" | grep -oE 'deepest [0-9]+' | tail -n 1 | grep -oE '[0-9]+'; }

# 1. Exact verification
"$PLAYER" "$TMP/pin.jaffar" "$TMP/ref.sol" --reproduce --disableRender --exitOnEnd --unattended --dumpHashes "$TMP/ref.hashes" > /dev/null 2>&1 || fail "player hash dump failed"
out="$("$JAFFAR" "$TMP/pin.jaffar" 2>&1)"
[[ "$out" == *"Step 8 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "pinned run did not solve at step 8"; }
mask="$(printf '%s\n' "$out" | grep -oE 'volatile mask covers [0-9]+ / [0-9]+')"
read -r covered total <<< "$(printf '%s\n' "$mask" | grep -oE '[0-9]+' | tr '\n' ' ')"
[[ -n "$total" && "$covered" -lt "$total" ]] || fail "the volatile mask covers the whole state ($mask): exact verification compares nothing"
[[ "$(pinHits "$out")" -eq 8 ]] || fail "the reference was not pinned at every depth (deepest $(pinHits "$out"))"

# 2. Hash Lookahead with exact verification
"$PLAYER" "$TMP/lookahead.jaffar" "$TMP/ref.sol" --reproduce --disableRender --exitOnEnd --unattended --dumpHashes "$TMP/ref.lookahead.hashes" --dumpHashesLookahead 1 > /dev/null 2>&1 \
  || fail "player lookahead hash dump failed"
out="$("$JAFFAR" "$TMP/lookahead.jaffar" 2>&1)"
[[ "$out" == *"Step 8 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "lookahead pinned run did not solve at step 8"; }
[[ "$(pinHits "$out")" -eq 8 ]] || fail "with Hash Lookahead the reference was not pinned at every depth (deepest $(pinHits "$out"))"

# 3. Frameskip reference floor: produce the reference with the same frameskip config, then race it
python3 - "$TMP" <<'PY'
import json, sys
t = sys.argv[1]
c = json.load(open(t + '/frameskip.jaffar'))
del c['Driver Configuration']['Reference Reward Floor']
json.dump(c, open(t + '/frameskip.noref.jaffar', 'w'), indent=1)
PY
out="$("$JAFFAR" "$TMP/frameskip.noref.jaffar" 2>&1)"
[[ "$out" == *"Step 4 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "frameskip reference run did not solve at step 4"; }
cp "$TMP/frameskip.best.sol" "$TMP/frameskip.ref.sol"
out="$("$JAFFAR" "$TMP/frameskip.jaffar" 2>&1)"
[[ "$out" == *"Reference reward floor computed from solution"*": 5 steps"* ]] || { printf '%s\n' "$out" | grep -i "reference" | head -5; fail "the frameskip floor trace is not one entry per search step (expected 5 for 4 steps)"; }
[[ "$out" == *"Step 4 - Exit Reason: Solution found."* ]] || { printf '%s\n' "$out" | tail -n 15; fail "frameskip floor run did not solve at step 4"; }

echo "PASS: exact pin verification compares, lookahead pinning holds the reference, and the frameskip floor is per step"
exit 0
