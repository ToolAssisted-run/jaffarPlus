# NES Lode Runner — TAS project notes

**Status: ACTIVE 2026-08-05 — per-level replicate-then-beat campaign** (resync effort on hold
at level 9; the 8 synced level references are the working material). This file is the
session-independent state dump; `docs/` holds the digested references (tas-submissions,
ram-map-and-lua, gamefaqs-guide, strategywiki + 50 level maps).

## Per-level campaign (stage01 active)

Setup: dedicated `build-lodeRunner/` (release, `-Dgame=lodeRunner -Demulator=QuickerNES
-DdetailedProfiling=true`; the meson default emulator is QuickNES — always set `-Demulator`).
Per-level spans of the synced candidate (via `levelReport.py <movie> [maxLevels]`): L1 310,
L2 486, L3 393, L4 448, L5 284, L6 1009, L7 463, L8 400 frames (skip-press → level counter
increment; end spawn timer printed too — L1 ends st=22 matching adelikat's 22/23 assert).

**stage01**: seed = frames 0–428 (`stage01.initial.sol`, ends on the Left+Select skip press);
reference tail = `stage01.reference.sol` (320 post-seed inputs, win at step 310); config
`stage01.jaffar` (30 GB state DB — ALWAYS 30 GB per user directive; driver Reference Reward
Floor tol 10 as the behind-reference tripwire). Route: 6 chests; ref lets enemy 2 carry two of
them ((22,5) grabbed ~f440, extracted via pit-trap at f=526; (4,1) grabbed ~f616, extracted
f=720 after a ~104-frame wait at (19,3)); exit = hidden ladder col 18. The guard-extraction
paces (steps ~98 and ~292) are where searches genuinely fall behind — shaping work lives there.

### THE deep game fact: no fixed frame loop ⇒ hash-equal ≠ future-equal

The engine runs as much logic as fits each frame. The between-frame CPU capture point is
near-continuous (166 distinct PCs along the 321-step reference alone), and states identical in
ALL of RAM but captured at different points genuinely diverge later (twin test: seed+L vs
seed+D+L — RAM-identical at depth 1 except the $06 button latch, spawn timer forks by depth 12,
sub-tile position stutters at luck events). Consequences, all measured:
- Dedup keeps one arbitrary micro-phase representative per RAM-class → best-vs-reference
  wobbles ±few shaping points (the recurring −3.75 = 3 px) — irreducible at hash granularity.
- Hashing the phase kills dedup: full 96-B digest → 4M states @ step 32; even PC-only →
  7.3M @ step 20. Machinery kept but off: emulator property "Cycle Phase" (QuickerNES wrapper,
  serialized-state bytes [8,16) refreshed per advance) + module knob `Hash Cycle Phase`.
- Reference pinning is unusable here (user also vetoed it): hash-only pinning anchors twins,
  byte-verify false-rejects on worker instance residue. `Exact Verification` knob added to the
  engine's pinning block regardless.
- Therefore the floor needs tolerance ≈ shaping noise (10 = one tile of gradient); REAL
  lateness is 100k-quantized and still caught instantly. Progress-only floors via the game's
  `getFloorReward()` override are the tol-0 alternative (user prefers shaped reward + tol 10).

### Hash configuration (validated set)

Timers ON ($53/$9E — luck-manipulation correctness; per-depth spread is small), Camera ON,
Enemy Offsets ON, Player Offsets ON (never off — extinction), dig-button latch ($06 & 0xC0,
unconditional — digs are press-edge-triggered; NEVER hash the full latch: every input becomes
a distinct state, DB explodes), Sound State knob OFF (covers $B7-B9/$D0-DF/$1EA-$1FF; 4× DB
for no benefit). Serialization: NTAB/CHRR/SRAM disabled (NTAB irrelevant for this game —
tested), Precise State Timing ON (round-trip == live verified byte-exact over 321 steps —
serialization was NEVER the divergence source; the phase lives in the capture point).

### DISASSEMBLED: the player input dispatcher ($CC83) -- priority chain with fall-through

`JSR $CFB1` fetches the current player's latch ($06 P1 / $07 P2 / demo stream), then first-match:
**U ($08, climb-up) > D ($04, climb-down) > B ($40, dig-left) > A ($80, dig-right) > L ($02) >
R ($01)**. Handlers that cannot act return carry-set and FALL THROUGH to the horizontal checks
(skipping intermediate priorities: U-pressed never examines D). Consequences, all matching the
empirical probes: L+R resolves LEFT; U+D on flat ground = pure cycle-phase nudge (failed climb,
D never examined); L+A = dig-right if legal else walk-left (the reference's held-composite
economy); B+A digs left. Button bits: $80=A $40=B $20=Select $10=Start $08=U $04=D $02=L $01=R.
Poll routine $C1E1 (writes $06 at $C1F9); scroll-skip Select check in the camera pan loop at
$C9DA/$CA01; speed menu (Select+A/B, DEC/INC $E5) at $C4EA+.

### Input alphabet (from the minimized reference movie as oracle)

15 inputs: null, U, D, L, R, A, B, L+A, L+B, R+A, R+B + diagonals U+L/U+R/D+L/D+R (501 uses
movie-wide — GrabLadder combos; the original 11-input set couldn't express the reference).
Conditional prune: dig state $A0 ∈ [1,10] → null only (player locked; reference concurs across
levels 1-8). dig=11 keeps the full set (last locked frame: input takes effect on control
return). Fall-prune UNSAFE (direction chosen from a falling state acts on the landing frame —
minimizer kept such inputs). Climb-prune unsound (no on-ladder flag; U+L/U+R are where ladder
frames are won). U+D (×9 in ref) left out — first candidate if a level won't replicate.

### Reward function (stage01)

Gold banked 100k (dominates) + nearest-chest 10/tile with **Path distance** (module
`"Nearest Chest Distance Mode": "Path"`: multi-source Dijkstra from chests over the REVERSED
movement graph — one-way falls, ladder-only up, trapdoors(5) fall-through, marker(8/9)
fallback to $0400; single-entry cache keyed by map hash ⇒ ~free at 2.3 Mstates/s; unreachable
→ Manhattan fallback; `"Enemy Path Cost"` knob off) + guard-carry {Intensity 0, Distance
Factor 10} = carried chest scores like an on-map chest at the guard's tile (reward-neutral
grab, no +5k cliff) + exit point-magnet (18,0) on gold==0 or ≥250 ($93 underflows to 255
during the exit climb — gate rules accordingly).

### STAGE01 BEAT: 734 vs reference 738 (VERIFIED by power-on replay, 2026-08-06)

The advanced-start bisection ladder (`segLadder.py <Kstart> <Kstep> [minK]`: seed = reference
frames 0..K, floor vs the remaining tail at tol 0/grace 0, short-span search at 4 GB; failure
on a short span = reward/hash/alphabet lead with the frontier excluded, success moves K back)
produced wins at K=280/260/240 on its first rungs -- all converging to a level-2 flip at frame
**734 (reference: 738), a 4-frame beat found while never dropping below the reference's reward
at any step**. Verified: full power-on replay of `stage01.win734.full.sol` (seed + ref[0:260] +
the K=260 win tail; banked in this folder with `stage01.seg260.win.sol`). CAUTION when
harvesting ladder wins: rungs run sequentially and share /tmp/jaffar.winsolution.sol --
copy per rung immediately or re-run the rung to regenerate.

### BREAKTHROUGH: no-op input elimination fixes tol-0/grace-0 divergence (run52+)

User doctrine vindicated: the "irreducible ±1-frame wobble" had an exact cause. Collision
mining (scratchpad collisionMiner.py: all 15 children per ref-prefix depth, hash-equal pairs,
common-suffix causality probe, full-state diff) showed ALL causal hash collisions at depths
0-11 are HELD-DIRECTION TWINS (L vs D+L etc.): the $06 direction latch is masked from the hash
(user directive) and a held vertical on flat floor is a movement no-op that only perturbs the
cycle budget -> twin shadows the true lineage. FIX: never GENERATE the no-ops -- module property
"Vertical Input Useful" (player on/adjacent-to ladder, over ladder top, or on rope; computed in
stateUpdatePostHook) gates the input sets: predicate-false states get a 9-input alphabet (no
U/D/diagonals). Predicate validated against every reference vertical press in the win span
(0 violations). Result: grace-0/tol-0 runs sail past the old step-11 wall. NOTE the serialized
state layout: LRAM at offset 178 (not 1541!) -- earlier internal-byte attributions were wrong;
size 3595 = 178 header + 2048 LRAM + whole-Ppu + 4 module bytes.

### Floor protocol: tolerance 0, grace = measured event-wobble count (SUPERSEDED by grace 0 + root-causing)

User doctrine: tol-0 cancels are the instrument -- each one indicates a game aspect missing from
the hash or a missing alphabet input; investigate, fix, rerun. Measured so far: the ±1-frame
event-timing wobble at luck boundaries (grab/fall/trap/release) is UNHASHABLE (RAM candidates
exhausted -- the mined work-RAM byte set $26/$28/$3B/$55-$5B/$1F6 changed the cancel by exactly
0.000000; discriminant is emulator-internal, near-continuous, hashing it kills dedup). So: keep
tol 0 and ratchet Step Grace by 1 per stacked luck boundary the route has passed -- grace 1
passed step 11 (grab), grace 2 passed 67 (grab+fall), grace 3 tests the extraction at 97-99
(grab+trap+release). A cancel whose magnitude is NOT fractions-of-a-tile (or which does not sit
at an event boundary) is REAL and gets the full investigation. The driver log's "graced check
vs step N" field (added this session) shows the exact cancel comparison. Note: the display
Best-Ref is graceless; the check is graced.

### The reference's stage01 endgame is a KILL, not a trap (decoded f=515-635)

E2 is trapped at (18,13), its climb-out at f=550 is RE-TRAPPED (pit timer resets), and at f=578
the hole refills WITH E2 inside → kill ($C5 increments) → respawn at (3,1) timer 127 (top row,
respawn column = ($53+digCount)%32, phase-manipulated) → at f=595 the respawned E2 walks onto
(4,1), grabs it, and couriers it along row 1 to the player waiting at (19,3). So the last-chest
delivery is ENGINEERED: kill the courier with the right spawn-timer phase so it respawns next to
the chest. Shaping hooks: "Kill Count" property ($C5, registered); stage01 rules 1004 (+2000
persistent once killed) and 1005 (+500 keeping E2 boxed post-extraction until the refill kill).
Scaffold runs without these expire at step 321 with 5 banked (runs 35/40): trap-escapees wander
mid-map and never cross row 1, leaving (4,1) stranded.

### BEAT CANDIDATE: the reordered double-delivery route (run33, floor-killed at 247)

Free search (grace 60) found: (17,12) first at s=24, pit-trap extraction at s=84 (ref 97),
(24,10) at s=115 (ref 132), (23,3) at s=175 with E2 grabbing (4,1) simultaneously (ref 188) —
and by s=247 the MAP WAS EMPTY: 4 banked + BOTH remaining chests in guard transit (E2 timer −1
about to drop at (14,5), E3 carrying the (7,10) chest at −10), player at (16.4,5.4) near the
exit. Back-loaded banking (per-step floor can't see it) but the finish plausibly beats 310.
Solutions kept: `stage01.run33.best.sol` era (scratchpad copies run30best/run32best/run33best).
Revisit in the polish phase after the reference-order replicate lands. Also noted: the
carry-countdown drop extraction (lead the laden guard until its per-tile counter expires —
no dig needed) is a general alternative to pit-trapping.

### Diagnostic tooling added this session

`levelReport.py` (per-level spans/skip/solved/spawn-timer table); player env
`JAFFAR_ROUNDTRIP_PER_STEP=1` (serialize+deserialize between steps — search-dynamics fidelity
A/B); player env `JAFFAR_DUMP_FULLSTATE_DIR` (per-step full serialized states); engine envs
`JAFFAR_PIN_TRACE=1` (pin claim/dedup tracing) and `JAFFAR_PIN_DUMP_DIR` (pinned-state input
histories). Twin-probe recipe: two 1-input-different tails under the stage config (tail-only —
the config prepends the seed; inlining it double-seeds and strands the movie), `--dumpRam`,
diff timelines.

## Goal & assets

Resync adelikat's 49,304-frame NesHawk TAS (tasvideos 10412S, all 50 levels) to QuickerNES,
then beat it with JaffarPlus. ROM `Lode Runner (U) [!].nes` (Mapper 0, matches submission;
`"Use Flat Code Map": true`). Movie structure: 50 levels split by ~340-frame null jingle
gaps; per-level play spans 191–1100 frames.

- `fullGameNesHawk.sol` — the reference movie.
- `fullGameQuickerNESPartialResync.sol` — hand-seed (levels 1–6 + level-7 entry).
- `fullGameQuickerNES.sol.partial` — **current resync candidate: levels 1–8 synced**
  (fixes: L7 insert 6 @5107, L8 delete 1 @5924). Level 9 is the wall (see below).
- `lodeRunner.jaffar` (replay config), `resyncTool.py`, `spliceLevel.py`.
- Game module `games/nes/lodeRunner/lodeRunner.hpp`. Build: `meson configure build
  -Dgame=lodeRunner` (switch back to other games' modules the same way).
- **UNCOMMITTED at pause**: the game module, these example files, and the
  `source/player.cpp` pacing patch (reproduce mode skipped usleep+keypoll when
  `--disableRender --unattended`: full 49k replay 13+ min → 8.4 s — this is what makes
  power-on probe sweeps viable, since savestate probing is lossy).

## Key mechanics (details in docs/)

- Win = collect all gold, then climb the exit ladder that appears at gold=0. A=dig right,
  B=dig left. Guards move at half player speed, are steerable, walkable when trapped;
  respawn column = ($0053 + digCounter) % 32. No dedicated RNG — free-running counters
  $0053/$009E.
- **Select scroll-skip**: a 1-frame direction+Select window during each level's ~300-frame
  opening scroll skips it. A missed window opens the level-select menu (play mode $00DB→0)
  and strands the movie. Speed menu: title Select+A ×34 → game speed $00E5=1 (max).
- Reference plays levels 14/32/34/47 at speed 4 (skip impossible at speed 1) — prime
  improvement targets.
- RAM: player $20–$23 (tile + offset/8), alive $9A, dig $A0, gold left $93, level $A6,
  enemy timers $661+/$669+/$671+ (signed), dig slots $6A0/$6C0/$6E0, tile map $200–$567
  (hash it), $0400-layer tile bytes = chest present 7 / gone 0.

## Resync pipeline (resyncTool.py)

Replay candidate from power-on with `--dumpRam`, find the first level not *genuinely*
solved (gold==0 during play of level k — the level counter alone is fooled by menu
wandering), then probe phase-shift null edits with 12 parallel players: tier B = in-level
±1..4 nudges; tier A = whole-level delay 1..310 insert/delete in the preceding jingle gap;
tier C = both. skipLanded oracle = playMode 1 + gfx 0x18 + player displacement >0.5 tiles
by start+40 (camera stability is the WRONG oracle — post-skip camera follows the player).
The editlog auto-carries shifts into later levels.

## Level 9 — needs a jaffar re-solve (resync tiers all fail: guard AI phase diverges)

Run 10 was healthy when paused: tower chest ~step 130, best lineage at right ladder (24,5),
step ~228/1300, 30 GB DB, ~39 s/step. **Resume: `setsid build/jaffar stage09.jaffar`** from
this folder (everything is baked into the config). Best partial lineage:
`stage09.run10.best.sol` (post-seed inputs; replay under stage09.jaffar which seeds).
After a win: `spliceLevel.py 9 stage09.initial.sol <win.sol> <out>` then
`resyncTool.py <out> fullGameQuickerNES.sol`.

- Layout: castle, 4 chests — (23,4) sealed right tower (enter row-1 rope, dig roof);
  (12,7)+(15,7) entombed rows 6–9; (15,10) interior pocket by ladder (16,10). Route =
  adelikat's: tower first, then dig-weave the pair, then the pocket, exit TOP-LEFT (0,1).
- **PROVEN GRAVE: (15,7)-first** — the 2-wide pocket has no exit (side-clearance dig
  rules); a seeded probe search exhausted without escaping. Runs 8/9 died because the
  gold-count checkpoint locked onto that earliest-but-doomed lineage (the classic
  checkpoint dead-end trap).
- Config machinery in stage09.jaffar / lodeRunner.hpp: `Watch Tiles` (per-chest tile
  properties for rules); poison order-guards (fail if any non-tower chest gone while
  Tower==7, gated level==9 && playMode==1 && rem∈[1,3] — that gate range never occurs in
  the two stale-map windows: level counter increments ~4 frames before map swap, and the
  scroll streams tiles progressively); checkpoints all require Tower==0, tolerance 200;
  per-leg waypoint Add-Rewards + point/dig magnets, gold magnet 100k/chest dominates;
  exit magnet to (0,1).
- Hash knobs: Timers/Camera/Enemy Offsets false (40× win); **Player Offsets MUST stay
  true** (false = tile-representative extinction — "ran out of states" at step ~58).
  Slim 11-input set. 30 GB DB (bigger only slows steps; reward pruning does the work).
- If the resumed run stalls: per-leg mini-searches from saved lineage prefixes (the
  pocket-probe pattern is fast); check UntilDig side-clearance semantics; consider
  allowing guard-carry of the central chests as an alternative delivery.

## Progressive-reward package (trace-monotonicity study, 2026-08-06)

Goal: gold-banked credit only ~20% above best-proximity value so a stray pickup cannot derail
exploration; test = monotone reward along the full reference floor trace (JAFFAR_DUMP_REF_TRACE,
plus JAFFAR_DUMP_COMPONENTS module instrument, 3 eval lines per trace step).

Package (module + stage01.jaffar):
1. Gold Magnet 100000 -> 480 (= 1.2 x max chain-weighted next-gold swing ~400 measured on trace)
2. Rule Add-Rewards /100: 1003=10, 1004=20, 1005=5, 1006=30, 1007=20, 1008=10
3. Transit continuity: pit-trapped enemies (timer 1..125) stand in as gold sources when
   grounded+carried < gold remaining (release-window blackout guard)
4. Path fields (player d1 AND chain-hop matrix) computed on the static $0400 layout, not the
   live map: self-dug holes are transient (refill) and must not perturb distance guidance --
   this removed the -148 "dig severs the under-route to carried gold" truth-dip at step 92
5. Dig lifecycle continuity: $A0==12 registration-gap credit (dug live tile, solid layout, no
   refill slot yet -> 11), REFILL_INIT 180->182 (slot spawns at 171; 171+11 seamless handoff),
   kill bonus 181->183 (= max progression + 1)

Result: violations 19 (worst -143.5) -> 16, ALL <= 10; residual classes are structural and
benign: leg point-magnet counter-progress (-5..-6), greedy chain hop-target switches (-2..-7),
one exit-phase d1 edge (-10 @ 295). Strict monotonicity would require dropping waypoint legs /
chain lookahead; not worth it.

K=100->150 rung failed under OLD reward at both 5GB and 30GB (cancel ~step 47-49, the post-kill
shaping cliff). Retrying under the package: kill-event cliffs shrank 100x, landscape is far
smoother. stage01.g480.jaffar = the experiment config; package now also in stage01.jaffar.

## K=100 rung: retention defect found, then BRIDGED (2026-08-06)

K=100->150 failed identically at 5GB/30GB (cancel step 49 = abs 149, the kill landing) under
BOTH old and package rewards. Postmortem of the 30GB run: reference NEVER fell below worst
(0 below-worst addenda) yet its lineage died; DB was 93.1% full with 25k cumulative drops.
DIAGNOSIS: admission drops are arrival-order -- an incoming state is refused when a slab is
full even if it outranks kept states, so a ref child can be dropped while worse states survive.
This is a FOURTH divergence class (retention), beyond hash/alphabet/reward, and it breaks the
tol-0 guarantee "ref above worst => ref line retained".

Decisive experiment: same rung at 80GB (span never near fill) -> BRIDGED to 150 at tol 0.
No hash/alphabet/reward gap in the kill window. NOTE: even the 80GB run logged 50,850 drops
and bridged anyway -- drops are a lineage-survival hazard (lottery), not a frontier signal.
Proper fix (PROPOSED, not implemented -- engine shared with Bomberman session): reward-aware
admission -- when full, evict the worst kept state if the incoming outranks it.

Certified chain now: 100 -> 150 -> 200 -> 234/235/236 -> ... -> 734. Remaining: 50->100, 0->50
(both launched at 80GB under the package reward).

## FULL-LEVEL TOL-0 CERTIFICATION COMPLETE (2026-08-06)

Final rungs bridged at 80GB under the package reward: 50->100 and 0->50. The certified chain
now covers the ENTIRE 734-frame line from power-on to win at tolerance 0, grace 0:
  0 -> 50 -> 100 -> 150 -> 200 -> 234/235/236 -> ... -> 734
Every window of the reference is reproducible by search under the current hash/alphabet/reward.

Next (user-directed): full-level exploratory run, floor tol=1000, DB escalation 5GB -> 10GB ->
300GB on floor-cancel (fullEscalate.py; tests the "smooth reward = self-cleaning DB" hypothesis
-- small DB should suffice if arrival order ~ reward order).

## Full-level tol=1000 escalation: WIN 721 (2026-08-06) -- BEAT by 13 frames

5GB: floor cancel at step 148 (self-cleaning carried it deep, not to the end).
10GB: SOLUTION FOUND at step 292 -> verified by power-on replay (levelReport.py):
flip at abs frame 721 (vs our tol-0-certified 734; adelikat 738). lastIn 720, skip Y,
gold0 at 709, spawn timer at inc = 10. The progressive package searching with tol=1000
slack found a 13-frame improvement with a mere 10GB DB -- the smooth-reward self-cleaning
hypothesis materially confirmed (old reward needed 30-80GB just to REPLICATE windows).
Reference REBASED to the 721 line (stage01.reference.current.sol, 292 steps); full movie
preserved as stage01.win721.full.sol. NOTE: 721 line is NOT yet rung-certified at tol 0
(the 734 chain remains the last fully certified line).

## Boot/loading acceleration (user-detected, Bomberman NMI-timing kin) (2026-08-06)

probeBoot.py sweep (376 null frames x 8 inputs, readiness = first frame DB==1 & lvl==1 & gold==6):
- Baseline anatomy: menu presses end 90, load window 95-413, READY at 414, 13-frame input-
  swallowing hold (gt frozen), scroll auto-starts ~427, skip press 428, player moves 429.
- ONLY hits: Start at even frames 82/84/86 -> ready 407/409/411. Mechanism = earlier menu exit
  (select+A setup completes at 81; Start accepted from 82), NOT load-window NMI magic: all 319
  load-window frames x 8 inputs inert, and round-2 sweep on the boosted seed = 0 hits (no
  compounding). Ceiling: -7 frames.
- Boosted timeline (S@82): ready 407, hold 407-419 (gt frozen), scroll auto-starts 420.
  A press CANCELS the in-flight scroll at its current camX (the "skip" is scroll-cancel):
  P=421 -> camX stops at 2, player controllable from ~423 => play start -7 vs baseline.
  TRAPS: P=420 cancels at camX=0 but play deadlocks (x frozen); P=406 press swallowed ->
  gt-frozen fake state that fools skipLanded (camX=0 because NOTHING advances).
- Old tail desyncs at every shift (gt phase at play start differs by 7 -> enemy AI phase) =>
  level re-solved from the new start. stage01.initial.v2.sol = 422-frame seed (S@82 + press@421).
  Re-solve running: stage01.v2start.jaffar (floor disabled -- no aligned reference; 10GB DB).
  Target: beat 721 - 7 = 714 or better.

## Boost = board REROLL, not just -7 frames (2026-08-06)

Phase-hostility control test: old tail replayed on boost1 seed at every press frame 421-428.
ALL diverge at tail-step ~11 -- including P=428 (phase 0, the exact baseline press). So S@82
does not merely shift timing: loading 7 frames earlier samples the free-running counters
($53/$9E) at different values during level build => DIFFERENT enemy pattern (a board reroll)
at every phase. Implications:
- The scaffold rules (1003-1008 kill windows, 1010-1017 legs, 1910-1913 order poisons) encode
  the OLD board's choreography -- actively misleading on the rerolled board. v2 10GB run with
  full scaffold: 340 steps, 0 wins, banks 27-112 steps late from bank2 on.
- S@84 / S@86 would give two MORE board variants (different rerolls, -5/-3 frames).
- v2neutral run launched: scaffold dropped (kept 1000-1002 magnets/exit, fails, win), 30GB,
  360 steps. If the natural route on the rerolled board is <= 298 steps, we beat 721.

## Menu handler DECODED; boot-acceleration lead CLOSED (2026-08-07)

User caught it: v2 runs were at Game Speed 5, not 1. $E5 = speed divider, boots at 35 (0x23,
init $C027), delay loop at $C49F (LDX $E5 nested wait; 0 wraps to 256 = SLOWEST -- tested:
0.38 tiles/40f vs 11.5 at speed 1). Menu handler at $C4D3 (disassembled):
  A alone = level+1 (wrap 50->1) | B alone = level-1 | sel+A = speed-1 | sel+B = speed+1
  after speed change: spin until A/B released (AND #$C0) => 2-frame cadence is a HARD floor
  Start (AND #$10) accepted any iteration => the S@82/84/86 "hits" = mid-countdown starts
Proofs of v1-menu optimality: S@8 earliest title Start (1/4/6 dead); s@17/18 REQUIRED
(dropping delays first dec 24->30); pairs 23-89 = 34 x 2f minimum; Start@90 immediate.
NO compression exists. probeBoot "readiness" metric was speed-blind -- add $E5==1 to any
future readiness checks. v2 seeds/configs are INVALID artifacts (speed 5). 721 line stands.

## Load-window NMI-manip investigation: NEGATIVE, complete (2026-08-07)

probeLoad.py (stage A) + probeHold.py (stage B), speed-1-asserting metrics:
- Load window 91-413: NO input pattern (8 singles + 6 combos x held / alternating / per-quarter,
  plus round-1's all-frames singles) completes loading before 414. ONLY effect: a 1:1 PAUSE-gate
  -- holding impossible pads (A+B, U+D, L+R) suspends the loader frame-for-frame (81 held = 81
  late, exact), resumes on release. Input reaches the loader solely via a wait-loop; no
  cycle-drift acceleration path exists (unlike Bomberman's transition gates).
- Hold phase 414-427: the 13-frame swallow is ABSOLUTE. probeHold's 189 "hits" (press 420-422)
  were false positives: press swallowed, player frozen, the camX<=2/gt-running check was fooled
  by the auto-scroll starting 8 frames post-press (outside the sample window). Walk-test
  (held R, player x frozen) disproved all. Lesson: play-start oracles MUST assert player
  DISPLACEMENT under held input, not camera/timer proxies.
=> The entire pre-play prefix 0-429 is end-to-end tight: menu provably minimal, loader
   input-insensitive (pause only), hold absolute, press 428 earliest effective. v1 seed FINAL.

## Post-ready single-press sweep: the +1 toggle mapped (2026-08-07)

User-observed variation CONFIRMED and characterized. Stage-start (scroll-start) reachable set
is exactly {428, 429} on the v1 seed:
- ANY lone 1-frame press (all 8 buttons) at ANY load frame 96-413 => scroll 429 (+1)
- two separated presses, or a held run of >=2 frames => 428 (cancels; NOT frame-count parity)
- baseline (no presses) = 428 = the FAST edge; nothing reaches 427.
Mechanism (disassembly): $C5A1 = spin-while-input-held (the swallow); scroll advance at
$C9E6/$CA0D gated per-frame on select ($C9DA: held select freezes scroll -- the "skip" is
scroll-freeze/cancel); hold-phase select = PAUSE toggle (player+gt freeze, dead state -- NOT
a skip; probeHold's swallow finding stands). $B7 (jingle flag) clears by ~416, not the binding
hold constraint at stage start.
LEAD for full-game (stage 2+): $E163 wait-loop `sel held -> exit, else wait $B7==0` -- a
select-skippable jingle wait, likely the LEVEL-END/inter-level transition. Test when
optimizing stage transitions.
=> stage01 play start: 429 (press 428) on the fast edge, IMMOVABLE. v1 seed final, again.

## Stage02 optimal initial sequence (2026-08-07) -- PENDING USER REVIEW

stage02.initial.sol (1068 frames) = win721 + transition, every element bracketed:
  721 flip | 722-722 lag (K=2) | select@723 (celebration skip -- polled ONLY at 723, next
  chance 731, 8-frame cadence; unpressed = waits forever, B7=1) | 724-727 dead (1 normal +
  4 lag) | Start@728 (724-727 ineffective; ready scales +1/frame with later start) |
  unskippable load 729-1052, NMI-IMMUNE (lone-A samples at 740/800/900/1000/1045 + pair:
  ready 1053 / scroll 1067 invariant; not even stage01's +1 toggle) | ready@1053 (8 chests,
  player (10,12)) | hold 1053-1066 (presses 1058-1065 swallowed; 1066 = dead-pause trap,
  same as stage01's P=420) | L+s@1067 cancels scroll at camX=2, walk-verified dX=3.25 |
  PLAY from 1068.
Transition cost: 721 -> 1068 = 347 frames (vs stage01's 429 boot incl. menu).
NMI probe was SAMPLED not exhaustive -- exhaustive sweep available if wanted.

## Mixed-strategy reward: reference-following vs exploratory (2026-08-07)

USER PROTOCOL: two disambiguated reward families with a mix weight w_ref, per level starting
at w_ref=0.9-1.0 (follow adelikat) and relaxing toward exploratory after an initial match /
small wins.
- REFERENCE family (x w_ref): in-order prefix credit rules (chests 1..k of the ref tile-order
  all collected -> +w*480 each, cumulative; off-order grabs beyond the prefix earn nothing)
  + watch-tile legs (magnet to first uncollected chest in ref order; intensity 15 so a leg
  handoff swing (~15*hop) never exceeds the +480 pickup credit) + exit leg (50).
- EXPLORATORY family (x (1-w)): any-order banked (Gold Magnet 480->48) + any-gold chain
  (Nearest Chest 10->1, carried weight 0.5 inside).
- NEUTRAL: dig lifecycle + kill bonus (serve both).
NOTE: the "reference order" is the TILE-clearing order, which includes ENEMY grabs (stage02:
chests (2,3)@1107 and (8,7)@1120 taken by enemies pre-first-player-pickup). Crediting them is
intentional: enemy grabs are deterministic given the player line, so matching them = matching
the choreography. stage02.jaffar = w0.9 instance; derivation currently manual (scriptize when
stabilizing: legs + prefix rules from a reference replay's RamDump).

## Full capture-choreography tracking (2026-08-07)

USER SPEC: match gold captures in the same order AND same way as the reference. Key finding:
all 8 stage02 reference pickups are FLOOR pickups by $C4 (the true per-level pickup counter --
$92 is NOT it), but two are at DROP SITES of enemy-carried gold (e.g. chest (2,3) grabbed by
enemy 3 @1107, collected at (1,3) @1189). Capture order != grab/tile-clearing order; the old
tile-prefix tracked grabs (superseded).
Mechanics decoded: carry timers ($671+) tick UP toward 0 (spontaneous drop at 0); trapping
overwrites the timer with the trap counter (cargo invisible); a trapped carrier deposits its
gold on the tile ABOVE the hole (head-riding = instant catch, counted in $C4).
Implementation: config "Reference Pickup Order" [[x,y]..]; advanceStateImpl detects $C4
increments and matches player tile vs next expected event; sticky "Choreo Violated" latch
freezes reference-family gold credit; _choreoMatched/_choreoViolated SERIALIZED lineage
members (bomberman-style) + hashed; properties "Choreo Matched"/"Choreo Violated"; legs
2010-2017 gate on Matched==k & !Violated -> magnet at next CAPTURE position (incl. drop
sites). Breakdown: "Choreo pickups: N/8 matched [VIOLATED] [player picked M]".
Validated: ref replay walks 0->8 matched, 0 violations, 0 fails, win@485.
Also this session: $C4 discovery (RAM-differenced), surrender term now $C4-based (dormant
safety net), predictive fall-doom trap rule 1996 (engine-validated on ref + control).

## Transition alignment study + repair attempt: FAILED; campaign on ALIGNED seed (2026-08-07)

Question: cost of making the reference tail replay. Straight aligned movie seed = 20 frames
(17 their-slower-stage01 + 3 transition). Cheaper-alignment findings:
- $53 (spawn timer) FREEZES at the stage01 flip and seeds stage02's AI; cycles fast (~mod 28,
  2-3/frame) during play => k=3 nulls before our last stage01 input lands $53=22 (=ref) for
  3 frames (flip 724). k sweep: only k=3 hits 22; several k break the win.
- Celebration select poll is on a GLOBAL 8-frame grid (not flip-relative): k=3 forces sel@734.
- $9E diverges via phase-dependent freeze windows; exact (22,192) pair achievable (st@752,
  press 1090 = 23 frames, worse than aligned).
- Enemy SPAWNS identical at ready in all variants -- divergence is ongoing-AI, and even the
  exact counter pair DESYNCS the tail (3 pickups then breaks): deeper history-dependence
  (cycle phase / other work RAM), consistent with the no-fixed-frame-loop lore.
- repairTail.py hill-climb (insert-null/insert-copy at EVERY tail position + null deletes,
  2086 probes/round): ZERO improving single edits, twice. The post-pickup-3 divergence is a
  discrete AI branch, not absorbable lag drift.
=> CAMPAIGN NOW ON THE ALIGNED SEED (stage02.reference.initial.sol, play 1088): floor tol 0
grace 0 vs stage02.reference.sol (validated: 0 fails, win@485), Path mode restored, w0.1,
legs 15, trap rule, repulsion 5/r5, 5GB. The 20 frames (17 stage01 + 3 transition) are
RECOVERABLE LATER by laddering the transition window from our fast seed once the level line
is certified. Stage01's 17-frame win is banked in stage01.win721.full.sol regardless.

## Alignment endgame: transition levers PROVEN dead; SFX door negative at depth 1 (2026-08-07)

Pause/stall probes (106): NO differential lever exists post-flip -- music ($D8-$DA) and spawn
work ($55-$5B) are baked BEFORE the transition. Arithmetic: $53 has period 14 in flip-delay
(2/frame mod 28) => flip in {724, 738} only; music period does not divide 14 => full alignment
via delays forces flip=738 (surrender all won frames). Endgame SFX-rotation probes (486: one
extra A/B/L/R/U/D at each null frame 640-720, k=3 kept): ZERO residue movement with flip/$53
held. Full alignment: no path found; deeper endgame re-choreography would be the only remaining
angle (jaffar state-matching search -- unproven odds).
ACTIVE: HYBRID -- stage02.initial.hybrid.sol (1298f) = win721 + k3 transition + ref choreo
through pickup 3 (runs 9-11f compressed on our phase; pickups 1164/1179/1296). Search solves
remaining 5 pickups from (1,10), w0.5, legs on capture positions, Path, 30GB, floor off.
All 17 stage01 frames + fast transition kept (play 1078 vs aligned 1088).

## fullGameNesHawk.sol VERIFIED on the NesHawk C++ translation (2026-08-07)

Added NROM support to quickerNesHawk (was hardcoded UNROM-128KB): NROM-128 PRG = UxROM
degenerate case (prg_mask=0 mirrors the 16KB bank); CHR ROM loaded into the pattern space
with a chrIsRom write-protect flag; header-driven cart setup (mapper 0), CHR preserved across
BoardSystemHardReset. Build: build-nesHawk (-Demulator=NesHawk -Dgame=lodeRunner); config
needs QuickerNES-specific emu keys stripped (scratchpad lodeRunnerNH.jaffar).
VERIFIED: the 49,304-frame movie replays boot -> stage50 COMPLETE in full sync:
- all 49 level->level+1 transitions at sensible frames (lvl1@737, lvl2@1573 = exact QuickerNES
  resync agreement on the early levels);
- stage50: play from ~48760 (14 gold), gold 0 at 49286 (exit-phase 255 @49290), player climbs
  to the (0,0)-area exit by the final frame.
This doubles as the NesHawk translation's first FULL-GAME sync validation (49k frames,
CPU+PPU+APU+input), far beyond the M1/M2 fuzz milestones.

## CROSS-EMULATOR TRANSPLANT PIPELINE (2026-08-07) -- THE alignment killer

USER PLAN, fully validated: NesHawk state -> QuickerNES -> solve fast -> verify back.
1. Transplant = full-LRAM poke (2048 bytes from the NesHawk RamDump frame) onto a QuickerNES
   donor state captured at ANY hold frame (donor: aligned-movie stage02 hold @1080). The game
   is LRAM-complete; PPU content is cosmetic. Player: --pokeRAM + NEW --savePokedState[Step].
2. VERIFIED: adelikat's stage50 tail replays FRAME-IDENTICALLY in QuickerNES from the
   transplanted state (all 15 gold events match the NesHawk original exactly; press phase
   pad=6 is the one working alignment -- same 1-in-N phase behavior as everywhere).
3. Anchor for search = state advanced 7 steps post-poke (past the L+s scroll-cancel, which is
   not in the search alphabet -- the tol-0 floor caught this at step 7, working as designed).
   Saver config MUST mirror the campaign config's emulator options (Precise State Timing etc.)
   or state sizes mismatch (10648 vs 11498).
4. stage50.jaffar: Initial State stage50.transplant.state, floor tol0/grace0 vs
   stage50.reference.sol (543 lines; validated 0 fails, win@527), win rule = level counter
   WRAPS TO 1 (no 51), initial gold 14, choreography 14 pickups extracted from the replay,
   legs + trap rule + w0.9. RUNNING at 5GB.
Implication: any level of adelikat's chain is now directly searchable at QuickerNES speed
with exact-history fidelity + NesHawk ground-truth verification path back. Also: NesHawk core
got NROM support; the 49,304-frame full-game movie syncs on it end to end.

## Transplant rendering fixed; root cause = NTAB not serialized (2026-08-07)

The "garbage background" chain, fully root-caused with a NesHawk ground-truth render:
- CIRAM extraction (NesHawk state offset 2104, right after RAM at 56) was CORRECT: empirical
  calibration at the same game moment (stage02 hold 1080 on both emulators) shows IDENTITY
  mapping, QuickerNES nt_ram pages 0-1 == NesHawk CIRAM pages 0-1 exactly. Pages 2-3 of the
  4KB nt_ram are NOT mirrors -- never touch them (doubling CIRAM into them breaks rendering).
- The REAL bug: the config lineage carries 'Nametable Block Size': 0 (+ NTAB in Disabled
  State Properties), so savestates silently DROP nametables: every Initial-State load started
  with stale PPU garbage no matter what was poked pre-save.
- Fix: TWO transplant states: lean stage50.transplant.state (search; gameplay is LRAM-complete)
  and stage50.transplant.render.state (Nametable Block Size 4096, NTAB enabled; 15594B) with
  scratchpad stage50render.jaffar for all rendering/screenshot/video flows. Verified pixel-wise
  vs the NesHawk ground truth at the same frame.
- Also fixed: NesHawk wrapper saveScreenshot now self-refreshes from ppu->xbuf (the player's
  headless screenshot pass never calls updateRendererState; QuickerNES self-refreshes too).
Tooling added along the way: --pokeNTAB, --dumpNTAB, --savePokedState[Step] (player).

## STAGE50 BEAT by 2 frames -- GROUND-TRUTH VERIFIED (2026-08-07)

Trace magnet broke the endgame: ladder v5 K=450 WON at 5GB (rung step 75 vs ref 77); verified
in QuickerNES (wrap 525 vs 527, last input 524 vs 526) AND transplanted back to NesHawk:
fullGameNesHawk[:48777] + our 525-step line -> gold cleared 49289, LEVEL-WRAP AT 49302 = exactly
2 frames ahead of the published movie's completion, on the same core the TAS was made for.
Spliced full-game movie: fullGameNesHawk.beat2.sol. THE TRANSPLANT PIPELINE IS PROVEN END TO
END (NesHawk state -> QuickerNES -> search -> solution back to NesHawk, frame-exact).
Ladder: 450 WIN / 400,350 bridged @5GB; K=300 failed both sizes -> root cause = REFERENCE USES
L+A (dig-left composite) which stage01 removed as redundant -- stage50 refutes that; all four
dig composites restored (sets 11/17); ladder v6 resuming from K=300.
K=450-era diagnostics that got us here: euclidean leg wall-parking (fixed: path-aware choreo
leg), then ref-detour-vs-field-optimum wobble (fixed: step-indexed trace magnet, intensity 10,
per-rung auto-generated traces).

## The K=150 wall: full detective arc -> $A2 dig-work byte (2026-08-08)

Symptom: K=150 rung failed at 5/30/260GB across five configs; cancels scattered (K=160@abs172,
K=165@abs194, K=170@abs186); outcomes flipped with ANY hash-stream change (even repartitioning
identical bytes) -- a "lottery".
Eliminated in sequence: admission drops (260GB 8% fill 0 drops), sibling hash collisions
(mining x2 clean), fail rules (trace state-type clean), input legality (all R/null, no locks),
floor-vs-search reward divergence (same function). LRAM bisection produced phantom $7F ($7F is
constant 0 game-wide): a bisection driven by stochastic outcomes ALWAYS converges somewhere.
Controls (only-$7F fail / complement bridge) + partition test ([0,0x80) as 1 range bridges, as
8 chunks fails) proved value-sensitivity, not content-sensitivity.
NEW INSTRUMENT: driver JAFFAR_FLOOR_AUDIT=1 -- per-step best-vs-reference byte diff under the
engine's volatile-residue mask (+ cancel-time deep dump). Result: follower is best through
step 2; a ONE-BYTE deviant (offset 314 = LRAM $A2, next to the $A0 dig counter) TIES the
reward exactly from step 3; deviants lead by step 8; follower extinct by 12.
ROOT CAUSE: $A2 (dig work byte) is causal but unhashed => follower and $A2-deviant HASH-MERGE
(cross-parent twin, invisible to sibling mining); the race winner is decided by thread arrival
=> hash-value lottery. Bisection missed it because $A2 is in [0x80,0x100) and one lucky bridge
of [0,0x80) steered the search into the wrong half.
Cycle-phase hash SHOULD have fixed it and did separate the twins -- but it also disabled dedup
broadly (per-instance audio-ring residue in the digest), bloating 260GB to 91.5% + drops: the
follower died in the admission lottery instead. Its verdict was invalid, not exculpatory.
FIX: Hash LRAM Ranges [[0xA0,0xA8]] (dig work area), cycle-phase OFF. Ladder v11 rerunning.
LESSON: bisection/ddmin REQUIRE determinism -- always run identical-config repeats FIRST.

## SESSION CHECKPOINT (2026-08-08, pre-compaction)

STATE: box idle, all runs stopped by user.
- stage50 tol-0 ladder: CERTIFIED 150 -> win(525). K=150 accepted by USER DECISION (the audited
  260GB run was deep past all cancel points when stopped; outcomes in that window are provably
  run-to-run NONDETERMINISTIC -- engine-level race, see the K=150 arc above). PENDING: K=100,
  K=50, K=0. Ladder policy: tiers 5/30/260GB per rung; treat failed rungs as possibly-bad draws
  (retry-tolerant). After full green: the 260GB all-in full-span solve (stage50.full300.jaffar
  exists; floor tol 0 vs the rebased 525 reference per user instruction).
- NEW ENGINE FEATURE "Hash Lookahead" (Engine Configuration, default 0; shown in the log's Hash
  Database block): hash the state after N null advances instead of hashing transient bits.
  Zero-serialize design: dup candidates pay only N advances (discarded in advanced posture --
  the caller reloads the base per candidate); survivors rebuilt via base-reload + re-advance.
  Initial-state site uses a self-restoring scratch path. Game hook: getNullInputIndex()
  (game.hpp virtual + lodeRunner override).
- stage50.jaffar is in the LEAN LOOKAHEAD regime: Hash Lookahead 1, "Hash Input Latch": false
  (new game knob, default true -- gates latch bits + facing), Hash LRAM Ranges [].
  stage01.jaffar untouched (latch/facing hashing still on, lookahead 0).
- LEAN REGIME FIRST DATA (v15 watcher final report, post-stop): K=100 @5GB floor-cancel at
  step 16; @30GB floor-cancel at step 56; 260GB tier killed by user stop before finishing.
  So Hash Lookahead=1 is FUNCTIONALLY VALIDATED (clean runs to genuine floor-cancel, no
  crash), but K=100 resists at small tiers -- same early-floor family K=150 showed before the
  surgical hash adds. Throughput/DB-size comparison vs old regime still unmeasured. Next
  session: retry K=100 (nondeterminism policy) and/or run the 260GB tier.
- Ladder runs: outcome one-liners in scratchpad s50ladder*.log; per-rung logs
  stage50.s50.<K>.<dbMb>.log; JAFFAR_FLOOR_AUDIT=1 gives per-step best-vs-ref byte diffs.
- ALL SOURCE CHANGES UNCOMMITTED (module, engine, driver, player, quickerNesHawk NROM+video).
  Commit only when user asks.

## T-BACK CAMPAIGN (2026-08-08, user policy)
Mission accomplished (2f beat banked); now extending the lead. Strategy: launch from T-N
(seed reference minus last N frames, win-oriented), recursing:
- WIN (strict beat, total < ref length): verify spliced movie replays to win, rebase
  stage50.reference.sol (preserve old as .prevNNN.sol), then T-(N+50) @30GB.
- FAIL @30GB (floor-cancel / exhaustion / equal-length win): same T @260GB.
- FAIL @260GB: STOP, report to user (no further autonomous escalation).
Fixed regime (user-specified, do NOT change unilaterally): 30GB base tier, Hash LRAM Ranges
[[160,169]] ($A0-$A8), Hash Input Latch false (also gates facing bit), Hash Cycle Phase false,
Hash Lookahead 0 (user chose over lookahead=1), floor tol 0 / grace 0 + below-worst cancel.
Driver: tback.py <backFrames> <dbMb> (wraps stage50Ladder.runK win-oriented). Started at T-100.
Caveat noted: PICKS frame indices in stage50Ladder.py are old-reference-relative; trimming uses
f > K so it stays correct while accumulated beats are small -- recheck if a pickup nears a K
boundary on deep rungs.

## FORWARD (INVERSE) LADDER -- section-by-section campaign (2026-08-10, ACTIVE)
Reference: stage50.win519.sol (519 inputs from stage50 anchor = full-movie frame 48777;
wrap@519 verified BOTH cores; full prefix preserved as stage50.fullPrefix48777.sol).
Folder purged to essentials (282 files + 168 logs deleted).
DESIGN (user): solve sections forward from level start; win points = very definite events;
strict route conformance -- module latches "Route Violated" on ANY tile change that doesn't
advance the sectioned route odometer (waypoints w/ gold gates from the 519 RAM trace);
rule 2005 fails violators. Alphabet per section: dig inputs stripped unless the section digs
(519 dig starts: frames 207/283/315). After each section win: splice, use the 519's equivalent
section as pacing (Max Steps), next section starts from the spliced state. Enemy-phase drift
between branches accepted (riskier, faster).
MODULE FEATURES (all in lodeRunner.hpp, built): Route Waypoints File "x y g" (g=required C4),
Route Waypoint Reward (30), high-water sub-tile proximity (monotone, verified 0 violations),
Route Progress/Route Violated/Gold At Watch Tile/Enemy Drop Done properties; Enemy Drop Watch
[x,y]. Reward doctrine: route odometer ONLY (no exploratory magnets), monotonicity verified
via JAFFAR_DUMP_REF_TRACE before每 launch.
SECTION 1 (sec1.jaffar, RUNNING): start=stage50.newstart.state, win = Route Progress>=43
(route ends ON the first fall-entry tile, frame-118 ref) + Gold Collected>=2 + Gold At Watch
Tile==1 (the enemy's VOLUNTARY deposit at (11,7), ref frame ~107; enemy stands (10,7), gold
lands (11,7) -- layout $4CF==7). Ref section time 118. No floor/prune/pins; lookahead 1;
Max Steps 128; A/B stripped from alphabet.
519 section facts: picks (0,12)@14 (9,7)@81 (12,3)@125 (15,3)@135...; enemy1 pickup@5 (4,9)
drop@106-107 gold->(11,7); enemy2 pickup@22 drop@75 (22,10); enemy3 pickup@68 drop@344 (22,13).
Render fast path: NH anchor state stage50.nh48777.state + nhFast.tmp.jaffar (seconds).
