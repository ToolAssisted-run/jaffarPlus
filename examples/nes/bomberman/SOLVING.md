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
