# NES Lode Runner — RAM Map and adelikat Lua Bot Digest

Sources:

1. **RAM map**: tasvideos userfile https://tasvideos.org/UserFiles/Info/639133364566427500 —
   this is `RamMap.wch` (a BizHawk RAM-watch file by adelikat). Verified byte-identical (modulo
   trailing newline) to `LodeRunner/Tools/RamMap.wch` in the repo below.
2. **Lua bot framework**: https://github.com/adelikat/tas/tree/master/LodeRunner — the scripting
   framework adelikat used to produce the current published TAS (movies `adelikat-lode-runner-v7.bk2`
   etc. are in `LodeRunner/movies/`). All files under `LodeRunner/` were read: `Lua/` (core, HUD,
   per-level scripts 1–50, minimizers, scroll-skip, toLevel1), `Tools/` (watch files, CPU trace logs
   of the select skip), `movies/`, `lode.bat`, `compare.xlsx`.

Reference movie note: the old reference `Takanawa.fm2` was made on **Lode Runner (J)**
(`romFilename Lode Runner (J) [!]`); adelikat's own work is in `.bk2`/`.tasproj` (BizHawk, NesHawk
core — one comment says "Magic neshawk only").

---

## 1. RAM map (complete, from RamMap.wch + addresses/semantics recovered from Lua code)

All addresses are NES main RAM (2 KB, $0000–$07FF). "b/u" = unsigned byte unless noted.
Semantics in *italics* were recovered from the Lua code, not the .wch file.

### Global / game state

| Address | Meaning | Notes / units |
|---------|---------|---------------|
| $0003 | Graphics mode | 0 = show nothing, 8 = show BG only, 0x18 = BG + sprites. *Lua waits for ==8 to detect "level appeared".* |
| $0004 | Camera X | Level is wider than screen (28 tiles × 16 px = 448 px); camera scrolls horizontally. *Used to detect the "select skip" (camera stops moving).* |
| $0006 | Buttons pressed (bitfield) | |
| $0007 | Buttons pressed (duplicate?) | |
| $0053 | Spawn/Drop timer ("E Drop Loc") | Free-running counter. **Enemy respawn column at the top row = (($0053) + digCounter) % 32** (see HUD `drawSpawnPrediction`). Also governs when a carried gold is dropped. Primary luck-manipulation target. |
| $009E | Global timer | Free-running 0–255 frame counter. |
| $00A7 | Game mode (menu) | 0 = 1 player, 1 = 2 player, 2 = Edit mode. |
| $00B7 | Music flag | 1 = playing, 0 = silent. |
| $00B8 | Music tempo | Lower is faster. |
| $00B9 | Music tempo timer | Increments up to value of $00B8. |
| $00C3 | Current player (1P/2P) | |
| $00DB | Game mode (play state) | 1 = playing a level. *Lua uses ==1 to confirm gameplay resumed after Select tricks; ==0 means on a menu/level-select.* |
| $00E5 | Game speed | Changed on the speed menu / with Select+A (faster, value decreases) and Select+B (slower, value increases). TAS value during play = 1 (fastest); 3× Select+B takes it 1→4. `toLevel1.lua` presses Select+A 34 times at the title-screen speed menu to reach max speed. |

### Player

| Address | Meaning | Notes / units |
|---------|---------|---------------|
| $0020 | Player block (tile) X | Level tile coords: X ∈ [0, 27] (28 columns). |
| $0021 | Player block (tile) Y | Y ∈ [1, 13]; **Y=1 is the top row**, larger Y is lower. Screen y = (Y−1)·16 + 8. |
| $0022 | Player X tile offset | Sub-tile position. Lua computes `xPos = $0020 + $0022/8` (units of 1/8 tile); the HUD also adds the offset directly as pixels to `tileX·16`. Comment "should be precisely 7.500" shows 0.5-tile granularity matters. |
| $0023 | Player Y tile offset | Same convention: `yPos = $0021 + $0023/8`. |
| $009A | Is player alive | 1 = alive, 0 = dead. |
| $009B | Is falling | 32 = on ground, 0 = falling. |
| $00A0 | Dig sequence (animation state) | *Lua: ==1 two frames after the dig press ⇒ dig successfully started; player is locked until it reaches ≥ 0x0C (dig complete, control returns).* |
| $0098 | Current player lives | |
| $00A6 | Current player level | 1-based level number. Mirrors: $05A0 (P1), $05B0 (P2). |
| $00C4 | Gold collected | Count picked up this level. |
| $0093 | Gold remaining | Decrements on each pickup; *Lua detects a gold grab by watching this decrement. All gold gone ⇒ exit-ladder phase.* |
| $00C5 | Kill count | Enemies buried. |
| $00D9 / $00DA | Pole / climb sound effect | |
| $0702 | Player facing direction (sprite attr) | Constants used by Lua: Left = 1, Right = 65 (0x41). |

### Level data

| Address | Meaning |
|---------|---------|
| $0200–$02FF | 256 bytes of level data |
| $0300–$03FF | 256 bytes of level data |
| $0400–$04FF | 256 bytes of level data |
| $0500–$0567 | Level data rows: 16-byte blocks at $0500, $0510, $0520, $0530, $0540, $0550, $0560 (+8 more bytes) |

(The .wch does not decode the format; the tile map for a 28×13 level lives here. Digging/refilling
mutates it, so it belongs in any state hash.)

### Enemies (3 max: E1, E2, E3; index i = 0..2)

| Address | Meaning | Notes |
|---------|---------|-------|
| $0661+i | Ei block X | Same tile coords as player. |
| $0669+i | Ei block Y | |
| $0671+i | Ei timer (**signed** byte) | Multi-purpose: **negative = carrying gold** (countdown to gold drop; HUD displays −timer); **positive 1..125 = pit timer** (trapped in a dug hole); **126–127 = respawning at top row**. 0 = normal. |
| $0679+i | Ei X tile offset | Same 1/8-tile convention as player. |
| $0681+i | Ei Y tile offset | |
| $0722+16·i | Ei facing direction (sprite attr) | Left = 1, Right = 65. |

Enemy respawn: an enemy buried in a hole respawns at the **top row (Y=1)**, column
`($0053 + digCounter) % 32` — manipulable by choosing when to dig and by 1-frame delays.

### Dig holes (8 simultaneous slots, index j = 0..7)

| Address | Meaning |
|---------|---------|
| $06A0+j | Dig location X (slot j) |
| $06C0+j | Dig location Y (slot j) |
| $06E0+j | Dig counter (slot j) — hole-refill timer; 0 = slot unused |

### Scores (BCD, one digit per byte, 0–9)

| Address | Meaning |
|---------|---------|
| $0588–$058F | Hi score, digits 1–8 |
| $0590–$0597 | Score (temp copy during level transition), digits 1–8 |
| $0598–$059F | Current player score, digits 1–8 |
| $05A0 | Player 1 current level (== $00A6) |
| $05A1 | Player 1 lives (== $0098) |
| $05A8–$05AF | Player 2 score, digits 1–8 |
| $05B0 | Player 2 current level |
| $05B1 | Player 2 lives |

### Sprite/OAM shadow ($0700–$074F)

4-byte entries (Screen Y, Tile, Direction/attr, Screen X), 4 sprites per entity:

| Range | Entity |
|-------|--------|
| $0700–$070F | Player sprites 1–4 |
| $0710–$071F | Unused (would be 2nd player) |
| $0720–$072F | E1 sprites 1–4 |
| $0730–$073F | E2 sprites 1–4 |
| $0740–$074F | E3 sprites 1–4 |
| $0750–$075F | Unused (would-be 4th enemy) |
| $0760–$07FF | Unused |

### RNG

There is **no dedicated RNG address** in this game per the map. All "randomness" is the pair of
free-running counters **$0053 (spawn/drop timer)** and **$009E (global timer)** sampled at event
time — which is why the whole Lua bot is built on 1-frame delay searches ("Manip E3 spawn",
"spawn timer must be a specific range", `SpawnTimer() == 22 or 23` asserted at level-1 end).

### Coordinate/physics summary

- Tile = 16×16 px. Level = 28 tiles wide (X 0–27) × 13 tall (Y 1–13, 1 = top).
- Position = block coord + tileOffset/8 (1/8-tile sub-position). Screen x = tileX·16 − camX + 16;
  screen y = (tileY−1)·16 + 8.
- Walking one tile takes at most ~16 frames at game speed 1 (`RightUntil` panic-abort =
  tiles·16 + 16 frames). Lag frames (which ignore input) are frequent and level-dependent —
  enemy positioning changes lag ("The enemy patterns are very slightly different and cause
  significant lag differences").

---

## 2. How the Lua bot framework works

Platform: BizHawk (EmuHawk, NesHawk core), driven either standalone (savestate slots) or inside
TAStudio (branch/marker integration). `lode-runner-core.lua` defines a global `c` API; each of
`level1.lua` … `level50.lua` is a route script for one level, run sequentially
(session file `lode-runner.luases` lists them all, plus the always-on `lode-runner-hud.lua`).

### It is a greedy scripted-search bot, not a global optimizer

Each level script is a hand-written route expressed in high-level movement primitives; every
primitive internally does savestate-based micro-search so each step is locally frame-optimal.
On top of that, small brute-force searches pick delays and route variants:

- **`FrameSearch(func, limit=25)`** — try delay 0,1,2,… before running `func`; first delay where
  it returns true wins (earliest-success search). Used for select skips, enemy-timing windows.
- **`BestSearch(func, limit)`** — try *every* delay 0..limit, keep the one with the smallest final
  frame count, replay it.
- **`BestOf({f1, f2, …})`** — run alternative route closures from a savestate; replay the fastest
  successful one. Level scripts commonly offer 2–4 hand-written sub-routes per section.
- **`UntilLag(btns)`** — advance until a lag frame is hit.

### Movement primitives (all bail out on death, most re-verify with savestates)

- `LeftUntil/RightUntil(tileX)`, `LeftFor/RightFor(n)` — hold direction until $0020 reaches target.
- `ClimbUntil(tileY)` — Up or Down until $0021 reaches target.
- `GrabLadder(h, v)` — the subtle one: crosses into the next column, then per frame tries
  {h+v}, {h}, {v} with savestates and picks the combination that first changes Y (and prefers
  pure-vertical if it climbs at least as well). Handles grabbing a ladder while running past it.
- `GrabAndClimbOne(h)` — 1-tile diagonal optimizer; evaluates each 1-frame button choice with
  score `xPos + (13 − yPos)·10` (vertical progress worth 10× horizontal), picks the best.
- `UntilGold(dir)` / `ClimbUntilGold` — advance until **$0093 decrements** (gold grabbed).
- `UntilDig(dir, 'A'|'B')` — walk until the first frame a dig press "takes" (**$00A0 == 1** two
  frames later), prefer pressing only the dig button, then wait until **$00A0 ≥ 0x0C** (dig
  animation over). A = dig right, B = dig left (per usage in routes).
- `UntilFall/FinishFalling/Fall{Left,Right}` — via $009B (0 = falling); `FinishFalling` replays
  the fall without inputs so the fall itself carries no wasted presses.
- `WalkOverEnemy(dir)` — FrameSearch a delay such that walking 2 tiles across an enemy's head
  survives.
- `ClimbUntilLevelEnd()` — hold Up at the exit ladder until **lag frames** begin (level end causes
  a lag streak), then replays ending **1 frame before** the level ends "to manipulate the next
  level" (spawn-timer manipulation carries across levels).

### Level-completion / phase detection

- Level end, menu transitions, and the level-start jingle are all detected via `emu.islagged()` —
  the game lags (ignores input) during non-interactive phases. `UntilNextInputFrame` /
  `UntilNextLagFrame` navigate to the exact boundary frames.
- Gameplay active: $00DB == 1; level rendered: $0003 == 8; level number: $00A6.

### The two big menu/glitch tricks

1. **Select skip ("scroll skip")** — at level start, pressing **direction+Select** on the correct
   frame skips the camera scroll-in (verified by: $00DB==1, camera X stable over 12 frames, and
   the player already moved). Each level script "Starts on the frame immediately after pressing
   select to do the select skip". `FindSelectSkip` searches the delay (windows up to ~310 frames);
   `scroll-skip-v2.lua` automates the whole level-to-level chain: end level with Up → press
   Select on the score screen (enters level-select, $00DB==0) → Start → jingle → FrameSearch the
   direction+Select skip at the next level. The `Tools/*.log` files are NesHawk CPU trace logs
   comparing "press select, wait 2 frames" fast/no-skip variants used to reverse the trick.
2. **Game speed menu** — at the title: Start → Select ×2 → **Select+A pressed 34 times** to crank
   game speed to max ($00E5 decreasing each press; in-play value 1 = fastest), then Start into
   level 1 (`toLevel1.lua`).

### Luck manipulation

All enemy behavior manipulation is done with 1-frame `WaitFor(1)` nudges plus FrameSearch,
targeting: enemy respawn column (`($0053 + digCounter) % 32`, HUD draws the predicted spawn
tile), gold-drop timing (enemy timer $0671+i going negative ⇒ carrying gold; drop windows like
"spawn timer must be 12 to 15"), and lag reduction (enemy pixel positions change lag; e.g.
"Causes E2 to lag by a pixel", "This causes E1 to lag by a pixel, just enough to die from the
dig"). Level 1 asserts $0053 ∈ {22, 23} at level end or fails the run.

### Input minimizer (post-processing)

`nes-input-minimizer.lua` / `input-minimizer-tastudio.lua`: replay the finished movie; for every
frame with input, turn off each pressed button, advance 10–20 frames, and compare a **full RAM
snapshot ($0000–$07FF)** (tastudio version also compares CPU registers) against the original.
If identical, the button was cosmetically useless and stays off. Produces a minimal-input movie
without resync risk.

### HUD (`lode-runner-hud.lua`)

Draws: player/enemy boxes with `(x.xxx, y.yyy)` positions, enemy pit/gold timers, **predicted
respawn column** for each active dig slot, game speed, level number, Y-axis ruler.

---

## 3. Notes for a JaffarPlus setup

- **Property candidates**: player $0020/$0021/$0022/$0023, $009A, $009B, $00A0; enemies
  $0661–$0663, $0669–$066B, $0671–$0673 (signed), $0679–$067B, $0681–$0683; gold $0093/$00C4;
  dig slots $06A0–$06A7, $06C0–$06C7, $06E0–$06E7; timers $0053 and $009E; level $00A6;
  game/graphics mode $00DB/$0003; camera $0004.
- **Hash**: must include the mutable tile map ($0200–$0567) because digging changes traversability,
  plus dig-slot arrays and enemy timers. Score digits ($0588–$05BF) and sound bytes should be
  excluded/irrelevant.
- **Win detection per level**: gold remaining $0093 == 0 and then the top-exit climb; concretely
  the level-number increment ($00A6) or the game leaving play mode ($00DB) after the lag streak.
- **Timing counters** $0053/$009E are the "RNG": keeping them in the hash preserves manipulation
  correctness but multiplies state count; consider the same latched/allowlist treatment used in
  other NES campaigns.
- **Known human-found tricks to replicate**: direction+Select scroll skip at each level start
  (frame window), max game speed setup at the title, end-level 1-frame-early Up to steer the next
  level's spawn timer, enemy-respawn column steering via dig timing.
