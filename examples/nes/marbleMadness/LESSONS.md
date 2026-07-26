# NES Marble Madness — TAS improvement campaigns: lessons learned

Two full campaigns were run against the published full-game TAS (`fullGame.sol`, 9908 frames):
a strict-adherence campaign (reference pinning + phase-space progress magnet) and a free
waypoint-box campaign. Both concluded **no beat found (search-limited)** on stage 6, and the
frame-rule analysis below explains why — and where any future improvement must come from.

## Game timing architecture (measured, not theorized)

- **The stage clock `$0A` resets to `$12` at every stage start** and freezes during transitions.
  Every stage's hazard/platform schedule is stage-anchored and identical on every playthrough.
  Savings in one stage never desync the next stage's internals.
- **The blink-platform phase system (`$9B`, 24 phases, dwell/burst on the `$0A & 0x1F` grid,
  128-frame cycle) only runs in stage 6** (`$9B == 255` elsewhere). The coarse frame rule that
  voids upstream savings is exclusive to the Ultimate race.
- **Transitions snap the next stage's start to a 16-frame slot grid** (Marble Madness' analog of
  SMB's 21-frame rule). The win frame itself does not snap; the transition stretches/shrinks.
  The time-bonus tally is a fixed 51 ticks — finish time does not change tally length.
- **Stage 6's wait-zone chute** (x≈612, y 668→711) is forced-descent (~1 px/frame, braking
  ineffective, entry = commitment). The floor below y=684 only materializes at the stage-relative
  f792 phase tick; the reference steps on it 4 frames later. Upstream leads are quantized away:
  we banked a **+106-frame lead** to that point and the schedule voided all of it. Pausing cannot
  open gates earlier: pause freezes `$9B` while `$0A` runs, so it only delays the lattice
  (useful to HOLD a platform, never to summon one).

## Per-stage verdict table

| Stage | Win slack to its 16f slot | Frames to save for one slot | Verdict |
|---|---|---|---|
| 1 | ~9-11 | ~5-7 | **TIE** — exhaustive pinned re-search: 1.25M wins, none earlier than ref |
| 2 | 0 | 16 | untested (three clock-gated waits; 164f free tail) |
| 3 | 1 | 15 | untested (one clock-gated wait; ~314f free tail) |
| 4 | 0 | 16 | untested (late waits are reactive, not clocked; ~575f tail) |
| 5 | 1 | 15 | search aborted at step 214/745 (wrap-up), inconclusive |
| 6 | — (internal 32/128 lattice) | — | **no beat** — two independent methodologies, ties to the frame |

Savings upstream of a stage's clock-gates are absorbed by the gate wait (departure times are
absolute — verified by parked-marble delay insertion). Only post-last-gate tail savings pay
toward the slot deadline, and gains are all-or-nothing per 16-frame slot.

## Probe methodologies that worked

- **Parked-wait insertion**: insert N null frames where the reference is stopped (upstream line
  unchanged); if the marble's departure frame is absolute-constant under N, the wait is
  clock-gated; if it shifts ~N, it is reactive choreography. Missing a gate window in stages 2-5
  costs only ~8 frames (short-period hazards); stage 6's lattice costs 32-128.
- **Delayed-win tail-nulling**: nullify the last K steering inputs so the marble coasts in
  slower; sweep K and watch the next stage's start snap between slots → measures the slot grid
  (16) and each stage's exact slack. A 1-frame input shift anywhere else kills every stage
  (line fragility), so only these two non-invasive perturbations give clean measurements.
- **Full-game RAM timeline** (`jaffar-player --dumpRam` on `fullGame.sol`, 0x800 bytes/frame):
  `$8B` edges segment stages; `$0A` freeze/run maps the clock; per-byte periodicity and
  autocorrelation locate hazard cycles.

## Search machinery lessons (implemented in `marbleMadness.hpp` / `driver.hpp` / `engine.hpp`)

1. **Waypoint Magnet** (free box-chase): a sparse chain of arrival boxes at course pivots,
   path-tracked (`_wpNext` serialized + hashed) because course position is not monotone.
   Per-box rewards must be auto-sized to `intensity × (leg + box radius) + margin` so every
   collection is a net reward increase — a flat box reward breaks monotonicity at long legs.
   Boxes need Z bands (ledges alias in x/y) and must encode the corridor: every stall we hit
   (upper ledge, funnel south shelf, x=612 rail) was the straight-line pull crossing impassable
   geometry, fixed by inserting a guide box.
2. **Dedup-correctness law**: every reward or fail input that depends on *path history* must be
   hashed, or RAM-identical states shadow each other nondeterministically and the reference
   lineage dies (tolerance-0 floors then cancel on subpixel residues). The ratchet (min-ever
   distance) violates this — use `Ratchet: false` (live distance) for segments whose reference
   approaches targets monotonically; the void-fall persistence counter is hashed.
3. **Reference lineage protection**: with any reward whose local geometry dislikes the
   reference's waits, the ref gets evicted within steps (observed at step 8). Reference Pinning
   (hash prefilter + byte-exact verify, bonus ≥ the reward deficit) is mandatory for
   tolerance-0 floors. Pair with `Below Worst Margin` = pin bonus.
4. **Floor semantics for jumpy rewards**: `Tolerance` in reward units cancels at every box
   boundary regardless of pace. Use the driver's `Step Grace` (compare against ref reward G
   steps earlier) to bound slack in TIME; set `Tolerance` to the reference's own max G-step
   wiggle (measured from `--dumpReward`).
5. **Funnel detection**: `$7B == 1` alone is NOT the funnel — it blips on tile seams and ledge
   events (farmable by a latched frame reward). The genuine funnel hold is `$7B == 1 && $DE != 0`
   (countdown 48→0, fixed length, unique in the stage). The funnel slide is `$7B == 255` ending
   in a hard teleport to the exit point.
6. **Early fail fences** beat late death detection: the void-fall persistence fence
   (`$7B==1 && $DE==0` for ≥8 consecutive frames; death latch `$80==9` fires at 24) and the
   measured absolute-time chute fence each stopped doomed cohorts from dominating the frontier
   and evicting viable waiters.
7. **Mandatory gates before any launch**: (a) reference replay through the exact config must be
   reward-monotone (`--dumpReward` + check), (b) a scripted forced-input smoke run must reach
   the win. Both caught real defects repeatedly (frame-term re-fire, `$82`-based fence killing
   the reference at the funnel transition frames).

## Where a future improvement could come from

- Stages 2-5 slot hunting: 15-16 frames each from post-last-gate tails and ~8-frame gate-window
  flips; stage 5's ~740f tail is the largest unexplored space (search was aborted, not tied).
- Schedule-origin attack on stage 6: anything that shifts the stage-entry `$0A/$9B` init by
  frames mod 32 would move the whole gate lattice against the achievable approach (needs
  disassembly of stage init, not search).
- Glitch/wrong-warp hunting — outside both campaigns' scope.
