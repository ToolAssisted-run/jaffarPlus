# NES Bomberman — TAS solving notes

Companion to `MECHANICS.md` (game facts). This file documents **how the 50-floor deathless TAS
was built with JaffarPlus**: the search setup, the game-module reward/input knobs, and the
hard-won lessons — especially the floor-50 optimization. Compiled 2026-08-03.

## Deliverables (the essentials in this folder)

- **`bomberman_full_50floors.K20fixed.sol`** — the complete deathless 50-floor movie from power-on
  (106,933 frames). This is *the* TAS.
- **`stage50.LOCKED.initial.sol`** / **`stage50.LOCKED.win.sol`** — the canonical final floor 50
  (seed through floor 49 + the winning 833-input / 831-frame solve).
- **`stageNN.jaffar`** + `stageNN.initial.sol` + `stageNN.winNNNN.sol` — per-stage search config,
  seed, and winning solution (reproducibility chain for floors 1–49 + bonuses A–J).
- `MECHANICS.md`, `Bomberman (USA).nes`.

## Search approach

- One JaffarPlus BFS **per stage**: `stageNN.initial.sol` seeds the emulator to the stage arm,
  the search solves that stage, its win is appended, and the next stage seeds from the result.
- **v2 stage-start rule (v1 got this wrong on every stage)**: a stage's start frame is the
  frame that runs its first game-logic iteration — input held there acts a full input-window
  earlier than anything later, because the board draw then stalls the game in a 13–17-frame
  lag group (no joypad poll). v1 seeds were cut past this frame, so all 60 stage solves
  started late (the K20fixed movie holds null on all 60 start frames). **RAM detection
  (immediate, no lag backtracking)**: on the start frame the stage-init code regenerates the
  board array at `$0200` (13 rows × 32-byte stride) AND writes `#$10` into `$0C`, the
  PPU_CTRL `$2000` shadow (ROM `$C2AA: LDA $0C / STA $2000`, `$C1FA: LDA #$10 / STA $0C`) to
  turn the screen off for the draw. `start ⇔ ($0C: 0x90→0x10) ∧ ($0200 board changed)` —
  screen-off alone also fires on stage-card draws, board changes alone happen all through
  play; the conjunction is exact (60/60, zero false positives over 106,933 frames). The
  stage seed is the full-run prefix of `startFrame − 1` inputs (first searched input = start
  frame). `findStageStarts.py` implements this from a single `jaffar-player --dumpRam` pass,
  labels starts against the stage schedule, writes seeds with `--writeSeeds`, and with
  `--crossCheck` verifies the lag-group signature via `--dumpPolls` (per-frame `$4016/$4017`
  read counts from the QuickerNES `Joypad Read Count` property; 0 = lag frame).
- **RETRACTION + the real bug (2026-08-04)**: an earlier "seed-boundary input-death trap" at
  the stage05 cut was a test artifact — the probe .sol files had the seed prefix prepended
  while the config ALSO seeded them (double-seed), so they replayed the movie's own leading
  nulls from the stage arm. A byte-level round-trip harness (serialize → snapshot → perturb →
  deserialize → snapshot; plus continuation tests at pre-stall/mid-stall/stall-end
  boundaries) proved QuickerNES serialize/deserialize is **fully faithful** at stage-arm
  boundaries, start-frame latch included. All `startFrame − 1` seed cuts work.
  **Pitfall to remember: never prepend the seed to a .sol replayed under a seeding config.**
- **The REAL defect (FIXED in bomberman.hpp)**: the module hash excludes the pad mirrors
  ($10–$17) — correct in normal play (the next poll overwrites them), but during the load
  stall polls stop and the start frame's latched input IS causal (the in-flight stage-init
  iteration processes it; movement banks +1/+2 px before the stall ends). A start-frame-input
  child and the null child hashed identically for ~13 frames, so dedup extinguished the latch
  lineage before its position diverged — **no search could ever exploit the start-frame
  input**. Fix: hash `$12/$13` only while `$0C == 0x10` (screen-off load window; zero churn in
  play). Measured on the stage05-arm micro-benchmark: flat alphabet 69 → **68**, module
  alphabet 70 → **68**.
- **Start-frame input is worth real pixels**: holding a direction on the stage-start frame
  gives +1px (stage01) to **+2px (stage05)** head start over pressing one frame later — the
  init iteration processes it and movement banks during the load stall.
- **v2 alphabet (module, `"Allow Composite Directions": true`)**: the game module now registers
  and offers UL/UR/DL/DR (diagonals; where both components legal) and LR/UD (opposing pairs;
  where either component legal) through the corridor-parity gate, plus a **straddle window**
  (perpendicular directions also offered within 3px of a corridor cell's edge — the
  cornering assist engages there, which the original parity proofs missed). Configs enabling
  the flag MUST list the six new input strings in some input set (never-satisfiable is fine)
  or the runner's string map crashes. The tracked player position now snaps to raw during the
  load window ($0C==0x10) so the parity gate is correct on the first playable frame.
  Micro-benchmark (stage05 arm → crossroad (3,3)): singles-only 71 steps, module composites
  70, flat-config composites 69; after the load-window latch-hash fix (see below) both
  composite alphabets reach **68**. Still open: offering A as a pure align tool at bomb
  capacity (the relevance gate never offers it — its "prunes no positional lines" assumption
  is disproven by the refused-A align mechanic).
- Objective per floor: kill all enemies, then step on the exit (hidden under one brick, active
  only once enemies are dead). Win rule = tripwire on **"Enemies Alive"** ($9C is glitchy — see
  MECHANICS; keying rules on it caused false wins).
- **Full-fidelity serialization is mandatory for all post-bonus stages**: `"Disabled State
  Properties": []`, `"Nametable Block Size": 4096`, `"Precise State Timing": true`. The bonus
  rampage is a glitch region where reduced serialize/deserialize diverges from live replay.
- Bonus stages (every ~5 floors + a hidden 10th before floor 50): player is invincible, timer is
  input-independent, goal is pure kill count (area-denial + centroid reward stack).

## Game-module config knobs (games/nes/bomberman/bomberman.hpp)

All default to preserve the solved floors 1–49; set per-stage as needed.

- **`Force Immediate Bomb Placement`** (bool) — forces A on the first free-slot frame.
- **`Force Immediate Detonation`** (bool, default true) — a ticking bomb forces B (exclusive:
  move+B composites + plain B). `false` offers B as an *addition* alongside movement/A/null
  (freer, can trim lag frames, but explodes the branching factor).
- **`Any-Brick Ladder Reward`** (float, default 10) — per-brick reward for every plain brick
  destroyed. **Do not lower this thinking it's noise**: at 10 it steers the search through the
  path-opening bricks and produces *shorter* routes. Dropping to 0.1 made floor 50 *longer*
  (911 vs 835). See "reward lessons" below.
- **`Disable Input Restrictions`** (bool, default false) — **everything-goes** mode: every input
  frame offers the full 20-input joypad universe (all directions, A, B, and every move/A/B
  composite) with NO parity/hazard/detonation gating. Astronomical branching — use ONLY on short
  localized windows (see endgame technique). When enabled you MUST also declare the composite
  input strings in the config's `Allowed Input Sets` (they're registered in the emulator but the
  runner's `_inputStringMap` — used by the "Show Allowed Inputs" printer — is built only from
  config-declared inputs; otherwise `map::at` crashes).
- Powerup-skip: `"Powerup Stat"` = the stage's held stat → `_powerupObtained` true from entry,
  skipping the grab detour. Rule action `"Set Powerup Grab Bonus"` (100000) rewards grabbing;
  zero it on floors where the powerup isn't needed to win (e.g. floor 50).
- Exit pull: rule action `"Set Exit Chain Magnet"` `Head Intensity` = the closeness-to-exit-door
  magnet (approach reward = −intensity × headDist).

## Floor-50 optimization (1557 → 831 frames)

Floor 50 is the hardest board (1K 2V 5P 2T = 2 Pontans + 5 Pass, homing enemies). The journey:

1. **Board reseed via mid-rampage frame insertion.** The floor-50 layout RNG is a function of the
   preceding bonus-J rampage. Editing the bonus *ending*/gap does NOTHING (RNG latches once the
   rampage enemies are gone). **Inserting K null frames mid-rampage (~frame 104400)** reseeds it.
   Swept K=1..40 → many center/right exits (cols 20–28) vs the original far-left col-3 exit.
2. **Ran Jaffar on the 6 best boards** (equal budget, capped at the running best). Board **K20**
   (K=20 insertion, exit col 25 upper-right) solved shortest: **868**, beating the col-3 board's
   1557 (46% shorter — the upper-right exit removes the backtrack).
3. Iterating the seed starting-point trimmed 868 → **835**.
4. **Endgame everything-goes search** cracked the plateau: seed from the solution minus its last
   100 frames, then search that tail with `Disable Input Restrictions` + NO checkpoints +
   exit-magnet Head Intensity ×20 (→100). Total input freedom on a short window found a tighter
   approach to the door — exit reached in 97 frames vs 100 → **831**, verified deathless.

### Reward lessons (what did NOT work — all "no beat found, search-limited")

- **Reference-reward-floor pinning is brittle for bomberman.** The reward has large one-frame
  transient spikes (~+380/detonation, +3000/kill, +100000/powerup-grab). Tolerance-0 pinning
  false-cancels the whole search on a 1-frame timing skew at any spike (died at step 59/303).
  Flexible detonation diverges on kill *order* (fell −2556 behind). Forced detonation + a
  tolerance ≥ the spike amplitude runs deep but found no sub-835 line. Set tolerance ≈ the
  reference's max transient jump if you use it at all.
- **Bigger DB (up to 200 GB) alone did not beat 835.** Localized short-window free search did.
- Brick reward 0.1/1/10 sweep and powerup-grab removal all failed to beat the original-reward 835;
  the endgame free search is what improved it.

## Performance / operational gotchas

- **`jaffar-player` lean patch** (source/playback.hpp + player.cpp): `Playback::initialize` was
  caching a 256 KB framebuffer *per step* → ~20 GB RSS for a full-movie replay. The
  `storeRendererState=!disableRender` flag skips it in headless mode → **0.5 GB flat**. Without
  it, parallel board-probing sweeps OoM the box. Savestate load is lossy for *continuation*
  (diverges after 1 frame) — only full continuous replay is byte-accurate, so probing must
  replay from power-on; parallelism (≤~12 full replays, or many lean ones) is the only accelerator.
- **Power-on death-check**: replay the full movie, scan `$5C != 0` for gameplay frames (frame ≥ 2;
  frames 0–1 are 0xFF uninitialized boot RAM — not deaths), and confirm floor 50 clears `$58→51`,
  `$9C=0`.
- ~57 fps single-thread replay is the emulation floor (precise-timing/dumpRam are not the cost).

## v2 campaign (2026-08-04+): greedy per-stage rebuild vs the reference TAS

The v1 movie (106,933 frames) is being rebuilt stage by stage against the best known solution
(YouTube 3EjTKqNN8p8, F-counter overlay = frame ground truth, **final F76663**; per-stage
timings in `refvideo.timings.tsv`). Their bonuses equal ours — the whole 30k gap is in the
floors (powerup investment: they buy bomb capacity early, capacity 6 by mid-run). Strategy:
greedy — make every stage faster than both v1 and the reference segment.

### Results so far (absolute frame of stage clear)

| stage | v2 clear | v1 ~abs | ref segment | notes |
|-------|----------|---------|-------------|-------|
| 01 | 1167 (win 857 post-arm, arm 311) | 1390 | 2051 (invest) | 50 GB DB re-run |
| 02 | 2815 (win 1290 post-arm, arm 1526) | ~3700 | 1899/floor | 10 GB; 50 GB re-run pending |
| 03 | 4932 (win 1759 post-arm, arm 3174) | — | — | 60GB + Hash Lookahead 1 direct run (1763), then −4 via cut-750 reseed of the SAME ending (e62e00ae — RNG-preserving polish). Stage04 board: pu(27,1) ex(29,9) est 2978 (vs staged ending's 3703); stage04 arm 2118 post-arm03. Ending survey: all reseeds canalize onto e62e00ae — no third ending found |
| 04 | 6456 (win 1166 post-arm, arm 5291) | — | — | first solved arm-to-win in ONE 60GB+lookahead run (1198, ending 8cf166cb), then REBANKED onto ending 49446bcb via the stage05 board hunt: −32 frames AND a far better stage05 board. Stage05 arm 6820 (was 6855). Transition slack sweep (358 A-press variants): zero gains |

### At-will (Detonator) mode: every fuse-weighted term needs a special case

Reward terms that scale with fuse progress are meaningless once the Detonator is held — bombs never
tick, so `$3D2` stays ~0. The pending/threat terms already special-cased this (`progress = atWill ?
1.0f : elapsed/160`), but the **blast-escape danger did not**: it weighted by the raw fuse timer, so
standing ON a live bomb scored a −2.8 penalty while the head-approach penalty and the (9b)
next-brick pull — both maximal at zero distance — paid hundreds to hug the brick. The search parked
on its own bomb for 600+ steps on stage05, never escaping and therefore never detonating.

A second trap sat behind it: the head-approach pull uses PATH distance, and a bomb tile costs 144px
vs 16px of floor, so stepping off your own bomb swings that term from −80 to −800 — more than the
danger fix pays. The approach pull is now **suspended while the head is committed** (a ticking cross
reaches it); once the bomb is set, distance to that brick is meaningless and the job is
escape-then-detonate. It re-engages for the next head automatically.

Fixed in `bomberman.hpp` block "(10) Blast-line escape danger" (at-will ⇒ progress 1.0): the same
state now scores −450 (danger 3.00), so escape-then-detonate outranks hugging. Fuse-mode stages are
unchanged. **If a Detonator stage stalls, audit the fuse-weighted terms first.**

Debug recipe: seed a 2-step probe from the live `/tmp/jaffar.best.sol` with
`Runner Configuration/"Show Allowed Inputs": true` (Driver rejects unknown keys) and a tiny DB, then
re-evaluate the module's gate conditions from `--dumpRam` in python — that distinguishes "input not
offered" from "offered but not rewarded".

### Counter power-up stats are CUMULATIVE — set the threshold per stage

`Powerup Obtained` for the Bombs stat is `$74 >= "Powerup Bombs Threshold"` (default 1), and `$74`
persists across stages. Stage05 is the second Bombs stage ($74 already 1 from stage02), so with the
default it reported the power-up as grabbed on frame one: power-up magnets zeroed, the +100000 grab
bonus pre-paid, fail rule 1997 (power-up destroyed) disarmed, and the win rule's
`Powerup Obtained` term pre-satisfied. The run churned 1630 steps with zero kills at a flat ~102.5k.

Set `"Powerup Bombs Threshold": <$74 at this stage's arm> + 1` (stage05 = 2, stage06 = 3, ...), and
measure $74 at the arm rather than assuming. Same for Flames stages via `Powerup Flame Threshold`.
Boolean stats (Detonator/Speed/Bombpass/Wallpass/Flamepass) need nothing.

### Win rules need a `Current Step > 50` guard (stale clear-latch insta-win)

`Game End Status` ($5E) is the stage-clear latch, and on some boundaries it stays set through the
ENTIRE transition, clearing one frame into the next load — while $58 (Level) increments *before*
the load. A seed cut at arm−1 can therefore land on a state that reads `Level = N, cleared,
not loading, powerup held`: rule 1999 fires immediately and the search exits at step 1 with a
1-input "win" (observed at stage04→05; frames 6805-6820 latched, cleared 6821).

`Loading == False`, `Level`, `Enemies Alive`, `Time Left` and `Bricks Left` CANNOT distinguish this
state — they all read the previous stage's values. The fix, now applied to every v2 stage config:
prepend `{"Property": "Current Step", "Op": ">", "Value": 50}` to every win rule (1999 and the 6666
canary). `Current Step` starts at 0 for the search (the seed replays below the game layer), and no
real stage win happens under ~850 steps.

When a win looks wrong, decode the log's `Rule Status` bitstring against the config's rule ORDER
before theorising — that is how this was pinned on 1999 rather than the 6666 corruption canary.

### Boot Start press is board manipulation — do NOT "optimize" it

The boot holds Start until frame ~141 (stage01 board loads at 311). That delay is deliberate: the
title-screen duration determines the RNG state at board generation, so the Start frame *picks*
stage01's layout (and, through the ending chain, every later board). `scoreBoards.score(board, K)`
exists for this sweep and charges every delayed frame against the board 1:1.

A naive sweep looks like free money — Start registers from frame 12, and Start@14 loads the board
at 181 (~130 earlier than ours, 17 ahead of the reference's 198) — but the campaign already tested
earlier presses: `kSweep.results` records `P=141 -> win 1016 (abs 1330)` while `P=017/022/037/143`
produced **no win at all** within the step cap. Their boards were not merely slower, they were
unsolvable at that budget. `stage01.k00..k44.*` are the follow-up sweep of extra delay on top.

Revisiting the boot re-rolls the entire campaign (all boards change, every stage re-solves), so it
needs a full scored sweep, not a frame-count argument.

### Stage05 board hunt (2026-08-13) — the ending-diversity method pays off

Unlike stages 03/04 (which canalized onto a single ending), a cut-500 reseed of stage04 (before
the 561 double-kill, 5 enemies still drawing RNG) yielded **4 distinct endings**:

| ending | stage05 board | board est | notes |
|---|---|---|---|
| **49446bcb** | pu(13,4) exit(24,7) | **2856** | ADOPTED |
| 8cf166cb | pu(24,7) exit(3,8) | 4074 | the original banked ending — powerup and exit at OPPOSITE ends |
| 4df867cb | pu(29,8) exit(1,10) | 4209 | |
| 882d6acb | pu(19,1) exit(17,6) | 4432 | |

The winner initially looked like a 254-frame trade (the cheap 6GB reseed clears stage04 late).
**RNG-safe polish erased it**: the stream freezes at the last kill, so re-searching only the
post-last-kill exit walk (20GB + lookahead, seed cut at the kill) provably preserves the ending
key -- 417-frame walk -> 133, making the whole line 1166 vs the old 1198. Strictly better on both
axes. Method to reuse: cut at the last kill, re-search the walk, verify the collected key matches.

Two tooling bugs fixed while running this (both would corrupt any future hunt):
- `scoreBoards.extract` read enemy records at +4 offsets with 10 iterations -- it dropped slot 3
  (the 7th enemy of stages 5/6/11+) and invented phantoms past the state array. Now reads the true
  array bases; validated against the stage table (6 for stage04, 7 for stage05).
- `reseedBoundary.py` detected the clear via `$5E`, which reads 0 for entire v2 movies, and had no
  validity guard: its first stage05 sweep ranked three DEATHS (player dies -> stage restarts ->
  "new board") above every real candidate. Now uses the last screen-off+board-regen transition and
  requires deathless + stage-advanced.

### Hash Lookahead verdict (stage03 A/B, 2026-08-12)

Engine `"Hash Lookahead": 1` hashes each child after +1 null advance — splits states that
differ only in latched pending inputs (hidden lag frames, e.g. during explosions; same
developer pattern as Lode Runner). Results on the stage03 funnel:
- 2 GB DB: catastrophic — productive lineages evicted (the lookahead multiplies distinct
  states; small DBs can't hold them), best collapsed to the floor band by step 600.
- 60 GB DB: transformative — break-cohorts formed every 1-2 steps (vs one per ~150 without
  lookahead), detonator grabbed at step 688 directly from the arm (first large-DB run ever
  to clear the funnel), win at 1763 tying the staged line in ONE clean run.
- Template guidance: funnel-heavy boards -> Hash Lookahead 1 + LARGE state DB (60 GB);
  never pair lookahead with a small DB. Costs ~2x emulation per child (~32 s/step at 80%).

### The per-boundary procedure (now routine — repeat for every stage N → N+1)

1. **Win-state collection harvest** (see engine feature below): the stage-N search runs with
   `"Stop Frames After First Win": 200` and Driver `"Win State Collection"` enabled
   (dedup on RNG State 1–4, output `winCollectNN/`). Every distinct frozen-RNG ending =
   a distinct stage-N+1 board.
2. **Score each ending's next board**: append ~600 nulls (the clear→next-arm transition is
   ~360 frames), replay, `--dumpRam`, then `scoreBoards.extract(ram[clearFrame*LR:])`
   (MUST offset past the current stage or extract() finds the wrong arm) and
   `scoreBoards.score()` (plan-aware: powerup leg + kill-order permutations + last-kill→exit
   leg + real arm-delta cost). `score() == None` = unsolvable-class board (unreachable
   powerup/exit) — veto.
3. **Pick** the best ending (often the shortest win is also the best board — but not always).
4. **Transition NMI-slack pass**: greedy A-press stacking over the dead post-clear window
   (RNG is frozen after the last kill, so presses can't change the board — verify by board
   signature anyway). Each round: try a single A on every null frame, keep the press that
   most reduces the next arm, repeat until no gain. Gains: stage01→02 −4, stage02→03 −1.
   Boot equivalent (presses around the initial Start): −4 free; bigger shifts break the
   mod-4 input-phase pre-skates of an already-banked win (only matters if reusing a solve).
5. **Rebank** `stageNN.initial.sol` cut at (finalArm − 1) with the slack presses embedded.
6. **Config**: `sed` the previous stage's .jaffar (initial sol path, winCollect path,
   `"Powerup Stat"` from the stage table in MECHANICS.md). The v2 reward stack carries over
   unchanged; per-stage fail-rule gates: `{"Loading","==",False}` on everything, and the
   stage-card guard keyed on `{"Enemies Alive",">",0}` (Time Left is unusable — the card
   inherits the previous stage's nonzero timer).
7. **Launch detached** (`setsid`), symlink logs, watch for `Exit Reason`. On win: bank
   `stageNN.full.sol`, verify deathless ($5C scan), render segment video
   (`make_video.py … --start arm --end clear`), send, go to 1.

### Engine feature: Win State Collection + Stop Frames After First Win (in master source)

- Driver config `"Stop Frames After First Win": N` — replaces/generalizes the old
  `"End On First Win State"` (legacy alias: true→0). N=0 stops at first win; N>0 keeps
  searching N more steps past the first win (collecting alternate endings).
- Driver config `"Win State Collection": {Enabled, Dedup Properties: ["RNG State 1"…],
  Path Prefix, Max Files}` — every win whose dedup-property byte-key is new gets its full
  input history saved to `<prefix><hexkey>.sol` + a manifest line. The log prints one
  per-step counter line (`Win States Collected: N/max`); hitting max terminates the run
  (exit reason "Win collection complete").
- GOTCHA (cost us two silent crashes): the driver must apply the collection config to the
  engine *after* engine construction, not at parse time.

### Reference-prune polish mode (debuted on stage02 — the standard post-win step)

Once a stage has a win, run a **pruned refinement** ladder to confirm (or beat) it cheaply.
Config: driver `"Reference Reward Floor": {Enabled: false, Tolerance: 0, "Solution File":
"<the win>.sol"}` (supplies the per-step reward trace, no cancel check) + engine
`"Reference Reward Prune": {Enabled: true, Tolerance: T}` + engine `"Reference Pinning"`
(hashes via `jaffar-player --dumpHashes`) with **`Bonus ≈ T`** — see gotcha 3. Every produced
state whose reward falls below the trace at its depth beyond T is dropped; the pinned
reference lineage is exempt, so a win of at-worst the reference length is always reachable.

Stage02 ladder (reference 1290): T=0 → 1290 in seconds; T=10 → 1290 in ~45 s (exhaustive
band, 1.3% of a 5 GB DB); T=5000 → 1290 in ~15 min. **No beat found** — 1290 is tight
against any line that stays within half a kill-latch of the reference at every step.
Semantics of T: 0 = never behind, ever; ~10 = micro-detours (a few px); ~5000 = latch
reordering within half a kill; bigger T needs real DB budget and approaches the unpruned
search. A tie run reproduces the reference length via a different micro-choreography
(same RNG ending), confirming the local neighborhood is flat.

**Hard-won gotchas (each cost a failed run):**

1. **Module lineage state must be deterministically initialized.** 12 serialized members
   (tracked position, bomb shadows, `_currentStep`, …) had no initializers; every runner
   instance (search workers / player / trace replay) started from different heap garbage, so
   states never byte-matched across instances and pinning could not engage at depth 1.
2. **Hashed/serialized buffers must be canonical.** `computeChainTo` now zeroes the full
   64-cell buffer before writing: the tail beyond the chain length is hashed and serialized
   (replan lineage state), and plan leftovers made hashes instance-dependent — the search's
   hashes never equaled `--dumpHashes` output.
3. **Pin dedup-bypass must be once-per-depth, and Bonus must scale with T.** Pruning
   collapses the surviving band onto the reference path, so every lineage re-creates the
   reference states each step; with an unconditional dedup bypass they flooded the DB (83%
   of 10 GB were copies of the same 1291 states — symptom: best == worst == ref exactly).
   The bypass+bonus now applies once per depth. And at Bonus 5 the trace-riding reference is
   the *floor* of the admitted band, so DB-full eviction kills it first (measured: pin died
   at depth 184, extinction at 254); Bonus ≈ T ranks it above every merely-in-band state
   while genuinely-ahead lineages still outrank it.

Debug aid: `JAFFAR_DEBUG_SAMPLE_RATE=N` samples every Nth stored state (depth, reward,
hash, player pos, RNG) and hard-checks the prune invariant on every push — zero cost unset.

### Observations that shape the campaign

- **Stage boundaries are RNG-narrow**: every stage01/02 harvest collapsed to 2–3 distinct
  RNG keys per solve-length tier — the RNG freezes at the last kill and there are few viable
  last-kill choreographies. Expect more variety once Detonator (stage 3+) frees kill timing.
- **DB size matters at the margin**: stage01 at 50 GB beat the 1 GB-era solve by 159 frames
  with the identical reward stack. Re-running a banked stage at 50 GB is cheap insurance.
- **Scorer blind spots (open TODO)**: sealed-exit pockets, enclosed powerups, enclosed/far
  enemies (tour legs need open-path-first treatment). Human-check the board map before
  committing a long search.
