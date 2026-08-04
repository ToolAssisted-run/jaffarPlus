#!/usr/bin/env python3
"""Systematic stage-start detection for the Bomberman 50-floor TAS (v2 protocol).

A stage's start frame is the frame that runs its FIRST game-logic iteration: input held there
acts one full input-window earlier than anything after the load. v1 seeds were cut past this
frame and lost it on every stage (the K20fixed movie holds null on all 60 start frames).

Detection is RAM-based and immediate (no lag-frame backtracking). On the start frame the
stage-init code (a) generates the new board into the tile array at $0200 (13 rows x 32-byte
stride) and (b) writes #$10 into $0C -- the PPU_CTRL ($2000) shadow (ROM $C2AA: LDA $0C /
STA $2000; ROM $C1FA: LDA #$10 / STA $0C) -- turning the screen off to draw the board, which
stalls the game in a 13-17-frame lag group. So:

    stage start  ==  ($0C: 0x90 -> 0x10)  AND  (board array $0200 changed this frame)

The screen-off alone also fires on every stage-card draw (no board regen there); board changes
alone happen constantly in play (bricks/bombs). The conjunction matches the lag-group ground
truth 60/60 with zero false positives over the full 106,933-frame movie.

The v2 seed rule: seed (initial.sol) = full-run prefix of (startFrame - 1) inputs, so the
first searched input lands exactly on the start frame.

Usage:
  ./findStageStarts.py [solution.sol] [--config replay.jaffar] [--writeSeeds] [--seedDir DIR]
                       [--crossCheck]

--crossCheck additionally runs the player's --dumpPolls pass (per-frame joypad read counts)
and verifies each detected start frame polls input and is followed by a >=8-frame lag group.
"""

import argparse
import os
import subprocess
import sys
import tempfile

LRAM_SIZE  = 2048
LEVEL_ADDR = 0x58
PPU_SHADOW = 0x0C   # PPU_CTRL $2000 shadow: 0x90 = normal render, 0x10 = screen-off work mode
MAP_BASE   = 0x200  # board tile array: 13 rows x 32-byte stride (31 used columns)
MAP_SIZE   = 13 * 32
MIN_LAG    = 8      # cross-check only: stage loads lag 13-17 frames; in-play lag tops out at 4-5

# Chronological stage schedule: bonus stages come after these floors (MECHANICS.md: the 8th
# comes one floor early, after 39, and the 10th is the hidden one after 49, before floor 50).
BONUS_AFTER = {5: 'A', 10: 'B', 15: 'C', 20: 'D', 25: 'E', 30: 'F', 35: 'G', 39: 'H', 44: 'I', 49: 'J'}


def buildSchedule():
    """Ordered stage labels as they load in a full run: stage01..stage50 with bonuses spliced in."""
    schedule = []
    for floor in range(1, 51):
        schedule.append(f'stage{floor:02d}')
        if floor in BONUS_AFTER: schedule.append(f'bonus{BONUS_AFTER[floor]}')
    return schedule


def runPlayer(playerBin, config, solution, flag, outPath):
    cmd = [playerBin, config, solution, '--reproduce', '--unattended', '--disableRender', '--exitOnEnd', flag, outPath]
    res = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(config)) or '.', capture_output=True, text=True)
    if not os.path.isfile(outPath) or os.path.getsize(outPath) == 0:
        sys.exit(f'player pass {flag} produced no output.\ncmd: {" ".join(cmd)}\nstderr tail: {res.stderr[-2000:]}')


def detectStarts(ram):
    """Frames whose advance turned the screen off AND regenerated the board array."""
    nSteps = len(ram) // LRAM_SIZE - 1  # dump has steps 0..N (step f = state after f inputs)
    starts = []
    for f in range(1, nSteps + 1):
        prev, cur = (f - 1) * LRAM_SIZE, f * LRAM_SIZE
        if ram[prev + PPU_SHADOW] != 0x90 or ram[cur + PPU_SHADOW] != 0x10: continue
        if ram[prev + MAP_BASE:prev + MAP_BASE + MAP_SIZE] == ram[cur + MAP_BASE:cur + MAP_BASE + MAP_SIZE]: continue
        starts.append(f)
    return starts


def crossCheckPolls(polls, starts):
    """Verify each start frame polls input and is followed by a >=MIN_LAG lag group."""
    for f in starts:
        if polls[f - 1] == 0:
            sys.exit(f'cross-check FAILED: detected start frame {f} did not poll input')
        lag = 0
        while f + lag < len(polls) and polls[f + lag] == 0: lag += 1
        if lag < MIN_LAG:
            sys.exit(f'cross-check FAILED: start frame {f} followed by only {lag} lag frames (expected >= {MIN_LAG})')
    print(f'cross-check OK: all {len(starts)} start frames poll input and precede >= {MIN_LAG}-frame lag groups')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('solution', nargs='?', default='current/bomberman_full_50floors.K20fixed.sol')
    ap.add_argument('--config', default='replay.jaffar')
    ap.add_argument('--player', default=os.path.join(os.path.dirname(__file__), '../../../build/jaffar-player'))
    ap.add_argument('--writeSeeds', action='store_true', help='write <label>.initial.sol seed files (startFrame-1 inputs each)')
    ap.add_argument('--seedDir', default='.', help='directory for --writeSeeds output')
    ap.add_argument('--crossCheck', action='store_true', help='also dump joypad polls and verify the lag-group signature')
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        ramPath = os.path.join(tmp, 'ram.bin')
        runPlayer(args.player, args.config, os.path.abspath(args.solution), '--dumpRam', ramPath)
        ram = open(ramPath, 'rb').read()
        polls = None
        if args.crossCheck:
            pollsPath = os.path.join(tmp, 'polls.tsv')
            runPlayer(args.player, args.config, os.path.abspath(args.solution), '--dumpPolls', pollsPath)
            polls = [int(line.split()[1]) for line in open(pollsPath)]

    inputs = [line.rstrip('\n') for line in open(args.solution) if line.strip()]
    starts = detectStarts(ram)
    if not starts: sys.exit('no stage starts detected -- wrong solution/config?')
    if polls is not None: crossCheckPolls(polls, starts)

    schedule = buildSchedule()
    if len(starts) != len(schedule):
        print(f'WARNING: detected {len(starts)} stage starts, schedule expects {len(schedule)} '
              f'(partial run?) -- labeling the ones present', file=sys.stderr)

    def levelByte(frame):  # RAM dump step f = state after f inputs
        return ram[frame * LRAM_SIZE + LEVEL_ADDR]

    rows = [(schedule[k] if k < len(schedule) else f'extra{k}', f, levelByte(f), inputs[f - 1])
            for k, f in enumerate(starts)]

    # Per-segment frame counts, with stage starts as the sole boundary source: a stage spans its
    # start frame up to (not including) the next stage's start; bootup spans frame 1 up to the
    # first start; the final stage runs to the end of the movie (so it includes the ending
    # sequence after the last exit -- flagged in the table).
    totalFrames = len(inputs)
    print(f'{"label":10} {"startFrame":>10} {"endFrame":>9} {"frames":>7} {"$58":>4}  refInputAtStart')
    print(f'{"bootup":10} {1:>10} {starts[0] - 1:>9} {starts[0] - 1:>7} {"-":>4}')
    for k, (label, f, lvl, refInput) in enumerate(rows):
        end  = (starts[k + 1] - 1) if k + 1 < len(starts) else totalFrames
        note = '' if k + 1 < len(starts) else '  (includes ending sequence)'
        print(f'{label:10} {f:>10} {end:>9} {end - f + 1:>7} {lvl:>4}  {refInput}{note}')
    print(f'{"TOTAL":10} {"":>10} {"":>9} {totalFrames:>7}')

    if args.writeSeeds:
        os.makedirs(args.seedDir, exist_ok=True)
        for label, f, *_ in rows:
            path = os.path.join(args.seedDir, f'{label}.initial.sol')
            with open(path, 'w') as out: out.write('\n'.join(inputs[:f - 1]) + '\n')
            print(f'wrote {path} ({f - 1} inputs; first searched input = frame {f})')


if __name__ == '__main__':
    main()
