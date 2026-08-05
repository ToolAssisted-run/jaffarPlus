# NES Lode Runner — TASVideos Submission Digest

Distilled from the two adelikat submissions plus the linked RAM map and Lua scripting
framework. Organized by topic (not by source page).

Sources:
- Submission #7195 (first published TAS): https://tasvideos.org/7195S — published as [4559M]
- Submission #10412 (current TAS): https://tasvideos.org/10412S — published as [7115M]
- RAM map (BizHawk .wch, by adelikat): https://tasvideos.org/UserFiles/Info/639133364566427500
- Lua framework + per-level scripts: https://github.com/adelikat/tas/tree/master/LodeRunner/Lua
- "New tech discovered" credit video (select skip): https://www.youtube.com/watch?v=KVQ-umbmyTw

## Headline numbers

| | #7195 (old) | #10412 (current) |
|---|---|---|
| Time | 17:41.29 | 13:40.382 |
| Frame count | 63,782 | 49,304 |
| Framerate | 60.0988138974405 | 60.0988138974405 |
| Emulator | BizHawk 2.6.2 | BizHawk 2.11.1 |
| ROM | Lode Runner (U) [!].nes (site lists (U) [i]) | same; SHA1 `ea09aec11efc4b42ebc644ea243c2284001fe34f` |
| Rerecords | 41,723 | 261,124 |
| Start | power-on | power-on |
| Author | adelikat | adelikat |

- #7195 was itself a 2,038-frame improvement over adelikat's cancelled #7154; several
  tricks/improvements came from a Takanawa .fm2.
- #10412 improves the publication of #7195 by **14,478 frames (4:00.903)**. The bulk of
  that comes from the newly discovered **select scroll-skip** (~300 frames x 50 levels),
  plus new routes and better spawn/gold-drop manipulation.
- **No per-level frame table is given in either submission** — only the totals above.
- Both movies play all **50 levels in order** (no level-order manipulation; level select
  is used only as a menu trick, never to skip a level's gameplay).

## Game speed setting (0x00E5)

- The game has a hidden speed setting, changed in the level-select menu:
  **Select+A = faster, Select+B = slower**. Value visible only at RAM `0x00E5`.
  Speed mode **1 = max speed**; the TAS runs at mode 1. Mode 4 is used temporarily in
  four levels (see select skip below). In the Lua bot, changing 1→4 is three
  Select+B presses interleaved with plain Select presses (and 4→1 three Select+A).
- Both TASes aim for fastest completion at the fastest speed setting.
- **Digging speed does not scale with game speed** — digging is just as slow at max
  speed, so "many strategies involve going significantly out of the way to avoid
  digging, since it is almost always faster." A regular-speed TAS would dig far more.

## Level select / end-of-level skip (old TAS) and select scroll-skip (new TAS)

- **End-level animation skip (#7195):** pressing Select at any time opens level select.
  Select is pressed only after a level completion (first input frame after leaving the
  screen enough to cause level-end lag frames), skipping the end-level animation:
  ~10 s per level across 49 levels, without altering gameplay. The menu is then
  confirmed with Start.
- **Select scroll-skip (#10412, the big one):** each level begins with a ~300-frame
  scrolling sequence. Pressing **Select for no more than 2 frames at the right time
  during the level scroll skips the scroll entirely**. (The tech was first found as
  "select then start during the scroll"; adelikat found plain Select for 1 frame
  suffices.)
  - At normal speed the window is easy (any time during scroll, <=2-frame press).
  - At **max speed it is a 1-frame window and highly dependent on random factors** —
    most frames do not work.
  - **Levels 14, 32, 34 and 47: the skip is impossible at speed 1.** Workaround:
    switch to speed mode 4 (in the previous level's select menu), get the skip, and
    play that level slower — loses time in-level but is a net save in all 4 levels.
  - Detection oracle used by the bot: after the 1-frame Select(+direction) press,
    wait 12 frames; skip succeeded if game mode `0x00DB == 1` and the player's pixel X
    (`levelX*16 + xTileOffset`) has changed / camera X (`0x0004`) has stopped moving.
  - The skip interacts with spawn-timer luck (below): the bot harvests as many distinct
    working skip frames as possible so a spawn-timer value good for the level's strat
    can be chosen; there is a constant trade-off "time lost waiting for ideal luck vs
    what the better luck saves."

## "RNG": the spawn timer (0x0053)

- **There is no RNG in this game.** All "randomness" is the spawn timer at `0x0053`:
  increments 0→27 then wraps to 0, representing an **X tile column** in the level
  (levels are 28 tiles wide).
- **Enemy respawn:** when an enemy dies, `0x0053` is read and used to place the enemy
  at the top of the screen at that X column. If the enemy cannot spawn there, the
  position moves **1 tile to the right** until an acceptable spawn column is found.
- **Gold drops:** when an enemy picks up gold, the current `0x0053` value is stored
  (per-enemy at `0x0671–0x0673`) as **the number of tiles the enemy will walk before
  dropping the gold**. The per-enemy timer is signed; negative = enemy is holding gold
  (Lua: `hasGold = timer < 0`).
- At normal speed the timer increments once per frame. **At max speed it increments
  anywhere from 0 to 5 per frame** depending on how much game logic ran — chaos.
- Manipulation is essential for optimal strats in most levels. Examples: Level 1 and
  Level 11 manipulate enemies to carry gold into favorable positions; the Level 1
  script asserts the level must end with spawn timer 22 or 23 to set up Level 2.
- In the old TAS, manipulation could only be done by delaying completion of previous
  levels — sometimes costing more than it saved, so some small savers were excluded.
  In the new TAS, the choice of select-skip frame doubles as the manipulation lever.

## Lag model

- **Old-TAS description (#7195):** lots of lag everywhere, including level begin/end.
  Frames are deliberately delayed to dodge lag at level end or during the opening
  scroll. Random lag makes "step-suboptimal" routes globally faster. Biggest lag
  source: **enemy route calculation, especially when a ladder is in the enemy's path
  to the player**. Even lag-only levels vary a lot in completion time.
- **Refined description (#10412):** "There is no lag in this game, in the normal
  sense. The lag counter never moves." The game has no strict frame loop; it executes
  as much logic as fits in a frame. At max speed it can't keep up, producing
  seemingly random slowdown, and — crucially — **the player, the enemies, and the
  spawn timer can all lag independently**. Certain strategies depend on precise
  player/enemy phase alignment to slip past an enemy.
- Consequences for optimization: adding a single frame of delay in a random place can
  yield big gains, but naively inserting a frame desyncs everything after it (it
  changes the frame counts of every subsequent action). Unnecessary input can itself
  cause extra lag — and sometimes that extra lag is *beneficial*.
- (The Lua framework nonetheless uses BizHawk `emu.islagged()` to detect the
  level-end lag frames and "next input frame" boundaries — input-poll-based lag
  detection still fires even though the classic lag counter doesn't.)

## Movement / collision tricks

- **Walking on enemies' heads:** you can walk and even climb on enemies. Normally
  fatal, but with frame precision you can walk across an enemy while both are moving.
  Used sparingly in the old TAS; **used much more in the new TAS for shortcuts**.
  Special case: let an enemy fall through a fake (trapdoor) floor and walk over its
  head at the precise moment to cross the fake floor — saves significant time in
  **Level 18**.
- **Climbing through enemies (ladder phase-through):** hit detection on ladders is
  weird. With precise Y position and timing you can stand still on a ladder, let an
  enemy pass through you, then start climbing without being touched — even climbing
  up through an enemy coming down at you. Level- and ladder-dependent (needs a
  suitable spot). Most prominent in **Level 8**; used in a number of levels.
  Considered theoretically RTA-viable but bad risk/reward.
- **Erasing gold:** if an enemy falls into a dug pit, it releases its gold *above*
  itself. If the square above is itself a dug hole, the gold cannot be placed and
  **the gold simply ceases to exist** — the player no longer needs to collect it.
  Used in **Level 32**.
- **Level end:** a level is completed by climbing off the top of the screen; the Lua
  bot detects the end as the onset of level-end lag frames while holding Up, and
  deliberately ends **1 frame before** the earliest possible end to manipulate the
  next level's spawn timer.

## Dig mechanics

- Digging uses the **A and B buttons** (the Lua `UntilDig(direction, digBtn)` takes
  `'A'` or `'B'` plus a facing direction). Consistent with standard NES Lode Runner
  controls: **B digs the hole to the player's lower-left, A digs to the lower-right**
  (the level-1 script digs with A while moving Right, and with B for the left-side
  hole).
- Dig state machine: `0x00A0` (dig sequence/animation state). A dig has *started*
  when `0x00A0 == 1` two frames after the press; the player is free again when
  `0x00A0 >= 0x0C`. Up to 8 simultaneous holes are tracked:
  X at `0x06A0–0x06A7`, Y at `0x06C0–0x06C7`, refill counters at `0x06E0–0x06E7`.
- Digging is slow and speed-setting-independent; routes detour heavily to avoid it.
- Timing sensitivity: in Level 1 the A press must land on a very specific frame "or
  else the spawn timer lags."

## RAM map (from adelikat's published .wch)

Coordinates: level grid is tiles (X 0–27); a tile is 16 px; `xPos = levelX +
xTileOffset/8` (offset counts in 1/8-tile = 2 px steps).
Screen mapping used by the HUD: `screenX = levelX*16 - camX(0x0004) + 16`,
`screenY = (levelY-1)*16 + 8`.

Key addresses:

| Addr | Meaning |
|---|---|
| 0x0003 | Graphics mode (0 none, 8 bg, 0x18 bg+sprites) — level visible when ==8 |
| 0x0004 | Camera X |
| 0x0006/0x0007 | Buttons pressed (raw) |
| 0x0020/0x0021 | Player block X / Y (tile) |
| 0x0022/0x0023 | Player X / Y tile offset (0–7, eighths of a tile) |
| 0x0053 | Spawn timer ("E Drop Loc"), 0–27 wrap |
| 0x0093 | Gold remaining |
| 0x0098 | Lives |
| 0x009A | Player alive (0 = dead, 1 = alive) |
| 0x009B | Falling state (32 = on ground, 0 = falling) |
| 0x009E | Global timer 0–255 |
| 0x00A0 | Dig sequence state (1 = dig started, >=0x0C = done) |
| 0x00A6 | Current level |
| 0x00C4 / 0x00C5 | Gold collected / kill count |
| 0x00DB | Game mode (1 = playing a level; 0 = menu/level select) |
| 0x00E5 | Game speed (1 = max) |
| 0x0200–0x04FF | Level tile data (3 x 256 bytes) |
| 0x0500–0x0567 | More level data (16-byte rows) |
| 0x0588–0x05AF | Hi-score / P1 / P2 score digits (BCD, one digit per byte) |
| 0x0661–0x0663 | Enemy 1–3 block X |
| 0x0669–0x066B | Enemy 1–3 block Y |
| 0x0671–0x0673 | Enemy 1–3 timer (SIGNED; negative = carrying gold; magnitude = tiles until drop) |
| 0x0679–0x067B / 0x0681–0x0683 | Enemy X / Y tile offsets |
| 0x06A0–0x06A7 / 0x06C0–0x06C7 / 0x06E0–0x06E7 | Dig hole X / Y / refill counter (8 slots) |
| 0x0700–0x070F | Player sprite quads (Y, tile, facing, X) — facing: Left=1, Right=65 (0x41) |
| 0x0720–0x074F | Enemy 1–3 sprite quads (same layout, 16 bytes each) |
| 0x0750+ | Unused (would be 4th enemy / 2nd player) |

Max 3 enemies (E1–E3); slots for a 4th enemy and a 2nd player exist but are unused.

## Bot / scripting methodology (#10412)

The entire new TAS was **programmed, not hand-input**: "I wrote a scripting framework
that breaks down each typical action in the game and then wrote scripts for every
single level. This entire TAS was done by programming the input, there is almost no
hand written input."

Framework (`lode-runner-core.lua`, ~1,300 lines, works standalone or inside TAStudio):

- **Action primitives** built on RAM oracles, each returning success/failure and
  aborting on death/panic timeouts: `LeftUntil/RightUntil(targetX)`,
  `ClimbUntil(targetY)`, `GrabLadder(h, v)` (per-frame greedy choice among {h},
  {v}, {h+v} maximizing progress via savestate probing), `UntilGold(direction)`
  (watches 0x0093), `ClimbUntilGold`, `Fall/UntilFall/FinishFalling` (watches 0x009B),
  `UntilDig(direction, 'A'|'B')` (probes 0x00A0 with savestates to find the first
  frame a dig registers), `WalkOverEnemy(direction)` (frame-searched 2-tile crossing),
  `ClimbUntilLevelEnd` (holds Up until level-end lag, then replays to finish exactly
  1 frame before the earliest end for spawn-timer manipulation).
- **Search combinators** (savestate-based local search):
  - `FrameSearch(func, limit)` — try func after 0..limit frames of delay, first success wins;
  - `BestSearch(func, limit)` — try all delays 0..limit, keep the one with lowest final frame count, replay it;
  - `BestOf([routes])` — run alternative route closures, replay the fastest;
  - `UntilLag`, `UntilNextInputFrame`, `UntilNextLagFrame` — lag-boundary alignment.
- **Scroll-skip bot** (`scroll-skip.lua`, `scroll-skip-v2.lua`): from the end pose of
  a level, `BestSearch` over end-of-level delays; for each delay it does the
  Select→Start menu exit (optionally executing the speed 1↔4 change via
  Select+B/Select+A triples), then `FrameSearch`es the 1-frame Select press inside
  the next level's scroll, validating via game mode + player X movement + camera X
  freeze. It harvests multiple working skip frames so spawn-timer luck can be chosen.
- Per-level scripts `level1.lua` … `level50.lua` (some with `-alt` variants) mix these
  primitives with a few hard-coded frame patterns found manually; they assert
  postconditions (e.g., Level 1 must end with spawn timer 22 or 23).
- Also: `nes-input-minimizer.lua` / `input-minimizer-tastudio.lua` (strip unneeded
  input, since unnecessary input can add lag), `lode-runner-hud.lua` (overlay of
  player/enemy tile positions, spawn timer, etc.).

## Why Select/Start presses appear in the movie

1. After each level completion (49 times): Select opens level select, Start confirms —
   skips the end-level animation (~10 s/level).
2. During each level's opening scroll (new TAS): a 1-frame Select press triggers the
   scroll skip (~300 frames/level).
3. In the level-select menu: Select+A / Select+B presses set game speed (initial setup
   to speed 1; temporarily 1→4→1 around levels 14, 32, 34, 47).
4. Stray-looking held inputs near level end (e.g., Up+Right for 9 frames in Level 1)
   exist to "prevent select lag on the level end screen" and to phase the spawn timer.

## Optimality assessment / remaining improvements

- adelikat calls it "one of the hardest to optimize games I've ever TASed" due to the
  triple interaction of independent lag (player/enemy/spawn-timer), the chaotic
  spawn timer at max speed, and the random 1-frame select-skip window.
- Optimization is explicitly *local*: delays are searched with bounded FrameSearch /
  BestSearch windows around scripted routes, and route choice is a hand-enumerated
  `BestOf` set. There is **no claim of global optimality**; luck choices are
  explicitly a heuristic trade ("weighing the time loss for getting ideal luck vs
  how much the better luck will actually save").
- Exploitable gaps for a JaffarPlus-style exhaustive search:
  - per-level global search over movement + dig + delay inputs (the author only
    sampled delay windows up to ~25–50 frames at scripted decision points);
  - joint optimization of select-skip frame choice x spawn-timer value x in-level
    route (author botted skip frames first, then picked luck heuristically);
  - the four speed-4 levels (14, 32, 34, 47) — any speed-1 skip window found there
    would be a large save;
  - lag phasing: the author notes single-frame delays in "random places" can yield
    big gains — a systematic search could find these everywhere.
- Judging notes (7195, GoddessMaria): praised "mastery of lag management" and
  "craftier solutions"; no known-improvement list was left in either submission.

## Sync notes for reproduction

- ROM: Lode Runner (U); SHA1 `ea09aec11efc4b42ebc644ea243c2284001fe34f`
  (movie files say "(U) [!]", site catalogs it as "(U) [i]" — same image).
- Power-on start, no SRAM. NTSC NES, 60.0988138974405 fps. Optimization metric:
  TAS timing (last input frame).
- BizHawk 2.6.2 (old) / 2.11.1 (new) — NesHawk core assumed (BizHawk default for
  accuracy; movie headers in the .bk2 confirm the core).
- The game's "lag" is not classic input-poll lag (10412: the lag counter never
  moves); an emulator core must reproduce the frame-budget behavior at max speed
  exactly (player/enemy/spawn-timer updating on different frames) or the movie
  desyncs — this is the main resync risk for QuickerNES vs NesHawk.
