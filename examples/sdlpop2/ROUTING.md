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
| Slow fall: 1 2 4 6 10 23 22 24 25 27 28 26 28 27 25 24 22 | none | 628 | running |
| Potion: 1 2 4 3 7 11 7 15 10 23 22 24 25 27 28 26 28 27 25 24 22 | room 11 | 1172 | queued |

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

## Levels 5..14

Not routed yet.
