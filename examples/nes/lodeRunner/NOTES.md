# NES Lode Runner — TAS project notes

**Status: PAUSED 2026-08-04** (pivoted to the Bomberman v2 campaign). This file is the
session-independent state dump; `docs/` holds the digested references (tas-submissions,
ram-map-and-lua, gamefaqs-guide, strategywiki + 50 level maps).

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
