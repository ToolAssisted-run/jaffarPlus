# Prince of Persia 2 TAS: routing notes

This file records every route tried, level by level, with its length, so that the global route can be put together at
the end. Lengths are game ticks (one Chimera step in play). "Level N at tick T" counts from the start of a new game.

## The goal that shapes the route

Jaffar (level 14) dies only to the flame's fireball. Becoming the flame and casting once needs **max HP 11 at full
health** when the turning starts. This was measured in SDLPoP2 (1.1) with chained standing turns on level 14:

- Turns 4..8 cost 1 HP and 1 max HP each. The 8th turn needs HP above 4, or the prince dies.
- The body's collapse takes 3 more HP and max HP.
- A cast needs HP above 2 and costs 2.
- Results by max HP before turning: 9 gives 4/4 then 1/1; 10 gives 5/5 then 2/2 (cannot cast); **11** gives 6/6 then
  3/3 (one cast).
- One fireball kills Jaffar outright (`final.c` `ovl_kind6_char`, `take_hp(100)`). No HP drained in the next 600 ticks.

The prince reaches level 3 with 3/3, so the run needs **8 big potions** (+1 max HP each). Small potions can refill HP
before the turns. The big potions come from two sources:

- **The levels' big potions:** for each one, the cost of taking it against skipping it (tables below).
- **Level 14's hidden spider room (room 3):** a killed guard drops a big potion 1 time in 3 (`final.c ovl_34024`,
  `random_2751(2) == 2`, on a plain floor tile only). The cost is the way down, the way back up, and the ticks per
  potion at steady state. Not measured yet.

A potion's kind is `(tile attribute >> 5) & 7`; 2 is a big potion.

## Level 1

| Route | Ticks | Notes |
|---|---|---|
| Straight | 460 | 15 seeds tried, 460..463; 460 taken as the optimum (`0100/level01.sol`) |
| Restart Level at the checkpoint | **433** | Restart (Alt+A) in the checkpoint room (`0100/level01-restart.sol`) |

## Level 2

The sand puzzle's answer (the tile that must stay up) comes from the RNG. 20 RNG values were tried at the level's
start: 102, 105, 105, 107, 115, 119, 136, 137 (x3), 143 (x3), 144, 145 (x3), 147, 148 (x2). The best is **102**
(RNG 18, `0200/seeds/seed18.sol`).

## Levels 1 and 2 together

| Combo | Level 3 at tick | Chimera (fm) | Chimera (MT-32) |
|---|---|---|---|
| 460 + 102, level-1 seed 151 | 562 | Random Seed 48, frame 646 | - |
| **433 (restart) + 102**, level-1 seed 7881336 | **535** | Random Seed 25, frame 620 | frame 1275 |

Piano rolls are in `combo/`.

## Level 3

Big potions: room 10 (tile 18, "potion 1") and room 20 (tile 9, "potion 2"). The other potions are small.

Measured from the restart combo's arrival (tick 535) in QuickerSDLPoP2, 100 GB runs (`0300/`):

| Route | Big potions | Ticks | vs best |
|---|---|---|---|
| Left exit: 1 5 7 8 11 13 | none | **476** | - |
| Right exit: 1 4 16 3 2 14 15 17 19 22 30 31 30 | none | 594 | +118 |
| Potion 1: ... 2 14 10 14 15 ... 22 30 31 30 | 1 | 752 | +276 |
| Potion 2: ... 22 18 20 21 26 27 28 25 24 23 13 | 1 | 985 | +509 |
| Both: ... 14 10 14 ... 22 18 20 ... 23 13 | 2 | 1136 | +660 (330 per potion) |

Potion 2 on top of potion 1 costs 384 ticks. Potion 1 on top of potion 2 costs 151.

Level notes:

- **Wall clips:** sword frames (Ctrl, then Down+Ctrl: strikes, frames 233..239) push the prince 32 px a tick through
  solid tiles. This happens on room 15's top row (14 -> 17) and as the room 13 plate jitter on the left exit. An early
  left-exit solution also passed room 8's closed gate this way. The room 15 clip is verified in Chimera's core (`combo/chimera-boot-to-level4-right-glitch*.txt`).
- **Room 30's gate stays open (0xFF) only with a weight on its plate:** the loose floor above it (row 0 col 1) has to
  fall onto it. Pressed by the prince alone, the gate opens to 0xFA and shuts again.
- **Room 8:** the loose floor at row 1 col 8 falls onto room 11's plate, which opens room 8's gate.
- **Room 13:** the loose floor at row 1 col 8 falls onto the plate below it and opens the exit door. The plate can also
  be reached through the wall with the sword jitter.
- **Exit doors:** press Up while standing next to an open door (modifier >= 0x2A). The level ends 22 ticks later.
  In Chimera, a story scene follows level 3; Space skips it on the next frame.
- **Shortcut not used:** falling from room 3 lands directly in room 10 (potion 1). The potion routes may take it.

**RNG (fixed 2026-10-01):** the copy protection between levels 2 and 3 draws one random number in the game, which
QuickerSDLPoP2 used to skip. SDLPoP2's headless core now makes that draw (SDLPoP2 597d25a, quickerSDLPoP2 bdeec56),
and level 3 starts with Chimera's seed. Under the corrected RNG the left exit was re-run (still 476) and plays
identically in Chimera's core (`combo/chimera-boot-to-level4-left.txt`, level 4 at frame 1107 with fm). The other
four routes replay unchanged.

The level 3 used from here on is the left exit: level 4 starts at tick 1011 (`0400/level010203.sol`), with 3/3.

## Level 4

Big potion: room 11 (row 2 col 7). Slow-fall potion: room 6 (row 0 col 2). The exit door is in room 22 (row 2 cols
3..4); the plate in room 26 (row 1 col 8) opens it. Guards: 7, 13, 14, 17, 23, 26, 27, 28 (three), some of them
skeletons (char id 4), which fall to 0 HP and rise again.

| Route | Big potions | Ticks (chunks, 15 GB) | Ticks (100 GB) |
|---|---|---|---|
| Slow fall: 1 2 4 6 10 23 22 24 25 27 28 26 28 27 25 24 22 | none | 628 | **592** |
| Potion: 1 2 4 3 7 11 7 15 10 23 22 24 25 27 28 26 28 27 25 24 22 | room 11 | **1172** | 1191 (worse; chunks kept) |

The level 4 used from here on is the slow-fall route (592): level 5 loads at tick 1604 (`0500/level01020304.sol`, the
last line is the load tick), with 3/3. In Chimera's core (MT-32) level 5 starts at frame 2365.

Level notes:

- **Top-row slide:** a guard's hit in room 23 drops the prince through the floor into room 22's top row, inside the
  wall. Held in a landing frame (109) with Down, he is pushed 32 px a tick and slides right through the solid top
  row of rooms 24, 25, 27 and 28, over all the guards, in about 40 ticks. The slow-fall chunks use it.
- **Room 11's gates:** the plate at col 1 holds the gates at cols 2 and 5 open (the plate at col 3 opens col 2's only).
  The prince has to stand on it until col 5's gate is open enough, then run. That gate's modifier carries 0x400
  above its opening.
- **Skeletons:** an Opponent HP magnet keeps the search beside a downed skeleton; the level 4 scripts use none.
- **Falls:** without the slow-fall potion, room 10's holes drop the prince through 23 into 22 to his death; the potion
  route stands at room 10's col 9 before going down.
- In quicker the level ends ("Next Level" = 5) one tick before level 5 loads; the times count to "Next Level".

## Level 5

Rooms (left to right): 22 1 23 2 9 3 4 7 10 12 on top (21 above 12); 5 6 11 18 below 4 7 10 12; 24 8 25 20 17; 13 14 15
16; 19 below 13. Big potions: room 22 (row 2 col 4) and room 6 (row 0 col 5). The exit is the magic carpet in room 3
(the trap tile at row 2 col 3): sitting on it (the crouched frame 109, not the plain stoop) lifts the prince through
the hatch at row 0 col 6, which room 3's top-row plate (col 8) toggles open. The rope bridge: 7 | 10 | 12, row 1.

| Route | Big potions | Ticks (chunks) |
|---|---|---|
| No potion: 1 23 2 9 3 4 5 8 13 14 15 16 17 18 12 10 7 4 3 | none | **1472** |
| Potion 1: 1 23 2 9 2 23 1 22 1 23 2 9 3, then as above | room 22 | 2153 (+681) |
| Potion 2: as above to 12, bridge collapse, 11 6 11 18 12 10 7 4 3 | room 6 | in progress |
| Both | rooms 22, 6 | not yet |

Level notes:

- **No direct route:** room 3's top row (the hatch's plate) is reached only from room 4's top row, and that one only
  from room 7's: from the start side the prince gets as far as room 4's middle row (through room 3's) and no higher.
  Hence the loop around the bottom of the level.
- **The bridge needs no fight** on the way to the carpet: the prince crosses 12 -> 10 -> 7 with running jumps in ~30
  ticks; room 10's plate (row 1 col 9) opens room 7's gate.
- **Room 2:** the gate at row 1 col 7 is held by the plate at row 2 col 2, with a loose floor right above it: bumping
  the loose floor down jams the gate open (0xFF).
- **Room 18:** from row 2 col 4, climbing onto the plate at row 1 col 5 costs a hit point (fatal at 1/3): the routes
  drink room 14's small potion (row 2 col 6) first.
- **Room 22 (potion 1):** walls on row 2 separate the potion from the gate; the way is row 1, and the loose floor at
  row 1 col 4 must not fall: it lands on the potion and breaks it (0x0A -> 0x0E). Room 9's loose floor (row 1 col 6)
  dropped on its plate jams room 22's gate open.
- **Room 6 (potion 2):** room 11's gate (row 0 col 9) is held by its own plate (row 0 col 2) or room 20's (row 0 col
  7); from room 20 it shuts before the prince gets round to it. Room 11 is reached from above by the bridge collapse
  (bridge5.c): the prince (cols 4..6) and the skeleton (cols 5..6) together on room 10's span for 60 ticks.
- **The level's end:** quicker loads level 6 on the tick the carpet leaves the top; level 6 starts in room 27.

## Levels 6..14

Not routed yet.
