#!/usr/bin/env python3
"""Inter-stage RNG reseed sweep: manipulate the tail of a solved stage to produce layout
options for the NEXT stage, score them plan-aware, and rank (v2, 2026-08-04).

RNG facts (see MECHANICS / board-rng memory): the stream advances on live enemy AI decisions
only, so it FREEZES once the last enemy dies — post-clear transition edits cannot reseed.
Levers that work, both applied here:
  - WIGGLE (cost 0): flip a null input to a direction on a frame while enemies are still
    alive (the final fuse window is mostly idle nulls). Enemy AI reacts to the player's
    position, so decision timing shifts and the next stage's board changes -- but the clear
    frame is unchanged. Validated: same clear frame, deathless.
  - INSERT (cost k): insert k nulls before the final detonation, delaying the clear by k;
    the extended live-enemy window consumes different draws.

Each variant is replayed with a null tail; the next stage's arm is detected (screen-off +
board regen), the board extracted and scored with the plan-aware scorer, and the total
ranked as score + (armAbs - min armAbs).

Usage: ./reseedBoundary.py <stageMovie.sol> [--window 140] [--inserts 20] [--top 6]
       (stageMovie = boot seed + win, concatenated; power-on replay via replay.jaffar)
"""

import argparse
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scoreBoards import extract, score, LR

NULL = "|..|........|"
DIRS = {'U': "|..|U.......|", 'D': "|..|.D......|", 'L': "|..|..L.....|", 'R': "|..|...R....|"}
PLAYER = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../build/jaffar-player')


def replayDump(sol, ramPath, cwd):
    subprocess.run([PLAYER, 'replay.jaffar', sol, '--reproduce', '--unattended', '--disableRender',
                    '--exitOnEnd', '--dumpRam', ramPath], cwd=cwd, capture_output=True)
    return open(ramPath, 'rb').read() if os.path.isfile(ramPath) else None


def clearFrame(ram):
    n = len(ram) // LR - 1
    for f in range(1, n + 1):
        if ram[(f - 1) * LR + 0x5E] == 0 and ram[f * LR + 0x5E] == 1: return f
    return None


def killerPlacementFrame(ram, lastKill):
    """Placement frame of the bomb whose detonation makes the final kill: find the slot that
    deactivates (explodes) nearest the last kill, then walk back to its activation. Inserting
    nulls BEFORE this frame shifts the entire kill schedule (the fuse is timer-driven), which
    extends the live-enemy RNG window -- the effective reseed lever for fuse-mode stages."""
    best, bestSlot = None, None
    for i in range(10):
        for f in range(1, lastKill + 3):
            if ram[(f - 1) * LR + 0x3A0 + i] != 0 and ram[f * LR + 0x3A0 + i] == 0:
                if best is None or abs(f - lastKill) < abs(best - lastKill): best, bestSlot = f, i
    if best is None: return None
    for f in range(best, 0, -1):
        if ram[(f - 1) * LR + 0x3A0 + bestSlot] == 0 and ram[f * LR + 0x3A0 + bestSlot] != 0: return f
    return None


def lastKillFrame(ram, upTo):
    """Last frame where the alive (table) count drops -- the final kill. RNG freezes here."""
    def alive(f):
        return sum(1 for i in range(10) if ram[f * LR + 0x584 + i] != 0 and ram[f * LR + 0x5AC + i] < 32)
    last = None
    prev = alive(0)
    for f in range(1, upTo + 1):
        a = alive(f)
        if a < prev: last = f
        prev = a
    return last


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('movie')
    ap.add_argument('--window', type=int, default=140, help='pre-clear frames eligible for wiggles')
    ap.add_argument('--inserts', type=int, default=20)
    ap.add_argument('--top', type=int, default=6)
    ap.add_argument('--outDir', default='/tmp/claude-1000/-home-jaffar-jaffarPlus/71b2dfef-217f-4977-bbb0-00b9c33fe7c9/scratchpad/reseed')
    args = ap.parse_args()
    cwd = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(args.outDir, exist_ok=True)

    movie = [l.rstrip('\n') for l in open(args.movie) if l.strip()]

    # Baseline: replay with a long tail, find the clear and the next stage's arm
    basePath = f'{args.outDir}/base.sol'
    open(basePath, 'w').write('\n'.join(movie + [NULL] * 600) + '\n')
    ram = replayDump(basePath, f'{args.outDir}/base.ram', cwd)
    clr = clearFrame(ram)
    board = extract(ram[clr * LR:])  # search the arm after the clear
    if board is None: sys.exit('baseline: next arm not found (tail too short?)')
    baseArm = clr + board['arm']
    lk = lastKillFrame(ram, clr)
    lp = killerPlacementFrame(ram, lk)
    bs0 = score(board, 0)
    print(f'baseline: killer placed {lp}, last kill {lk}, clear {clr}, next arm {baseArm}, board est {bs0["total"]:.0f} pu={board["pu"]} exit={board["exit"]}')

    # Variant generation
    variants = {}  # name -> (solPath, cost)
    for f in range(max(1, lp - args.window), lp):
        if movie[f - 1] != NULL: continue
        if (ram[f * LR + 0x33] & 3) == 0: continue  # phase-0: input swallowed pre-skates, wiggle is a no-op
        for d, inp in DIRS.items():
            var = list(movie)
            var[f - 1] = inp
            name = f'w{f}{d}'
            p = f'{args.outDir}/{name}.sol'
            open(p, 'w').write('\n'.join(var + [NULL] * 600) + '\n')
            variants[name] = (p, 0)
    for k in range(1, args.inserts + 1):
        var = movie[:lp - 8] + [NULL] * k + movie[lp - 8:]
        name = f'i{k:02d}'
        p = f'{args.outDir}/{name}.sol'
        open(p, 'w').write('\n'.join(var + [NULL] * 600) + '\n')
        variants[name] = (p, k)
    print(f'{len(variants)} variants; replaying (12-way parallel)...')

    sols = ' '.join(p for p, _ in variants.values())
    subprocess.run(f'ls {args.outDir}/[wi]*.sol | xargs -P 12 -I{{}} sh -c '
                   f'\'n=$(basename {{}} .sol); {PLAYER} replay.jaffar {{}} --reproduce --unattended '
                   f'--disableRender --exitOnEnd --dumpRam {args.outDir}/$n.ram >/dev/null 2>&1\'',
                   shell=True, cwd=cwd)

    # Evaluate
    rows, boardsSeen = [], {}
    for name, (p, cost) in sorted(variants.items()):
        rp = f'{args.outDir}/{name}.ram'
        if not os.path.isfile(rp): continue
        ram = open(rp, 'rb').read()
        c = clearFrame(ram)
        if c is None: continue
        if cost == 0 and c != clr: continue  # wiggle broke the win timing -> invalid
        board = extract(ram[c * LR:])
        if board is None: continue
        armAbs = c + board['arm']
        key = (board['pu'], board['exit'], tuple(sorted(p for p, _ in board['enemies'])))
        s = score(board, 0)
        if s is None: continue
        rows.append({'name': name, 'cost': cost, 'armAbs': armAbs, 'est': s['total'], 'pu': board['pu'],
                     'exit': board['exit'], 'collat': s['collatN'], 'key': key})
    if not rows: sys.exit('no valid variants')
    minArm = min(r['armAbs'] for r in rows + [{'armAbs': baseArm}])
    for r in rows: r['total'] = r['est'] + (r['armAbs'] - minArm)
    # baseline row
    bs = score(board, 0)
    rows.sort(key=lambda r: r['total'])
    seen = set()
    print(f'{"name":>8} {"cost":>4} {"armAbs":>6} {"est":>6} {"total":>6} {"pu":>8} {"exit":>8} {"col":>3}')
    shown = 0
    for r in rows:
        if r['key'] in seen: continue
        seen.add(r['key'])
        print(f'{r["name"]:>8} {r["cost"]:>4} {r["armAbs"]:>6} {r["est"]:>6.0f} {r["total"]:>6.0f} '
              f'{str(r["pu"]):>8} {str(r["exit"]):>8} {r["collat"]:>3}')
        shown += 1
        if shown >= 40: break
    print(f'\nTop picks: {[r["name"] for i, r in enumerate(rows) if r["key"] not in list(seen)[:0]][:0]}')
    top = []
    seen2 = set()
    for r in rows:
        if r['key'] in seen2: continue
        seen2.add(r['key'])
        top.append((r['name'], r['cost'], r['armAbs'], round(r['total'])))
        if len(top) >= args.top: break
    print('Top distinct boards:', top)


if __name__ == '__main__':
    main()
