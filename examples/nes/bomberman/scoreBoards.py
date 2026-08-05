#!/usr/bin/env python3
"""Plan-aware board scorer for Bomberman RNG (boot-delay) sweeps — v2 (2026-08-04).

Estimates the RELATIVE jaffar solving cost of each candidate board by modeling the actual
solve plan, not surface features (v1 scored exit column only). Per board:

  PHASE A (powerup leg): open-path walk to a bomb spot whose radius-1 blast covers the powerup
    brick, fuse wait (partially overlapped by pre-positioning toward the first kill), grab.
    Bricks blocking every approach pay a serial break penalty — "as chain-free as possible".
    Enemies spawning near the powerup blast are collateral/first-kill candidates (the powerup
    bomb double-duties) and are credited out of the kill tour.
  PHASE B (kill tour): exhaustive permutation over the remaining roster; per-kill intercept =
    path distance (diffusion-discounted for later kills — wanderers blur toward you) plus an
    alignment wait (radius-2 line), with cluster credit for enemies close enough to share a
    blast and a nudge for initial headings pointing at the hunter.
  PHASE C (exit leg): from the LAST kill position to a bomb spot covering the exit brick —
    the permutation search therefore jointly prefers plans whose last kill dies NEAR the exit —
    plus the exit-chain break cost (bricks blocking the approach + the exit brick's own fuse,
    overlapped by the all-covered early walk), with collateral credit if the tour likely broke
    the exit brick already.
  K: every frame of delayed Start counts against the board, 1:1.

Usage: ./scoreBoards.py <dumpDir> [--top N]   (dumps = kNN.ram files from boot-delay variants)
"""

import argparse
import glob
import heapq
import itertools
import os
import re
import sys

LR = 2048
MAP = 0x200
COLS, ROWS, STRIDE = 31, 13, 32

# Model constants (frames; only RELATIVE accuracy matters)
WALK       = 21.4   # frames per tile pre-skates (1px per processed frame, 3 of 4 frames)
FUSE       = 160    # bomb fuse
GRAB       = 30     # step onto the revealed powerup after flames clear
DOOR       = 20     # step onto the revealed door
ALIGN      = 70     # average wait to line an enemy into a radius-2 blast (1 bomb)
ALIGN_FAVOR= 35     # alignment if the enemy's initial heading points at the hunter
CLUSTER_MIN= 35     # floor cost for an enemy sharing a blast with the previous kill
KILL_BASE  = 90     # fixed per-kill overhead (place+escape)
BRICK_SER  = FUSE + 40  # serial cost of breaking one blocking brick (1-bomb capacity)
OVL_C      = 100    # all-covered pivot: exit walk overlaps the final fuse
COL_RADIUS = 5.0    # tiles: powerup-blast collateral influence radius
DIFF_DISC  = 0.13   # per-kill-index distance discount (enemy diffusion)
DIFF_FLOOR = 0.55


def detectArm(ram):
    n = len(ram) // LR - 1
    for f in range(1, n + 1):
        p, c = (f - 1) * LR, f * LR
        if ram[p + 0x0C] == 0x90 and ram[c + 0x0C] == 0x10 and ram[p + MAP:p + MAP + ROWS * STRIDE] != ram[c + MAP:c + MAP + ROWS * STRIDE]:
            return f
    return None


def extract(ram):
    arm = detectArm(ram)
    if arm is None: return None
    b = ram[(arm + 1) * LR:]
    grid = [[b[MAP + r * STRIDE + c] for c in range(COLS)] for r in range(ROWS)]
    ex = pu = None
    for r in range(ROWS):
        for c in range(COLS):
            if grid[r][c] == 4: ex = (c, r)
            if grid[r][c] == 5: pu = (c, r)
    f = arm + 16
    if (f + 1) * LR > len(ram): return None
    enemies = []
    for i in range(10):
        x, y, st, hd = ram[f * LR + 0x584 + i], ram[f * LR + 0x598 + i], ram[f * LR + 0x5AC + i], ram[f * LR + 0x5D4 + i]
        if x != 0 and st < 32: enemies.append(((x, y), hd))
    return {'arm': arm, 'grid': grid, 'exit': ex, 'pu': pu, 'enemies': enemies}


def walkable(t): return t == 0
def breakable(t): return t in (2, 4, 5)


def dijkstra(grid, src):
    """Cost per cell in TILES (bricks cost brickTiles extra) plus bricks-crossed count grid."""
    brickTiles = BRICK_SER / WALK
    INF = 1e18
    dist = [[INF] * COLS for _ in range(ROWS)]
    bcnt = [[0] * COLS for _ in range(ROWS)]
    dist[src[1]][src[0]] = 0.0
    pq = [(0.0, src[0], src[1])]
    while pq:
        d, c, r = heapq.heappop(pq)
        if d > dist[r][c]: continue
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cc, rr = c + dc, r + dr
            if not (0 <= cc < COLS and 0 <= rr < ROWS): continue
            t = grid[rr][cc]
            if t == 1 or t == 3: continue
            isB = breakable(t)
            w = 1.0 + (brickTiles if isB else 0.0)
            if d + w < dist[rr][cc]:
                dist[rr][cc] = d + w
                bcnt[rr][cc] = bcnt[r][c] + (1 if isB else 0)
                heapq.heappush(pq, (d + w, cc, rr))
    return DistMap(dist, bcnt)


class DistMap:
    def __init__(self, dist, bricks): self.dist, self.bricks = dist, bricks
    def __getitem__(self, r): return self.dist[r]


def dijkstraOpen(grid, src):
    """Open-corridor distances: bricks (and bombs) are WALLS. INF = unreachable without breaking."""
    INF = 1e18
    dist = [[INF] * COLS for _ in range(ROWS)]
    dist[src[1]][src[0]] = 0.0
    pq = [(0.0, src[0], src[1])]
    while pq:
        d, c, r = heapq.heappop(pq)
        if d > dist[r][c]: continue
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cc, rr = c + dc, r + dr
            if not (0 <= cc < COLS and 0 <= rr < ROWS): continue
            if grid[rr][cc] != 0: continue
            if d + 1 < dist[rr][cc]:
                dist[rr][cc] = d + 1
                heapq.heappush(pq, (d + 1, cc, rr))
    return dist


def bombSpots(grid, target):
    """Walkable cells whose radius-1 blast reaches target (its free orthogonal neighbors)."""
    c, r = target
    out = []
    for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        cc, rr = c + dc, r + dr
        if 0 <= cc < COLS and 0 <= rr < ROWS and walkable(grid[rr][cc]): out.append((cc, rr))
    return out


def score(board, K):
    grid, ex, pu, enemies = board['grid'], board['exit'], board['pu'], board['enemies']
    if ex is None or pu is None: return None
    spawn = (1, 1)
    dSpawn = dijkstra(grid, spawn)

    # -- Phase A: powerup leg
    spots = bombSpots(grid, pu)
    if not spots: return None
    dOpenSpawn = dijkstraOpen(grid, spawn)
    openSpots = [(dOpenSpawn[r][c], (c, r)) for c, r in spots if dOpenSpawn[r][c] < 1e17]
    if openSpots:
        aTiles, aSpot = min(openSpots)
        chainTax = 0.0
    else:
        # Enclosed powerup (user-caught on press 37): every blocking brick is a serial
        # bomb cycle, each needing its own reachable spot -- multiplier over the folded cost.
        spotCosts = [(dSpawn[r][c], (c, r)) for c, r in spots]
        aTiles, aSpot = min(spotCosts)
        chainTax = dSpawn.bricks[aSpot[1]][aSpot[0]] * BRICK_SER * 0.6 + 80
    
    # collateral candidates: enemies whose spawn is near the powerup blast
    dPu = dijkstra(grid, pu)
    collat, toTour = [], []
    for pos, hd in enemies:
        d = dPu[pos[1]][pos[0]]
        (collat if d <= COL_RADIUS * 0.6 else toTour).append((pos, hd, d))
    colCredit = sum((KILL_BASE + ALIGN) * max(0.0, 1.0 - d / COL_RADIUS) for _, _, d in collat)
    # fuse overlap: pre-position toward the first objective while the pu bomb ticks
    tourSeed = toTour if toTour else [(e[0], e[1], 0) for e in enemies]
    dA = dijkstra(grid, aSpot)
    nextD = min(dA[p[1]][p[0]] for p, _, _ in tourSeed) if tourSeed else dA[ex[1]][ex[0]]
    ovlA = min(FUSE, nextD * WALK) * 0.8
    phaseA = aTiles * WALK + FUSE - ovlA + GRAB + chainTax

    # -- Phase B + C: joint kill-order + exit optimization (exhaustive over the tour set)
    exSpots = bombSpots(grid, ex)
    dEx = dijkstra(grid, ex)
    dMaps = {}
    def dm(p):
        if p not in dMaps: dMaps[p] = dijkstra(grid, p)
        return dMaps[p]
    bestTour, bestOrder = 1e18, None
    tourList = [(p, hd) for p, hd, _ in toTour]
    for perm in itertools.permutations(range(len(tourList))) if len(tourList) <= 7 else [tuple(range(len(tourList)))]:
        pos = (aSpot if collat or True else spawn)
        t = 0.0
        for idx, ei in enumerate(perm):
            ep, hd = tourList[ei]
            d = dm(pos)[ep[1]][ep[0]]
            w = max(DIFF_FLOOR, 1.0 - DIFF_DISC * idx)
            # heading nudge: enemy initially moving toward the hunter aligns cheaper
            dx, dy = ep[0] - pos[0], ep[1] - pos[1]
            toward = (hd == 3 and dx > 0) or (hd == 1 and dx < 0) or (hd == 2 and dy > 0) or (hd == 4 and dy < 0)
            align = ALIGN_FAVOR if toward else ALIGN
            # cluster credit: enemy within 2 tiles of previous kill shares its blast setup
            if idx > 0:
                prev = tourList[perm[idx - 1]][0]
                if abs(ep[0] - prev[0]) + abs(ep[1] - prev[1]) <= 2:
                    t += CLUSTER_MIN
                    pos = ep
                    continue
            t += d * WALK * w + align + KILL_BASE
            pos = ep
        # phase C from the LAST kill position. An exit brick with NO open bomb spot (sealed
        # between bricks) is a hard endgame tax: every sealing brick is a serial fuse with
        # nothing to overlap (user-caught blind spot on the press-17 family).
        if exSpots:
            dmp = dm(pos)
            dOpenPos = dijkstraOpen(grid, pos)
            openEx = [(dOpenPos[r][c], (c, r)) for c, r in exSpots if dOpenPos[r][c] < 1e17]
            if openEx:
                exd, exSpot = min(openEx)
                sealTax = 0.0
            else:
                exd, exSpot = min((dmp[r][c], (c, r)) for c, r in exSpots)
                # Sealed/unreachable exit approach (user-caught on press 17): serial endgame
                # bombing with nothing to overlap.
                sealTax = max(1, dmp.bricks[exSpot[1]][exSpot[0]]) * BRICK_SER * 1.2 + 80
        else:
            exd = dm(pos)[ex[1]][ex[0]]
            sealing = sum(1 for dc, dr in ((1,0),(-1,0),(0,1),(0,-1))
                          if 0 <= ex[0]+dc < COLS and 0 <= ex[1]+dr < ROWS and breakable(grid[ex[1]+dr][ex[0]+dc]))
            sealTax = sealing * BRICK_SER + FUSE
        # exit-chain: brick-passing cost is already folded into the dijkstra weights; add the exit brick's own bomb
        collBreak = 0.5 * FUSE if min(abs(ex[0] - p[0]) + abs(ex[1] - p[1]) for p, _ in tourList + [((aSpot), 0)]) <= 1 else 0.0
        phaseC = exd * WALK + FUSE - OVL_C - collBreak + DOOR + sealTax
        if t + phaseC < bestTour: bestTour, bestOrder = t + phaseC, perm
    if not tourList:
        exd = min(dm(aSpot)[r][c] for c, r in exSpots) if exSpots else 0
        bestTour = exd * WALK + FUSE - OVL_C + DOOR

    total = K + phaseA - colCredit + bestTour
    return {'total': total, 'K': K, 'A': phaseA, 'collatN': len(collat), 'colCredit': colCredit,
            'tour': bestTour, 'exit': ex, 'pu': pu, 'puD': aTiles, 'nEnemies': len(enemies)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dumpDir')
    ap.add_argument('--top', type=int, default=5)
    args = ap.parse_args()
    rows = []
    boards = {}
    for path in sorted(sorted(glob.glob(os.path.join(args.dumpDir, 'k*.ram')) + glob.glob(os.path.join(args.dumpDir, 'p*.ram')))):
        K = int(re.search(r'[kp](\d+)\.ram', path).group(1))
        board = extract(open(path, 'rb').read())
        if board is None:
            print(f'K={K:2d}: arm not detected -- skipped', file=sys.stderr)
            continue
        boards[K] = board
    # The REAL frame cost of a delay is the arm shift, not K: the menu flow duration varies
    # with where Start lands (observed: K=8 -> arm +26). Charge arm_K - arm_ref.
    armRef = min(b['arm'] for b in boards.values())
    for K, board in sorted(boards.items()):
        s = score(board, board['arm'] - armRef)
        if s is None:
            print(f'K={K:2d}: unscorable (missing exit/pu) -- skipped', file=sys.stderr)
            continue
        s['bootK'] = K
        rows.append(s)
    rows.sort(key=lambda s: s['total'])
    print(f'{"rank":>4} {"K/arm+":>7} {"est":>7} {"A(pu)":>7} {"collat":>6} {"tour+exit":>9} {"pu@":>7} {"exit@":>7} {"puD":>5}')
    for i, s in enumerate(rows):
        print(f'{i+1:>4} {s["bootK"]:>3}/{s["K"]:<3} {s["total"]:>7.0f} {s["A"]:>7.0f} {s["collatN"]:>2}/-{s["colCredit"]:>3.0f} {s["tour"]:>9.0f} '
          f'{str(s["pu"]):>7} {str(s["exit"]):>7} {s["puD"]:>5.1f}')
    print('\nTop picks (bootK, armDelta, est):', [ (r['bootK'], r['K'], round(r['total'])) for r in rows[:args.top] ])


if __name__ == '__main__':
    main()
