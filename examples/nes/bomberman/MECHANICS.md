# NES Bomberman — game mechanics reference

Distilled from: StrategyWiki (main/Gameplay/Walkthrough pages), Daniel Engel's GameFAQs guide
(faq 14622, 2017 rev.), Data Crystal RAM map, and TASVideos' RNG page. Compiled 2026-07-26 as
the knowledge base for the TAS campaign. Facts below are source-documented; anything tagged
**(verify)** must be confirmed against the emulator before we rely on it in rewards/rules.

## Game structure

- Hudson Soft, 1985 (Famicom) / 1987 (NES). 50 stages, single player. Bomberman is a robot
  climbing 50 floors of an underground bomb factory to reach the surface and become human
  (the character is the Lode Runner robot; the ending references this).
- Every stage: a single horizontally-scrolling maze (larger than one screen), hard concrete
  walls in a fixed lattice (odd/odd tile positions) plus destructible soft bricks.
- **Stage objective (both required, in order):** (1) kill every enemy, (2) step on the exit
  door tile. The exit is hidden under one soft brick and only becomes active once all enemies
  are dead. Walking over the exit is enough ("you can walk through the exit").
- **Layout is randomly generated per visit** — only the enemy roster and which power-up type
  the stage holds are fixed per stage number. Soft-brick placement, the exit brick, and the
  power-up brick all come from the RNG (see below). This makes RNG manipulation the single
  biggest TAS lever in the game.
- Bomberman always starts in the **upper-left corner** of the maze; the immediate right and
  down tiles are always open (standard Bomberman spawn pocket) **(verify)**.
- **Timer: 200 seconds** per normal stage (RAM default 0xC8 = 200). On time-out (and if the
  exit door is bombed) enemies pour out of the exit — time-outs spawn Pontans (the fastest
  enemy). Bombing the revealed power-up likewise spawns extra hard enemies (penalty).
- **Bonus stages** after stages 5, 10, 15, 20, 25, 30, 35, **39**, 44, 49 (the 8th comes one
  stage early). 30-second timer, invincible (nothing can kill you), unlimited spawns of one
  fixed enemy type, pure score farming; no exit to find (auto-advance on time-out) **(verify
  auto-advance)**.
- Lives: start with 2 in reserve (RAM default 02) **(verify semantics: 02 = 3 total?)**.
  Death by enemy touch or any explosion (own bombs included).
- Passwords (20 letters) encode stage + power-up state; the guide's per-stage list grants MAX
  fire, MAX bombs, and Speed — useful for manual scouting, irrelevant for a from-power-on TAS.
- After the ending, the game restarts with all power-ups retained.

## Controls / input set

- D-pad: 4 cardinal directions (grid-corridor movement; sub-tile alignment auto-corrects
  around corners **(verify cornering behavior)**).
- A: drop a bomb on the tile Bomberman currently occupies.
- B: with Detonator only — detonates the **oldest** live bomb.
- Start: pause. Select: menu only (title cursor: $00 = Start, $01 = Continue).
- TAS input set: null, U, D, L, R, A, B, direction+A, direction+B (movement is 4-way; test
  whether opposing/diagonal combinations do anything useful before allowing them).

## Bombs

- Fuse: self-detonates "a couple of seconds" after placement — exact frame count TBD from RE
  **(verify; classically ~2.5 s)**.
- Explosion: cross shape, reach = flame range in each of the 4 directions; blocked by hard
  walls; destroys the first soft brick in each arm (arm stops there); kills enemies and
  Bomberman (unless Flamepass); detonates other bombs it touches → chain reactions.
- Chained bombs inherit "explode now", enabling long daisy-chains; chains are the tool for
  the Famicom bonus item (248+ bombs in one chain).
- A dropped bomb is solid: it blocks movement (walk-through requires Bombpass). Beware
  self-trapping — the classic TAS death.
- Wall-embedded enemies (wallpass types inside bricks) die if the brick itself is bombed.
- Bombing the exit door or the power-up spawns waves of harder enemies (score resource,
  time penalty otherwise).

## Power-ups (one hidden per stage, under a soft brick)

| Power-up | Effect | Kept on death? |
|---|---|---|
| Bombs | +1 simultaneous bomb (start 1, max 10) | yes (permanent) |
| Flames | +1 explosion range each direction (start 1, max 5) | yes (permanent) |
| Speed | Move twice as fast; only exists in stage 4 | yes* |
| Wallpass | Walk through soft bricks | no |
| Detonator | B detonates oldest bomb (no more fuse waiting) | no |
| Bombpass | Walk through bombs | no |
| Flamepass | Immune to explosions (permanent while alive) | no |
| Mystery | Full invincibility ~35 s (enemies + explosions) | n/a (timed) |

\* Sources disagree slightly: StrategyWiki says only Bombs/Flames survive death; the FAQ says
you lose exactly Wallpass/Detonator/Bombpass/Flamepass (i.e. Speed also survives). **(verify)**

- Power-up per stage (see the full table below). Detonator appears in 12 stages; Speed only
  in stage 4.

## Enemies (StrategyWiki names; FAQ aliases in parentheses)

| Enemy | Points | Speed | AI | Wallpass | First..Last stage |
|---|---|---|---|---|---|
| Balloom (Valcom) | 100 | 2 slow | 1 low | no | 1..27 |
| Oneal (O'Neal) | 200 | 3 normal | 2 normal | no | 2..32 |
| Doll (Dahl) | 400 | 3 normal | 1 low | no | 3..41 |
| Minvo | 800 | 4 fast | 2 normal | no | 4..44 |
| Kondoria (FAQ "Ovape") | 1000 | 1 slowest | 3 high | YES | 7..50 |
| Ovapi (FAQ "Doria") | 2000 | 2 slow | 2 normal | YES | 10..50 |
| Pass | 4000 | 4 fast | 3 high | no | 14..50 |
| Pontan | 8000 | 4 fast | 3 high | YES | 48..50 (and all time-outs) |

- "Smart" enemies chase Bomberman and hold the trail; dumb ones bounce back and forth,
  changing direction rarely (RNG-driven — see below).
- Chained enemy kills in one blast multiply point values (bonus-stage scoring note).

## Bonus (hidden score) items — one specific item per stage, absurd conditions

| Item | Points | Condition |
|---|---|---|
| Bonus Target | 10,000 | reveal exit and pass over it before killing any enemy |
| Goddess Mask | 20,000 | after killing all enemies, walk the stage's full outer ring |
| Cola Bottle | 30,000 | reveal+cross exit with 0 kills, then hold that direction 15 s |
| Famicom | 500,000 | after killing all enemies, chain-detonate >248 bombs (no Detonator) |
| Nakamoto-san | 10,000,000 | kill all enemies without destroying a single brick |
| Dezeniman-san | 20,000,000 | 0 kills, destroy every brick, bomb exit 3×, dodge spawns |

Irrelevant for a pure-speed TAS; listed for completeness (score-attack routes).

## Stage arrangements (enemy counts / power-up / bonus item)

Roster per stage (B=Balloom, O=Oneal, D=Doll, M=Minvo, K=Kondoria, V=Ovapi, P=Pass, T=Pontan):

| St | Enemies | Total | Power-up | Bonus item |
|---|---|---|---|---|
| 1 | 6B | 6 | Flames | Goddess Mask |
| 2 | 3B 3O | 6 | Bombs | Nakamoto-san |
| 3 | 2B 2O 2D | 6 | Detonator | Famicom |
| 4 | 1B 1O 2D 2M | 6 | Speed | Cola Bottle |
| 5 | 4O 3D | 7 | Bombs | Dezeniman-san |
| A | ∞ Balloom | — | bonus stage | |
| 6 | 2O 3D 2M | 7 | Bombs | Bonus Target |
| 7 | 2O 3D 2K | 7 | Flames | Goddess Mask |
| 8 | 1O 2D 4M | 7 | Detonator | Bonus Target |
| 9 | 1O 1D 4M 1K | 7 | Bombpass | Goddess Mask |
| 10 | 1O 1D 1M 3K 1V | 7 | Wallpass | Nakamoto-san |
| B | ∞ Oneal | — | bonus stage | |
| 11 | 1O 2D 3M 1K 1V | 8 | Bombs | Famicom |
| 12 | 1O 1D 1M 4K 1V | 8 | Bombs | Cola Bottle |
| 13 | 3D 3M 3K | 9 | Detonator | Dezeniman-san |
| 14 | 7K 1P | 8 | Bombpass | Bonus Target |
| 15 | 1D 3M 3K 1P | 8 | Flames | Goddess Mask |
| C | ∞ Doll | — | bonus stage | |
| 16 | 3M 4K 1P | 8 | Wallpass | Bonus Target |
| 17 | 5D 2K 1P | 8 | Bombs | Goddess Mask |
| 18 | 3B 3O 2P | 8 | Bombpass | Nakamoto-san |
| 19 | 1B 1O 3D 1V 2P | 8 | Bombs | Famicom |
| 20 | 1O 1D 1M 2K 1V 2P | 8 | Detonator | Cola Bottle |
| D | ∞ Minvo | — | bonus stage | |
| 21 | 4K 3V 2P | 9 | Bombpass | Dezeniman-san |
| 22 | 4D 3M 1K 1P | 9 | Detonator | Bonus Target |
| 23 | 2D 2M 2K 2V 1P | 9 | Bombs | Goddess Mask |
| 24 | 1D 1M 4K 2V 1P | 9 | Detonator | Bonus Target |
| 25 | 2O 1D 1M 2K 2V 1P | 9 | Bombpass | Goddess Mask |
| E | ∞ Kondoria | — | bonus stage | |
| 26 | 1B 1O 1D 1M 2K 1V 1P | 8 | Mystery | Nakamoto-san |
| 27 | 1B 1O 5K 1V 1P | 9 | Flames | Famicom |
| 28 | 1O 3D 3M 1K 1P | 9 | Bombs | Cola Bottle |
| 29 | 2K 5V 2P | 9 | Detonator | Dezeniman-san |
| 30 | 3D 2M 1K 2V 1P | 9 | Flamepass | Bonus Target |
| F | ∞ Ovapi | — | bonus stage | |
| 31 | 2O 2D 2M 2K 2V | 10 | Wallpass | Goddess Mask |
| 32 | 1O 1D 3M 4K 1P | 10 | Bombs | Bonus Target |
| 33 | 2D 2M 3K 1V 2P | 10 | Detonator | Goddess Mask |
| 34 | 2D 3M 3K 2P | 10 | Mystery | Nakamoto-san |
| 35 | 2D 1M 3K 1V 2P | 10 | Bombpass | Famicom |
| G | ∞ Pass | — | bonus stage | |
| 36 | 2D 2M 3K 3P | 10 | Flamepass | Cola Bottle |
| 37 | 2D 1M 3K 1V 3P | 10 | Detonator | Dezeniman-san |
| 38 | 2D 2M 3K 3P | 10 | Flames | Bonus Target |
| 39 | 1D 1M 2K 2V 4P | 10 | Wallpass | Goddess Mask |
| H | ∞ Pontan | — | bonus stage (one early!) | |
| 40 | 1D 2M 3K 4P | 10 | Mystery | Bonus Target |
| 41 | 1D 1M 3K 1V 4P | 10 | Detonator | Goddess Mask |
| 42 | 1M 3K 1V 5P | 10 | Wallpass | Nakamoto-san |
| 43 | 1M 2K 1V 6P | 10 | Bombpass | Famicom |
| 44 | 1M 2K 1V 6P | 10 | Detonator | Cola Bottle |
| I | ∞ Pontan | — | bonus stage | |
| 45 | 2K 2V 6P | 10 | Mystery | Dezeniman-san |
| 46 | 2K 2V 6P | 10 | Wallpass | Bonus Target |
| 47 | 2K 2V 6P | 10 | Bombpass | Goddess Mask |
| 48 | 2K 1V 6P 1T | 10 | Detonator | Bonus Target |
| 49 | 1K 2V 6P 1T | 10 | Flamepass | Goddess Mask |
| J | ∞ Pontan | — | bonus stage | |
| 50 | 1K 2V 5P 2T | 10 | Mystery | Nakamoto-san |

(The StrategyWiki and GameFAQs tables agree on every stage.)

## RAM map (Data Crystal — "stub", partial; all to be verified and extended by our own RE)

| Address | Meaning |
|---|---|
| $0010 | Buttons pressed (bitfield) |
| $0020 | Bomberman Y coordinate |
| $0028 | Bomberman X coordinate |
| $002C | Bomberman animation frame |
| $002D | Bomberman horizontal flip |
| $0054-$0057 | **RNG state** RandomCtrl1-4 (from TASVideos asm: `lda $54` etc.) |
| $0058 | Level number (01-0x32) |
| $005F | Title screen cursor ($00 Start / $01 Continue) |
| $0068 | Lives (default 02) |
| $0073 | Explosion radius (high nybble) |
| $0074 | Max simultaneous bombs |
| $0077 | Remote detonation flag (>=1 true) |
| $0093 | Time (default 0xC8 = 200) |

Everything else (enemy table, bomb table, per-tile map, exit/power-up location, scroll) is
unmapped — first RE task of the campaign.

## RNG (TASVideos, verbatim asm at ROM $D668)

State: 4 zero-page bytes $54 (RandomCtrl1), $55 (RandomCtrl2), $56 (RandomCtrl3),
$57 (RandomCtrl4). Called **on demand** whenever the game needs a random number, plus
**once per frame on the title screen** (this is the pre-game manipulation lever).

**v2 decode corrections/extensions (2026-08-04, verified against our ROM disasm):** the
routine entry as we trace it is **$D670** (TASVideos' $D668 label is off by 8). All 7 RNG
callers accounted for: (1) title screen +1/frame ($C38B), (2) board generator (below),
(3) enemy AI walk decisions ($D365: duration from {32..128} + heading — only LIVE enemies
draw, so **the stream freezes at the last kill** and post-clear inputs cannot reseed the
next board), (4) penalty/timeout spawners ($D6D5).

### Board generator ($CA4B) — fully decoded

One RNG stream at the stage-start frame draws, IN ORDER: (1) the exit brick cell, (2) the
powerup brick cell, (3) 50+2×level soft bricks, (4) per enemy: position then initial
heading (AND #3 + 1). Every cell placement goes through the $CACF pick-random-empty-cell
helper: col = AND #$1F clamped 1–31, row = AND #$0F clamped 1–11, **rerolls** on occupied
cells and on the spawn pocket (col<3 && row<3). The rerolls make seed→board chaotic: you
cannot predict a board from the RNG state — generate and dump it (replay ~1 s).
`scoreBoards.py` extracts a board from a `--dumpRam` pass (`extract()`, offset past earlier
stages) and plan-scores it (`score()`).

```asm
PermutateRandomCtrlVars:
$D668  lda RandomCtrl1   ; $54
$D66A  rol a
$D66B  rol a
$D66C  eor #$41
$D66E  rol a
$D66F  rol a
$D670  eor #$93
$D672  adc RandomCtrl2   ; $55
$D674  sta RandomCtrl1
$D676  rol a
$D677  rol a
$D678  eor #$12
$D67A  rol a
$D67B  rol a
$D67C  adc RandomCtrl3   ; $56
$D67E  sta RandomCtrl2
$D680  adc RandomCtrl1
$D682  inc RandomCtrl3   ; $56 increments every call
$D684  bne +
$D686  pha
$D687  lda RandomCtrl4   ; on $56 wrap, $57 += $1D
$D689  clc
$D68A  adc #$1D
$D68C  sta RandomCtrl4
$D68E  pla
+$D68F eor RandomCtrl4   ; output = result ^ $57
$D691  rts
```

Callers often post-permute the result further (e.g. `ror a` twice). $56 is effectively a
call counter, so the sequence never short-cycles; $57 changes only every 256 calls.

**TAS implications:**
- Stage layout (soft bricks, exit brick, power-up brick) is RNG-generated at stage load, so
  the number of title-screen frames before pressing Start — and every in-stage RNG
  consumption before the next load — steers all subsequent layouts. Prime manipulation
  targets: exit brick adjacent to the spawn pocket, minimal bricks between spawn and exit.
- Enemy direction decisions presumably consume RNG in-stage → our own movement/bomb timing
  perturbs enemy AI and the next stage's layout. (Which events consume RNG is undocumented;
  we must instrument call counts per frame ourselves.)
- The routine is cheap to model in C++ (TASVideos gives an equivalent); we can predict
  layouts ahead of time once we know the per-load call pattern.

## TAS strategy notes (initial, to refine after RE)

1. **Speed goal per stage:** kill all N enemies + touch exit. Time is dominated by (a) where
   the exit brick spawns, (b) how fast the roster can be herded into blasts, (c) walk speed
   (2x after stage 4's Speed — grab it), (d) fuse waits unless/until Detonator (stage 3).
2. **Detonator (stage 3) transforms routing**: no more ~2.5 s fuse waits; bombs pop the frame
   B is pressed (post-placement constraints TBD). Expect every post-3 stage's optimal to be
   Detonator-driven. Dying loses it — deaths are near-certainly route-fatal.
3. **RNG manipulation is the campaign's core**: like SMB frame rules but richer — waiting K
   frames pre-Start (or burning RNG calls in-stage) re-rolls the next layout. The search can
   legitimately trade a few frames of waiting for a drastically better layout draw.
4. Wallpass/Bombpass/Flamepass change movement topology (walk through bricks/bombs, stand in
   blasts); Flamepass enables suicide-adjacent tactics (stand in own blast) at zero risk.
5. Enemy AI: "smart" enemies path toward Bomberman — they can be lured into blast tiles;
   dumb ones need corridor traps or must be met on their track.
6. Bonus stages are pure time-loss for speed (fixed 30 s? — **verify** whether they can be
   ended early; if not, they're constant-time and out of scope).
7. Timer pressure is irrelevant for TAS pace (200 s/stage vs. expected <60 s optima).

## Reverse-engineering findings (2026-07-26, verified on QuickerNES via RAM dumps + controlled input experiments)

### Tile map — THE core structure
- **$0200, 13 rows x 31 cols, row stride 32 bytes** (last byte of each row padding).
- Tile codes: `0` empty, `1` concrete, `2` soft brick, `3` bomb, **`4` brick hiding the EXIT,
  `5` brick hiding the POWER-UP** (both identifiable the moment the map is built — no bombing
  needed to locate them!), `6` revealed power-up (0 once collected), `8` revealed exit door
  (persists), `$11-$14` explosion transients (1 frame).
- The map is hashed in its entirety by the game module (dedup correctness).

### Input gating — every 4th frame is inert (UNLESS the Speed stat is held)
- Global frame counter `$33` (+1 every frame during play).
- **On frames where `$33 % 4 == 0` ALL input is ignored** — a 1-frame direction tap does
  nothing and a 1-frame A tap does NOT drop a bomb (verified). On the other 3 phases input
  takes effect the SAME frame with zero latency (movement +1 px, bomb appears in map).
- **The Speed power-up bypasses the gate entirely** (found 2026-08-04, ROM `$CCA4: LDA $75 /
  BNE process-input`): with `$75 != 0` (stage-4 pickup, carried for the rest of a deathless
  run — set at frame 6254 of the v1 movie) input processes EVERY frame. That is the skates'
  entire mechanic: 1 px per processed frame → 4/4 px instead of 3/4 px per 4 frames.
  Verified: a phase-0 direction press moves the player in stage 5+, is swallowed in stages 1–4.
- Search consequence: stages 1–4 only, phase-0 frames need just the null input (25% of the
  tree pruned); **stages 5–50 + bonuses B–J have NO inert frames** — the module's `Input
  Frame` bool implements exactly this (`$75 != 0 || (($33+1) & 3) != 0`). v1 wrongly applied
  the phase gate to all stages: its solves from stage 5 on held null every 4th frame (the
  player stopped walking 1 frame in 4 — skates never actually exploited).
- Related input facts (ROM `$CF87`): gameplay input = `$12 | $13` — **controller 2 mirrors
  controller 1 in play** (but title/menu/transition waits read `$12` only). NMI read routine
  `$C19D` double-reads and zeroes both on mismatch (DMC-glitch filter).

### Movement
- Player position (TRUE, alias-free block): tile `$28`/`$2A` (X/Y) + pixel-in-tile `$29`/`$2B`
  (X/Y, 0-15, tile center = 8). World px = tile*16 + pixel. Spawn = tile (1,1) centered.
  Verified against bomb-placement ground truth (bombs drop at the player's true tile) and
  continuity audits over full-run RAM dumps: zero tile-jumps > 1, zero world-jumps > 2 px.
  NOTE: `$20` is shared map-access scratch that merely MIRRORS tile Y while the player is the
  last map reader; it was our tile-Y source until 2026-07-27 and caused phantom positions.
- Base speed: 1 px per input frame = **0.75 px/frame ≈ 21.3 frames per tile**.
- CAVEAT: `$20/$28/$29/$2A` double as the engine's map-access scratch — on frames where a bomb
  is placed/explodes they can briefly alias to that event's tile. Controlled runs show them
  tracking the player reliably during movement; corroborate with the map when precision matters.
- Only ~16 low-RAM bytes differ between two states with the player in different places (tiny
  state divergence — excellent dedup behavior).

### Bombs
- Placement writes code `3` at the player's tile the same frame A registers.
- **Fuse: 159 frames** (~2.65 s) from placement to explosion; explosion transient in the map
  lasts 1 frame (codes `$11-$14`); brick destruction resolves then (2 -> 0, 4 -> 8, 5 -> 6).
- Explosions also revealed: bombing 4/5 converts them (4 -> 8 exit revealed, 5 -> 6 power-up).

### Enemy table (decoded, 10-slot structure-of-arrays)
- `$584+i` tile X, `$58E+i` pixel-in-tile X, `$598+i` tile Y, `$5A2+i` pixel-in-tile Y
  (world px = tile*16 + pixel, same convention as the player).
- `$5AC+i` state: small values (1/2/3 anim cycle) = alive, `32` = dying (kill registered,
  `$9C` decrements), `44` = dead (slot inert, position frozen). Empty slots = 0.
- `$5D4+i` direction. `$5E8+i` synchronized per-wave countdown.
- Balloom speed: exactly 0.5 px/frame (moves 1 px every 2nd frame, NOT gated by the $33%4
  input gate). Player base 0.75 px/f outruns them.
- Module exposes aggregates: `Closest Enemy Distance`, `Sum Enemy Distance` (alive only) and
  a `Set Closest Enemy Magnet` rule action.

### RNG onset + Start-delay layout sweep (stage 1)
- From power-on, the RNG at $54-$57 is called **once per frame starting at frame ~11**
  ($56 = call counter). Every frame of Start delay therefore re-rolls the whole stage:
  bricks, exit brick, power-up brick AND enemy spawn positions.
- Sweep of 120 Start delays (k = 0..119 extra frames): **109 distinct layouts**. Some
  consecutive k share enemy spawns/most bricks with only e.g. the exit moved (partial
  re-roll) — useful for fine-tuning a good seed.
- Ranking by delay + NN kill-tour (BFS with brick penalty) + exit leg: best early candidate
  k=31 (exit (11,8), compact left-center enemy cluster). Exit-adjacent-to-spawn seeds exist
  (k=102: exit (3,1); k=40: exit (1,5)) but score worse on total tour with spread enemies.
- GOTCHA that burned the first sweep: `stage01.jaffar` auto-applies
  `stage01.initial.sol` as Initial Sequence — replaying full-boot sols through that config
  double-applies inputs. Use a config with an empty Initial Sequence for boot-relative
  experiments (`boot.jaffar` pattern).

### Other confirmed addresses
- **`$9C` = enemies remaining** (6 at stage-1 start, decrements per kill; 0 = exit armed).
- `$58` level (1-based), `$68` lives (2 at start), `$93` timer seconds (199 in play, ~1/s),
  `$73` explosion radius (high nybble), `$74` max bombs (0 at start — apparently 0-based),
  `$77` detonator flag, `$54-$57` RNG (matches the TASVideos asm; long stretches with zero
  consumption in-stage when nothing needs randomness).
- Enemy table: lives somewhere in `$0580-$05FF` (per-frame churn while enemies walk); layout
  not yet decoded. `$0600` page = PPU update queue, `$0700` = OAM shadow.
- Stage flow: title -> Start -> ~200-frame stage card -> map built (rows streamed over ~50
  frames) -> enemies spawn -> timer set to 200 and play begins. `stage01.initial.sol`
  (195 frames) ends right at map-built.

### Search setup notes
- Win rule: `Level == 2` (the game increments $58 on stage clear).
- Fail rule: `Lives < 2` (any death is route-fatal).
- Reward: `Set Enemies Left Magnet` (dominant, e.g. 3000/enemy) + `Set Exit Magnet` (Manhattan
  world-px distance to the exit tile, auto-located from the map).
- Smoke search (256 threads, 1GB state DB): ~3.2 Mstates/s, 77% duplicate rate.

### Win / fail signals (verified on the QuickerNES casual playthrough + scripted self-bomb death)
- **`$5E` (mirrored at `$9F`) flips 0->1 the moment the stage is cleared** (walking onto the
  active exit) — long before `$58` (level) increments. This is the win signal.
- **`$5C` flips 0->1 the moment the dying sequence starts** (lethal contact) and stays 0 for an
  entire clean playthrough. This is the fail signal (fires ~125 frames before gameplay stops;
  `$68` lives decrements only later).
- `$0B` = gameplay-active flag (1 during play, drops after clear/death resolution).
- IMPORTANT pairing note: `stage01.casualPlay.sol` contains NO Start press — it must be
  replayed with `stage01.initial.sol` as the Initial Sequence (i.e. through `stage01.jaffar`).
  Replayed standalone, the title ignores it and the ATTRACT DEMO plays instead (the demo
  self-triggers ~frame 2058 from power-on, plays stage 1 with maxed stats — radius 4-5, 10
  bombs, detonator — and is fully input-independent; earlier "late-Start stat leak"
  observations were this demo being mistaken for a real game).

### Reward machinery (v2, implemented in bomberman.hpp)
- **Path distances** (`Path Distance To Enemy/Powerup/Exit`): Dial-bucket Dijkstra over the
  tile map per frame; concrete impassable, bricks AND bombs cost +8 tiles (128 px) on top of a
  16 px step; multi-source from alive enemies for the enemy field; sub-tile smoothing via
  min-over-neighbors (tile distance + pixel walk). Breaking a route-blocking brick drops these
  sharply — path-consequential bricks reward themselves precisely.
- **Pickup ladder**: `Powerup Progress` = (flames-1)+bombCap+detonator (game-latched stats;
  the stat bumps the same frame the map's power-up cell clears, so a destroyed power-up can
  never count as grabbed).
- **Bomb pending term**: per live bomb, fuse progress (elapsed/160) x (threatened bricks,
  threatened alive enemies, threatened revealed exit/power-up as hazard) with per-component
  intensities — smooth per-frame reward growth across the fuse horizon (temporal continuity),
  collapsing into the realized discrete gains at detonation (observed valley <= 500 for <= 4
  frames before the +3000 kill step lands).
- Stage-1 config (`stage01.jaffar`): kills x3000 + pickup x3000 + pending (20/500/500);
  closest-enemy 1.0 while `Enemies Left > 0`; power-up 0.5 while `Flame Count < 2`; exit 0.5
  once `Flame Count >= 2`; fail on `Dying > 0` and on power-up-destroyed; win on
  `Game End Status == 1 && Flame Count >= 2`.
- Validated on the winning casual replay: monotone +3000 ladder at all 6 kills + grab, no
  pathological cliffs. Search throughput with the three Dijkstras: ~1.9 Mstates/s @256t
  (vs 3.2 without — optimization headroom: early-exit, compute only active fields).
- Controls unfreeze ~42 frames after the initial sequence ends (spawn freeze window).

### Serialization & input-alphabet decisions (validated 2026-07-26)
- **LRAM-only serialization adopted** (`Disabled State Properties` = all blocks except LRAM):
  state 2048+45 bytes vs ~11.5KB full (~5x DB capacity). Validated by the strongest available
  test: a search's best lineage (thousands of in-memory serialize/deserialize round trips
  across lineages) replays FROM SCRATCH with rewards matching exactly to the fraction —
  identical to full-state searches. All in-game states share the boot warm-up, so in-game
  cross-contamination of the non-LRAM residue is gameplay-neutral.
- **WARNING — cold state-file loads are NOT continuation-faithful** for this game: loading a
  full-block savestate (even with `Precise State Timing: true`) and continuing diverges from
  the live run at frame 1 (gameplay-real divergence; enemy AI/RNG fork). An un-restored
  warm-up variable in the QuickerNES core is suspected. Consequence: NEVER seed segment
  searches from state files here — seed with input-sequence prefixes (concatenated .sols),
  which are exact.
- **Input alphabet pruned to {null, U, D, L, R, A}** (plus B for detonator stages later):
  diagonals were believed positionally redundant. **OVERTURNED 2026-08-04 — that proof only
  held for aligned mid-corridor positions.** Within the ±3px cornering-assist window of a
  passable junction, a single perpendicular direction already moves BOTH axes (2px/frame);
  a diagonal adds the parallel handler's pixel on top (3px/frame while misaligned), reaches
  positions no held single direction ever occupies, and wins net frames: exhaustive BFS to
  crossroad (3,3) from the stage05 arm = 71 steps with {null,U,D,L,R} vs **69 steps with
  diagonals added** — the optimal line HOLDS D+R through the corridor and corrects with DL.
  Opposing pairs are also not no-ops: U+D (or adding L+R to a vertical push) moves 1px then
  FREEZES the player (a hover 1px off-center that null cannot hold, since null clears
  $A6/$7B); pure L+R horizontally cancels. v2 searches must include the 4 diagonals (and
  optionally UD/LR) in the alphabet. A-only bombing costs ~1 frame per bomb vs direction+A;
  final-polish passes can re-widen the alphabet around a found route.
- **A at bomb capacity is NOT a no-op — it is a SPEED TOOL (mechanism found 2026-08-04)**:
  every frame A is down with the player's current tile EMPTY, the bomb handler runs the
  align-to-center routines FIRST (`$CD01: JSR $CE10 / JSR $CE1F`, before the free-slot check):
  `$CE10` nudges pixel-in-tile X (`$29`) 1px toward 8, `$CE1F` same for Y (`$2B`) — both axes,
  unconditionally. Consequences (all verified on the stage05 arm):
  - **Walking toward a tile center with empty reserve: movement + align stack = 2px/frame**
    (offsets 1,3,5,7 observed). First half of every tile at double speed → up to ~25% faster
    corridor walking (4+8 frames per 16px tile instead of 16).
  - **At/past the center, held A stalls movement dead** (align −1 cancels the direction +1;
    verified: descent pinned at offset 8 indefinitely). The exploit is therefore *hold A for
    the approach half, release at center* — a per-tile A rhythm, which the search will time
    exactly.
  - Standing still with A held drifts 1px/frame toward the current tile's center (both axes).
  - Suppressed while standing on a non-empty tile (own bomb, brick residue): the handler bails
    on the tile check before aligning. With reserve available, the same press places a bomb
    (align still applies on that frame — the pre-placement snap).
  - `$74` is not "max bombs" but max slot INDEX: the placement loop checks slots `0..$74`,
    so capacity = `$74 + 1` (stage05's `$74=1` = 2 simultaneous bombs).
- Frame phases: the aligned-at-spawn diagonal test landed on an inert phase-0 frame (all
  inputs equal there) — any single-frame input probe must guarantee an active phase (burst
  4 frames or compute the phase).

### Corridor-parity input pruning (implemented; all pruned classes byte-exact-proven)
- Lattice geometry: concrete at (even row, even col). Cell classes: (odd,odd) = crossroad
  (4-way), (odd row, even col) = horizontal-corridor cell (U/D pillar-blocked at ANY pixel
  offset), (even row, odd col) = vertical-corridor cell (L/R blocked). Border rows/cols kill
  the outward direction at edge crossroads.
- Proofs: E1 = U/D at a centered corridor cell is a byte-exact no-op (not even facing bytes);
  E2 = 16-frame U-hold 4 px short of a junction moves nothing -> no cornering assist, turn
  window < +-4 px, so tile-parity stays valid while straddling tiles.
- Implementation: config supplies only {null, A} (+ null-only phase-0 set); the module's
  `getAdditionalAllowedInputs` adds the legal directions per parity/border of the tracked
  player tile, self-gated on `Input Frame`. GOTCHA: dynamically supplied inputs MUST also
  appear in some config input set (a never-satisfiable one works: Level==255) or the runner's
  `_inputStringMap` lacks them and `printInfo`/solution stringification crash (map::at).
- **Tracked player position (HISTORICAL)**: the aliasing saga came from mis-mapping tile Y to
  the `$20` scratch (see Movement above). The true block `$28/$29/$2A/$2B` never aliases; the
  continuity-filter shadow remains in the module as an inert safety only.
- Walking into a brick stops at pixel 2 of the adjacent tile (NOT center) -> brick-adjacent
  direction pruning ("v2") needs per-direction stop-limit calibration before it can be added.
- Branching on active frames: crossroads 6, corridor cells 4, edge cells 3-5; short smoke
  reaches better reward than the flat 6-input alphabet at equal steps and the best lineage
  still replays exactly.

### State-corruption incident — ROOT CAUSE: LRAM-only serialization is lossy at depth (RESOLVED 2026-07-26)
- Symptom: physically impossible frontier states (all enemies gone, bricks intact, no kills in
  the history; scratch replays don't reproduce them). With a narrow dedup hash it struck
  deterministically at step ~187-191; with the full-LRAM hash at ~step 900. NOT an engine bug.
- **Root cause (identified by the user): serializing only LRAM drops the ~152 bytes of
  CPU/PPU/APU/timing/controller/mapper context (TIME, CPUR, PPUR, APUR, CTRL, MAPR).** That
  context is genuinely consequential: explosions cause lag frames, after which lineages carry
  divergent hidden frame-alignment; LRAM-only state transfer then blends one lineage's RAM
  with another's context and the game logic misfires (enemy despawns etc.).
- Why the earlier "exact to the fraction" validations passed: they all ran below ~250 steps —
  the FIRST bomb explosions resolve at step ~187+ (placement ~27 + 160 fuse), so no lag/context
  divergence existed yet and LRAM-only transfer was coincidentally lossless. That is also why
  the narrow-hash corruption fired exactly at step ~187: first explosions.
- FIX: `Disabled State Properties` = ["SPRT","NTAB","CHRR","SRAM"] only (keep TIME, CPUR, PPUR,
  APUR, CTRL, MAPR serialized). Hot state 2250B. With the context restored, the bottom-up
  narrow hash no longer corrupts either (canary silent) — the hash only modulated exposure.
- Validation depth lesson: any serialization-fidelity claim must be validated PAST the first
  explosion horizon (>= step ~250), ideally >= 1000 steps, plus a scratch-replay audit of the
  best lineage at depth. The production config keeps a tripwire rule (Label 6666).
- The per-frame canary methodology (win rule on an impossible predicate + End On First Win)
  localized the trigger step and produced a 1-minute reproducer — keep it in the toolbox.

### Other fixes from the same session
- **Enemy aliveness**: `$5AC+i` state 0 is part of the walking anim cycle (positions keep
  moving while it reads 0). Alive = `state < 32 && tileX != 0`; 32 dying, 44 dead-frozen,
  unused slots have tileX 0. The old `st != 0` predicate silently dropped living enemies
  from the closest-enemy magnet/pending terms on anim-phase-0 frames.
- **A-gating**: A allowed only when a bomb slot is free (active bombs <= $74) — adapts to
  capacity growth automatically. A refused A-press auto-centers the player (replicable by
  direction holds), so nothing positional is lost.
- Enemy-contact deaths confirmed to latch `$5C` (fail rule covers both death types).
- jaffar-player MUST run from the examples dir (config ROM path is relative; from another
  cwd it aborts with a core dump that looks like a crash).

## Open questions for the RE phase

- Exact bomb fuse length in frames; explosion animation length; whether the exit can be
  entered on the same frame the last enemy dies.
- Movement: pixels/frame base and with Speed; sub-tile alignment rules; corner cut behavior.
- What consumes RNG each frame/event (instrument $D668 calls); layout-generation algorithm
  (map $54-$57 state at stage load → brick/exit/power-up placement).
- Full RAM map: enemy position/state table, bomb table, tile map, camera scroll, stage-phase
  byte (intro/play/dying/transition), score digits.
- Stage-clear sequence timing: is there a frame-rule-like quantization on transitions?
- Whether death/game-over resets or reseeds RNG (classic backup strategy in TASes).
