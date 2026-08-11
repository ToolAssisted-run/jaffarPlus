#!/usr/bin/env python3
"""Automated NesHawk -> QuickerNES resync for the Lode Runner full-game TAS.

The two emulators drift by occasional single lag frames, and every level start hinges on a
1-frame "select scroll-skip" window: if the skip press lands one frame off on QuickerNES, select
opens the level-select menu instead and the whole level desyncs. The fix for each drift point is
inserting or deleting null frames at the right spot (a "phase-shift null edit", same method as
the Prince of Persia NES resync) -- most importantly inside the transition jingle right before
the failing level's first input, which shifts that level's entire input block.

Method:
 1. Replay the current candidate movie from power-on, dumping per-frame RAM (fast: ~8 s).
 2. Find the first level k that never completes (its level counter never reaches k+1 within its
    scheduled span). The causal region is the preceding transition gap plus level k's play span.
 3. For every null-run in that region, and for shift magnitudes 1..MAX_SHIFT, build a probe
    candidate with that many nulls inserted (or deleted), truncated shortly after level k+1's
    scheduled start, and replay it headless. Success = the level counter reaches k+1 on schedule.
    Inserting anywhere within a null run is equivalent, so only one probe per run is needed.
 4. Apply the smallest passing edit (ties: earliest position); go to 1.

All probes replay from power-on (savestate continuation is lossy; full replay is ground truth).
"""

import json
import os
import subprocess
import sys
import tempfile
import concurrent.futures

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = os.environ.get("JAFFAR_PLAYER", os.path.join(HERE, "../../../build-lodeRunner/jaffar-player"))
CONFIG = os.path.join(HERE, "lodeRunner.jaffar")
REFERENCE = os.path.join(HERE, "fullGameNesHawk.sol")
NULL = "|..|........|"
RAMSZ = 2048
MAX_WORKERS = 12  # replay concurrency cap
PADDING = 400     # extra null frames appended so post-play transitions are always observable
MAX_SHIFT = 4     # try inserting/deleting up to this many nulls per position

WORKDIR = os.environ.get("RESYNC_WORKDIR", os.path.join(tempfile.gettempdir(), "lodeRunnerResync"))


def loadSol(path):
    with open(path) as f:
        return [line.rstrip("\n") for line in f if line.strip() != ""]


def saveSol(lines, path):
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def levelBounds(ref):
    """Per-level play spans [(level, playStart, playEnd)] from the reference movie's null gaps
    (each >=100-frame null run is a title/transition gap)."""
    runs = []
    runStart, runLen = None, 0
    for i, line in enumerate(ref + ["X"]):  # sentinel flushes the final run
        if line == NULL:
            if runLen == 0:
                runStart = i
            runLen += 1
        else:
            if runLen >= 100:
                runs.append((runStart, i - 1))
            runLen = 0
    bounds = []
    for k in range(len(runs)):
        playStart = runs[k][1] + 1
        playEnd = runs[k + 1][0] - 1 if k + 1 < len(runs) else len(ref) - 1
        bounds.append((k + 1, playStart, playEnd))
    return bounds


def replay(sol, ramPath, timeout=300):
    cmd = [PLAYER, CONFIG, sol, "--reproduce", "--disableRender", "--unattended", "--exitOnEnd", "--dumpRam", ramPath]
    res = subprocess.run(cmd, cwd=HERE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout)
    return res.returncode == 0 and os.path.isfile(ramPath)


class RamDump:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        self.frames = len(self.data) // RAMSZ

    def b(self, frame, addr):
        return self.data[frame * RAMSZ + addr]


def skipLanded(dump, k, playStart):
    """True iff level k's select scroll-skip landed: still in play with sprites shown at
    playStart+40 AND the player has actually moved. During an unskipped scroll-in the player is
    frozen while the camera pans; a mistimed select opens the level-select menu (play mode 0)."""
    f40 = playStart + 40
    if dump.frames <= f40:
        return False
    if not (dump.b(f40, 0x00DB) == 1 and dump.b(f40, 0x0003) == 0x18 and dump.b(f40, 0x00A6) == k):
        return False
    dx = abs((dump.b(f40, 0x20) + dump.b(f40, 0x22) / 8.0) - (dump.b(playStart, 0x20) + dump.b(playStart, 0x22) / 8.0))
    dy = abs((dump.b(f40, 0x21) + dump.b(f40, 0x23) / 8.0) - (dump.b(playStart, 0x21) + dump.b(playStart, 0x23) / 8.0))
    return dx + dy > 0.5


def levelCompletes(dump, k, lo, hi):
    """True iff level k is genuinely solved within candidate frames [lo, hi]: all gold collected
    (gold remaining == 0) while actively playing level k, alive. The level counter alone is not
    trustworthy -- a desynced movie stranded in the level-select menu can move it freely."""
    for f in range(max(lo, 0), min(hi + 1, dump.frames)):
        if dump.b(f, 0x00A6) == k and dump.b(f, 0x00DB) == 1 and dump.b(f, 0x0093) == 0 and dump.b(f, 0x009A) == 1:
            return True
    return False


def nullRunsIn(lines, lo, hi):
    """Start index of every null run intersecting [lo, hi)."""
    starts = []
    inRun = False
    for i in range(max(lo, 0), min(hi, len(lines))):
        if lines[i] == NULL:
            if not inRun:
                starts.append(i)
                inRun = True
        else:
            inRun = False
    return starts


def probeEdit(args):
    """Build+replay one probe candidate with one insert/delete edit applied.

    mode "complete": success = level k genuinely completes within its (shifted) span.
    mode "skip":     success = level k's select scroll-skip lands (in play, sprites shown at
                     start+40); truncates much earlier, so these probes are cheaper.
    """
    candidate, kind, pos, count, spanLo, spanHi, k, mode, tag = args
    if kind == "insert":
        probe = candidate[:pos] + [NULL] * count + candidate[pos:]
        shift = count
    else:
        # Only delete nulls actually present at this run
        if any(candidate[pos + i] != NULL for i in range(count) if pos + i < len(candidate)):
            return (kind, pos, count, False)
        probe = candidate[:pos] + candidate[pos + count:]
        shift = -count
    lo, hi = spanLo + shift, spanHi + shift
    end = (lo + 80) if mode == "skip" else (hi + PADDING)
    probe = probe[:end]
    solPath = os.path.join(WORKDIR, f"probe_{tag}.sol")
    ramPath = os.path.join(WORKDIR, f"probe_{tag}.ram")
    saveSol(probe, solPath)
    ok = False
    try:
        if replay(solPath, ramPath):
            dump = RamDump(ramPath)
            if mode == "skip":
                ok = skipLanded(dump, k, lo)
            else:
                ok = levelCompletes(dump, k, lo, hi)
    finally:
        for p in (solPath, ramPath):
            if os.path.isfile(p):
                os.remove(p)
    return (kind, pos, count, ok)


def main():
    seed = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "fullGameQuickerNESPartialResync.sol")
    outPath = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "fullGameQuickerNES.sol")
    os.makedirs(WORKDIR, exist_ok=True)

    ref = loadSol(REFERENCE)
    bounds = levelBounds(ref)
    print(f"reference: {len(ref)} frames, {len(bounds)} levels", flush=True)

    # Applied edits as (refFrame, shift): a boundary at ref frame F is shifted by the sum of
    # shifts of all edits applied at ref positions <= F. Persisted alongside the output so a
    # restart (e.g. after splicing in an externally re-solved level) keeps boundary offsets.
    editLogPath = outPath + ".editlog.json"
    editLog = []
    if os.path.isfile(editLogPath):
        editLog = [tuple(e) for e in json.load(open(editLogPath))]
        print(f"loaded {len(editLog)} prior edits from {editLogPath}", flush=True)

    # Candidate = resynced seed prefix + reference remainder (the seed's internal +/-1 edits
    # cancel out, so its trailing edge lines up with the same reference frame index). When an
    # edit log exists (restart after an external splice), the seed IS the complete candidate.
    seedLines = loadSol(seed)
    candidate = seedLines if editLog else seedLines + ref[len(seedLines):]

    def offsetFor(refFrame):
        return sum(s for (p, s) in editLog if p <= refFrame)

    def completionSpan(kIdx):
        """Candidate-coordinate window in which level k's counter must reach k+1: from its play
        start to a while past the next level's scheduled start (or movie end for the last)."""
        k, playStart, playEnd = bounds[kIdx]
        lo = playStart + offsetFor(playStart)
        nextStart = bounds[kIdx + 1][1] if kIdx + 1 < len(bounds) else playEnd + 340
        hi = nextStart + offsetFor(nextStart) + 45
        return k, lo, hi

    iteration = 0
    while True:
        iteration += 1
        fullRam = os.path.join(WORKDIR, "verify.ram")
        verifySol = os.path.join(WORKDIR, "verify.sol")
        saveSol(candidate + [NULL] * PADDING, verifySol)
        if not replay(verifySol, fullRam, timeout=600):
            print("FATAL: verification replay failed to run", flush=True)
            sys.exit(1)
        dump = RamDump(fullRam)

        failIdx = None
        for kIdx in range(len(bounds)):
            k, lo, hi = completionSpan(kIdx)
            if not levelCompletes(dump, k, lo, hi):
                failIdx = kIdx
                break

        net = offsetFor(len(ref))
        if failIdx is None:
            saveSol(candidate, outPath)
            print(f"[iter {iteration}] ALL {len(bounds)} LEVELS COMPLETE -> wrote {outPath} "
                  f"({len(candidate)} frames, net offset {net:+d})", flush=True)
            print("edit log (ref frame, shift):", editLog, flush=True)
            return

        k, spanLo, spanHi = completionSpan(failIdx)

        # Did the level's select scroll-skip land? (in play, sprites shown, player moving)
        skipOk = skipLanded(dump, k, spanLo)
        print(f"[iter {iteration}] candidate {len(candidate)} frames, net {net:+d}: "
              f"first incomplete level: {k} (scroll-skip {'landed' if skipOk else 'MISSED'})", flush=True)

        # Edit-position anchors: the transition jingle right before the level (shifts the whole
        # level block, moving its select press onto the skip window) and the in-level null runs
        # (phase nudges for mid-play divergence).
        gapRuns = nullRunsIn(candidate, spanLo - 400, spanLo)
        gapPos = gapRuns[-1] if gapRuns else spanLo - 1
        playEndC = bounds[failIdx][2] + offsetFor(bounds[failIdx][2])
        inLevel = nullRunsIn(candidate, spanLo, playEndC)

        def runChunked(jobs):
            """Run probes in chunks, early-exiting on the first chunk with a success."""
            CHUNK = MAX_WORKERS * 3
            with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
                for c in range(0, len(jobs), CHUNK):
                    results = [r for r in ex.map(probeEdit, jobs[c: c + CHUNK]) if r[3]]
                    if results:
                        return sorted(results, key=lambda r: (r[2], r[1]))[0]
            return None

        def gapJobs(mode):
            jobs = []
            for d in range(1, 311):
                jobs.append((candidate, "insert", gapPos, d, spanLo, spanHi, k, mode, f"gi{d}"))
                jobs.append((candidate, "delete", gapPos, d, spanLo, spanHi, k, mode, f"gd{d}"))
            jobs.sort(key=lambda j: j[3])
            return jobs

        def nudgeJobs(cand, base, positions, maxShift, tagPrefix):
            jobs = []
            for count in range(1, maxShift + 1):
                for i, pos in enumerate(positions):
                    jobs.append((cand, "insert", pos, count, base[0], base[1], k, "complete", f"{tagPrefix}i{count}_{i}"))
                    jobs.append((cand, "delete", pos, count, base[0], base[1], k, "complete", f"{tagPrefix}d{count}_{i}"))
            return jobs

        fix = None       # single edit fix: (kind, pos, count)
        comboFix = None  # two-edit fix: (gapKind, gapCount, kind, pos, count)

        if skipOk:
            # Tier B: in-level phase nudges at the current (working) level phase
            jobs = nudgeJobs(candidate, (spanLo, spanHi), inLevel, MAX_SHIFT, "")
            print(f"  tier B: {len(inLevel)} in-level null runs, shifts 1..{MAX_SHIFT}: {len(jobs)} probes", flush=True)
            r = runChunked(jobs)
            if r: fix = (r[0], r[1], r[2])

        if fix is None:
            # Tier A: whole-level delay search through the jingle (success = level completes)
            jobs = gapJobs("complete")
            print(f"  tier A: scroll-skip delay search at gap frame {gapPos}: {len(jobs)} probes", flush=True)
            r = runChunked(jobs)
            if r: fix = (r[0], r[1], r[2])

        if fix is None:
            # Tier C: every delay that merely LANDS the skip, each combined with in-level nudges
            jobs = gapJobs("skip")
            landing = []
            with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
                for r in ex.map(probeEdit, jobs):
                    if r[3]: landing.append((r[0], r[2]))
            landing.sort(key=lambda x: x[1])
            print(f"  tier C: {len(landing)} skip-landing delays; combining with in-level nudges", flush=True)
            for (gKind, gCount) in landing[:12]:
                shift = gCount if gKind == "insert" else -gCount
                cand2 = candidate[:gapPos] + [NULL] * gCount + candidate[gapPos:] if gKind == "insert" \
                    else candidate[:gapPos] + candidate[gapPos + gCount:]
                base2 = (spanLo + shift, spanHi + shift)
                inLevel2 = nullRunsIn(cand2, base2[0], playEndC + shift)
                jobs = nudgeJobs(cand2, base2, inLevel2, 2, f"c{gKind[0]}{gCount}_")
                r = runChunked(jobs)
                if r:
                    comboFix = (gKind, gCount, r[0], r[1], r[2])
                    break

        if fix is None and comboFix is None:
            print(f"  NO fix found for level {k} -- manual attention needed", flush=True)
            saveSol(candidate, outPath + ".stuck")
            sys.exit(2)

        def applyEdit(kind, pos, count):
            nonlocal candidate
            refPos = pos - offsetFor(pos)  # approximate ref-coordinate of the edit point
            if kind == "insert":
                candidate = candidate[:pos] + [NULL] * count + candidate[pos:]
                editLog.append((refPos, count))
            else:
                candidate = candidate[:pos] + candidate[pos + count:]
                editLog.append((refPos, -count))
            json.dump(editLog, open(editLogPath, "w"))

        if fix is not None:
            kind, pos, count = fix
            print(f"  FIX: {kind} {count} null(s) at frame {pos}", flush=True)
            applyEdit(kind, pos, count)
        else:
            gKind, gCount, kind, pos, count = comboFix
            print(f"  COMBO FIX: {gKind} {gCount} null(s) at gap frame {gapPos} + {kind} {count} at frame {pos}", flush=True)
            applyEdit(gKind, gapPos, gCount)
            applyEdit(kind, pos, count)
        saveSol(candidate, outPath + ".partial")


if __name__ == "__main__":
    main()
