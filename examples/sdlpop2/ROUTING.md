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

| Route | Big potions | Ticks | vs no potion |
|---|---|---|---|
| No potion: 1 23 2 9 3 4 5 8 13 14 15 16 17 18 12 10 7 4 3 | none | **1436** | - |
| Potion 2: as above to 17, then 18 11 20 25 6 11 18 12 10 7 4 3 | room 6 | 1696 | +260 |
| Potion 1: 1 23 2 9 2 23 1 22 1 23 2 9 3, then as no potion | room 22 | 2144 | +708 |
| Both: potion 1's start to 3, then potion 2's way | rooms 22, 6 | 2442 | +1006 |

Final table (2026-10-02): all four routes re-run clean from level 5's start with the smoothed y, in chunks (30 GB;
`0500/*-chunks.sol`), keeping each route's best. The clean rerun gave 1448 / 1696 / 2144 / 2458: the no-potion and
both routes kept their earlier solutions (1436, 2442), 12 and 16 ticks shorter. A chunked search varies by tens of ticks
from run to run (which of many near-equal states a full database keeps), e.g. potion 1's first chunk took 707 in one
run and 743 in the next. All four reach level 6 in QuickerSDLPoP2 and in Chimera's core (MT-32: frames 3811 / 4071 /
4519 / 4817).

Level notes:

- **No direct route:** room 3's top row (the hatch's plate) is reached only from room 4's top row, and that one only
  from room 7's: from the start side the prince gets as far as room 4's middle row (through room 3's) and no higher.
  Hence the loop around the bottom of the level.
- **The bridge needs no fight** on the way to the carpet: the prince crosses 12 -> 10 -> 7 with running jumps in ~30
  ticks; room 10's plate (row 1 col 9) opens room 7's gate.
- **Room 2:** the gate at row 1 col 7 is held by the plate at row 2 col 2, with a loose floor right above it: bumping
  the loose floor down jams the gate open (0xFF). The prince need not wait crouched until it lands: getting up as
  soon as it falls saves ~24 ticks (the first scripts pulled him down until the gate jammed, so standing up early
  scored worse and was pruned; noticed by Sergio in the videos).
- **Room 18:** from row 2 col 4, climbing onto the plate at row 1 col 5 costs a hit point (fatal at 1/3): the routes
  drink room 14's small potion (row 2 col 6) first.
- **Room 22 (potion 1):** walls on row 2 separate the potion from the gate; the way is row 1, and the loose floor at
  row 1 col 4 must not fall: it lands on the potion and breaks it (0x0A -> 0x0E). Room 9's loose floor (row 1 col 6)
  dropped on its plate jams room 22's gate open.
- **Potion 2 (room 6):** the canonical way drops room 17's loose floor (row 0 col 8) on its plate to jam the gate to
  room 20 (jumping from col 6: a shake counts only from another column), then 20 -> 25 (top right) -> up into 6.
  The search skips that: from room 17 it goes up into 18's bottom row, where a sword-frame push at col 1 takes it
  through the walls into room 11's col 8, and it falls into room 20's top row. The scripts now route only that way
  (with both ways as steps of one chain, a loose floor falling without jamming the gate left the skip penalized).
  After the potion, room 11's plate (row 0 col 2) opens its gate to room 18's top row. Room 20's own loose floor (row 0
  col 8) must not fall: its plate shuts the gate to 25.
- **No clip from room 5's potion ledge into room 6:** a search of every state reachable from the ledge found none.
- **The bridge collapse** (bridge5.c: the prince at cols 4..6 and the skeleton at 5..6 together on room 10's span for
  60 ticks) was tried as a way down into room 11; the skeleton would not come to the middle. Not needed.
- **Smoothed y for climbs** (from 2026-10-02; as in the PoP1 game): a jump-up and the hang that follows leave y
  unchanged and a climb moves it 63 px at once, so they scored no better than standing still, and with a full
  database the search waited ~90 ticks before the climbs from room 25 into room 6 and inside room 6 (seen by Sergio in
  the video). The game's Y magnet now uses a smoothed y ("Player Smooth Pos Y", princeOfPersia2.hpp
  stateUpdatePostHook): jump-up f67..f80 1 px a frame, hang f87..f99 20 px, climb up f135..f149 and down (seq 68)
  spread over their frames. Both potions: 2556 -> 2442. The routes solved before it (no potion, potion 1, potion 2)
  may still hold such waits.
- **The carpet** starts from a crouch that is not the landing (sequence 0x14) or the stoop (0x11); holding Down after
  landing on it starts it ~46 ticks later. (The first scripts' lift reward was outweighed by its own magnet; fixed.)
- **The level's end:** quicker loads level 6 on the tick the carpet leaves the top; level 6 starts in room 27.

## The original DOS game (checked 2026-10-01 by the OpenSamurai session, DOSBox-X oracle, 1.1)

`0500/level01020304.sol` (levels 1..4) holds in the original: room, x, y, frame and HP match SDLPoP2's core on every
tick, the restart at the checkpoint, level 3's room 13 jitter and level 4's top-row slide included. Two differences:

- **Level ends take longer in DOS:** the game waits for the level-end music, which SDLPoP2's sound model (and so
  QuickerSDLPoP2 and Chimera's core) does not: +105 ticks after level 1, +0 after 2, +94 after 3, +93 after 4 (292 over
  the chain), whatever the inputs.
- **The RNG drifts in DOS:** one more draw at level 3 tick 859 (sound-gated) and draws during those waits; level 4
  starts with seed 1915662242 in DOS against 3703121395 here. An RNG-dependent route after a level end may not replay
  in DOS as it does in Chimera.

## Level 6

Rooms (the temple): 28 27 on top; 24 15 13 12 2 14; 26 23 21 9 8 5 10 11; 25 22 16 7 4 6 3 1; 17 18 19 20 below 4 6 3
1. The prince comes out of room 3's door (his room reads 27 at first). Medusa heads (charid 7/8, `heads.c`): one in
room 4, two in room 7, one in room 16. The broken sword: room 17 (row 1 col 5), past the crawl tunnel from room 18.
The big potion: room 5 (row 2 col 7). The exit: room 16's two doors, left (cols 1..2, opened by the plate at col 7)
and right (cols 5..6, the plate at col 0). **The door decides where level 7 starts**, so each door is its own route.
Ticks count from level 6's first tick (Chimera frame 3811 after level 5's no-potion route, the story skipped).

| Route | Big potion | Ticks | Level 7 starts in |
|---|---|---|---|
| Sword (27 3 6 18 6 4 17), back up into 4: the head's bite throws him through the left wall into 7; 16, **left door** | none | **426** | room 32 |
| The same, **right door** | none | 440 | room 4 |
| No sword: 27 3 6 18 6 4 (the head waited out) 8 12 13 9 21 23 22 16, right door | none | 742 | room 4 |
| Sword, climbing past room 4's head (killed): 8 9 (a wall clip) 21 23 22 16, right door | none | 846 | room 4 |
| Sword, as above to 9, then down its hole into 7 (two heads), 16, right door | none | 959 | room 4 |
| Potion and push (Sergio's route), left door: 27 3 6 18 17 4 8 5 6 5 6 18 17 4 7 16 | room 5 | 1093 | room 32 |
| The same, right door | room 5 | 1155 | room 4 |

Final table (2026-10-02): 30 GB searches, from room 4 (the pushes) or from room 8 (the routes over the top), in chunks
for the potion. The pushes play to level 7 in Chimera's core on the predicted frame (MT-32: 4236 and 4250 without
the potion, 4903 and 4965 with it). The big potion costs **+667** for the left door (1093 against 426) and **+715**
for the right one (1155 against 440).

Failed or not run:

- **No sword, pushed:** without the sword the bite's push lands inside room 7's top-row wall, for good.
- **No sword, down through room 7:** room 7's two heads.
- **Left door over the top:** not run (slower than the push by far; with the RNG off by one, the sword's way down
  through room 7 reached room 16 with 1 HP and its head killed him).

Level notes:

- **Skipping the story scene costs a random draw:** after levels 5 and 9 the game lets the sound play on into the
  story scene (`game.c`); Space skipping it stops the sound (`nis.c`), and level 6's first tick, with nothing playing,
  starts an ambient sound: one random draw. The search core skipped no scene (as a scene watched to its end: no draw),
  so its first level 6 solutions were one draw behind Chimera and died to room 7's heads there (room 4's head never
  noticed). SDLPoP2 `pop2_set_skip_scenes`, quickerSDLPoP2 and JaffarPlus `"Skip Story Scenes": true` (routegen.py
  sets it); levels 1..5 replay the same tick for tick.
- **Room 4's head** spots the prince as he climbs in from room 17 with the sword and bites once (3 -> 2). The push:
  bitten in the sword stance by the left wall, he goes through it into room 7's bottom row (room 4 x ~169 -> room 7
  x 432). Pushed from the upper rows (climbing) he lands inside room 7's top-row walls, for good (a fail rule).
- **Climbing past room 4's head:** with the sword the head sits on the pillars he climbs; the search kills it (three
  cuts, or smashed against the wall) for a bite: room 8 at 569 (2/3). Without the sword he waits it out: room 8 at 318
  (3/3).
- **Room 8 -> 9 with the sword:** a sword-frame wall clip from room 8's middle row (col 0) left into room 9's,
  skipping 12 and 13.
- **Room 9's hole drops three rows** into room 7: from a hang it costs a hit point.
- **The doors' modifiers carry 0x2080000** whether shut or open; the opening is the low byte ("Tile Opening", 0x2A
  open). A rule on the whole value held from the start, and the first right-door runs never pressed its plate.
- **The potion (Sergio's route):** the running jump from room 5's middle-row tile (col 0) falls ~20 px short of the
  potion's floor (row 2 cols 6..7); Shift grabs the top of room 6's wall under it as he falls by, and Up climbs onto
  it. Back down off its left end, room 6's loose floor (row 2 col 5) is the only thing that breaks the fall, so it must
  still be there: the first way into room 18 is the hole (a fall, a hit point), not across the loose floor (the routes
  without the potion drop it at tick 61). He takes the sword on that first pass. Room 4's head must not be killed and
  must be left in the open on the bottom row: knocked into the left wall (x < 160) it never sees him again; left on the
  middle row's pillar (col 1) it cannot fly down. On the way back (17 -> 4, max HP 4) it pushes him through the wall.
- **Scoring:** a room on the route but not in the current step scored 0 (no magnet), more than any state in the
  step's own room: the search drifted into room 6 below the potion, and into rooms 7 and 16 after an early push.
  routegen's `penalize_ahead` gives those rooms the -5000 magnet. An HP magnet (`Set Player HP Magnet`) and a head-kill
  reward (`Set Opponent HP Magnet`) for the climbs over the top.

## Level 7

Rooms (the temple): 25 26 27 on top; 19 14 20 28 16; 32 . 18 13 21 8 2; 31 7 17 12 6 3 29; 30 22 11 10 5 1 4 15; 24 9
below 10 5; 23 below 9. Level 6's left door starts level 7 in room 32 (route 1), its right door in room 4 (route 2).
The exit: room 21's door (row 1 cols 7..8), opened by room 8's plate (row 1 col 6). Big potions: room 7 (row 1 col 8)
and room 24 (row 0 col 3). Heads (charid 7) in rooms 7, 9, 10, 20, 22, 24; charid 8 in rooms 2 and 22; the charger
(charid 11, `beast.c`: waits, turns, runs along its row and pounces) in rooms 3, 20, 29, 31. Ticks count from level 7's
first tick; level 8 starts in room 16 (x 296) on every route.

**Current table (2026-10-03, Sergio's rules):** level 7 is reached with max HP 5 (the run's big potions before it;
`"Max HP At Level": [7, 5]` sets it as the level starts), the small potions (+1 hit point) are never drunk (fails on
their tiles), route 2 is dropped (no faster in any variant), and the routes start 32 -> 31, right along it and through
its wall into room 7 (Sergio's idea).

| Route | Big potions | Room 2 (bottom row) | Ending | Ticks | vs best |
|---|---|---|---|---|---|
| 32 31 (the wall) 7 (the head's push) 17 12 6 3 29 2, then 16 28 20 21 8 21 | none | 573 | 391 | **964** | - |
| The same with room 7's potion | room 7 | 636 | 391 | 1027 | +63 |
| The same, then 17 11 10 (push) 5 9 24 (potion) 9 5 1 5 10 11 17 | rooms 7, 24 | 1461 | 391 | 1852 | +888 |

Earlier tables (max HP 3, small potions allowed): route 1 (32 31 30 22 11 17 ...) 1023, route 2 1024, through 22 into
room 7 without drinking 1094, with its potion 1147, route 2 + room 24's potion 1537, route 1 + both 1952 (first pass,
two chunks split at room 17: 1056 / 1056 / 1292 / 1358 / 1537 / 1984). Under the new rules route 1 by 30 22 11 no
longer reached room 2 in 900 steps (worn down to 1 hit point by room 12) and was not pursued.

Failed or not found:

- **Route 2 + both** (... 5 10, the head following, 11, its push through the wall or the gate into 22, 7): room 7
  is reached only from 22, and 22 from route 2's side only through room 11: through its top row's gate (one tile,
  shut, opened from 22's plate; 22's top-row floor behind it; the middle row has four wall tiles instead). Room 10's
  head follows him into room 11's bottom-right corner, his only way in, and is on him at once: climbing past it costs
  bites, and knocked away by his sword it freezes inside the bottom-row wall (cols 0..7), never to come up. Probes
  from room 10 (30 GB, 800 steps; the Opponent Pos X / Y Magnets drawing the head to the top row beside him) found no
  push: with the head free he stays pinned in the corner.
- **Room 10 -> 5 by the sword alone** (Sergio's idea, no head): not found in 300 ticks (30 GB) from room 10's bottom
  row with no hit point lost; the head's bite does it (a hit point, refilled by room 24's potion).

Level notes:

- **No way through room 2's left wall:** Sergio's routes ended 29 -> 2 -> (the head's push into room 8's middle row,
  on the exit's plate). The push works from the middle row: placed there (a test only) at x 150, facing right, sword
  out and striking, the head's bites carry him through the wall onto room 8's col 6..7 and the exit opens. But nothing
  gets him up there with the head able to push: it is on him ~17 ticks after he climbs in from room 29, a bite during
  the ~30-tick climb is fatal (even at full health), a sword hit holds it off ~10 ticks (sequence 0x99; the lasting
  retreat after a hit (`heads.c`) needs a second character in the room), and the one climb the search found (the head
  hit and knocked into the left wall) leaves the head frozen there for good. The routes take the canonical way round:
  2 (the head killed, up the col 0 pillars) 16 28 20 (down its gap at col 7) 21 8 (the plate) 21.
- **Room 31 -> 7** (Sergio): along room 31 to its middle row's floor (col 8, over the top row's gap onto the big
  pillar) and through the wall (31's col 9, 7's col 0) into room 7's middle row: 59 ticks faster to room 2 than by
  30 22 11, and room 7's potion is then on the way (+63).
- **The level door:** inside it (frame 0, x 441, y 93) he must wait with nothing pressed until level 8 loads (~10
  ticks); a key latched during the entry walks him back out. Three script fixes: the door step pulls into the door
  (the step before scored the floor in front of it higher), the clock is hashed during the door sequence (0x46: the
  wait inside it was dropped as a repeat), and the small-potion fails hold in level 7 only (level 8's tiles replace
  those on the tick it loads: the fail beat the win). A small HP magnet (50) breaks ties: without it the endings came
  to room 8's blades at 1 hit point and waited them out (437 against 391).
- **Room 29:** he comes out of room 3's middle-row wall (the 6 -> 3 -> 29 clip) in sword stance: he sheathes (~22
  ticks) and climbs at once, twice, into room 2. The long runs drew him to col 1 in sword stance before sheathing (a
  sideways pull that outscored standing still), ~40 ticks; noticed by Sergio in the videos, fixed by weakening it.
- **Room 31:** off the top row's right end at col 6 (a hit point); col 5's tile (0x0E) is no floor in this level
  kind: falling there goes on into room 30 and kills.
- **Room 12's hole** (cols 2..5): the running jump from room 17 falls short; Shift (and Up) catches col 6's edge as
  he drops past it and he climbs up (as level 6's potion).
- **Into room 7 from 22** (route 1 + potion): only through two loose floors bumped down from below (22's top row at
  col 2, then 7's bottom row at col 1); the floor above 22's top-row right end is wall.
- **Room 11:** route 2 comes in at its bottom-right corner (walls left of it, cols 0..7): up to the middle row there
  first. Room 10's head follows him into room 11 and bites.
- **Searches:** room 2's head moved the scripts to the HP magnet; a scoring fix lets the search keep hangs that start
  in the room below (room 10 under room 12's hole); an Opponent Pos X Magnet (`princeOfPersia2.hpp`) was added for
  the push attempts.

## Levels 8..14

Not routed yet.
