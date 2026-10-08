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

All three play to level 8 in Chimera's core on the predicted frame (MT-32: 5200, 5263, 6088). The real chain reaches
level 7 with 3/3, so the videos poke max HP 5 before level 7's first step (run-native `--poke 4237:Kid.HP=5` and
`Kid.Max HP=5`, the routing assumption); the first two also play through at 3/3 unchanged, the third needs the
hit points.

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

## Level 8

Rooms (the ruins): 15 on top; 7 13 16; 26 25 23 24 1 9 21 19 10 5 4 8; 11 22 20 12 2 3 14; 6 17 18 below. Level 7's
route starts level 8 in room 16 (row 1, x 296) with max HP 5 (`"Max HP At Level": [8, 5]`, as level 7); the small
potions (rooms 8, 20, 24) fail. The exit: room 23's door (row 2 cols 1..2), opened by the plate at its col 9. The
heirloom sword: room 9 (row 2 col 5); the first pickup plays the sword scene (sequence 0xED, story scene 6, the level
reloaded: `ruins.c` sword_scene), the second takes it. The big potion: room 18 (row 2 col 8). Ticks count from level
8's first tick to level 9's (room 10, x 328).

| Route | Big potions | Ticks | vs best |
|---|---|---|---|
| 16 13 4 3 (the plate past the cutter, back through room 2's gate) 2 12 20 22 11 9 (the sword) 1 24 23 | none | **817** | - |
| 16 13 4 3 2 12 6 17 (top row: the loose floor dropped on the gate's plate) 18 (the potion) 17 (up) 6 12 20 22 11 9 1 24 23 | room 18 | 1344 | +527 |
| 16 13 4 3 18 (the potion) 17 (its gate) 6 12 20 22 11 9 1 24 23 | room 18 | impossible | - |

The 817 (`0800/np.sol`, 60 GB) plays to level 9 in Chimera's core tick for tick (815 level-8 ticks compared, the
scene's aside); `0800/level0102030405060708.sol` chains it after levels 1..7 for level 9. The 1344 (`0800/pot2.sol`,
60 GB, Sergio's second potion route) plays the same (1342 compared; max HP 6 from room 18).

- **Room 18's potion (Sergio's second route):** past room 2 into room 12, down its shaft (cols 6..7) catching room
  6's top-row pillar (col 8), across to room 17's top row (its col 0 and room 6's col 9 are empty), onto the loose
  floor (col 2): it falls two rows onto the plate under it, which sticks (tile 0x0E: `mobs.c` mob_land) and opens the
  gate for good (stuck open, 0xFF). Down, through the gate, the cutter and the tunnel into room 18, over its closer
  (col 7: a closer slams even a stuck-open gate; the script fails on it) onto the potion's tile (a pickup reaches the
  tile under him or the one in front: from in front he would stand on the closer), and back over it. Back through the gate,
  up onto the row-1 gate tile (col 3, from col 2 facing right), up onto the top row's closer (the gate no longer
  matters), left along the top row into room 6, up into room 12 (its col 8 floor, from room 6's pillar). In the 1344:
  room 17 at tick 280, the potion at 538, room 12 again at 780, then the common way (564 ticks to level 9).

- **The sword scene:** skipped by a key in Chimera (Space on the scene's first frame, the frame after the 0xED
  sequence's last), it takes 88 movie frames for the one headless tick, so the movie runs 87 frames longer than the
  tick count (watched, it runs past 2090 frames). The same on every route: the tables count ticks. Skipped, the scene
  returns 2 (`drawn_room` cleared, room 9 not reloaded, the abort's `sound_stop_all`); the headless core plays no scene
  (result 1). Both play on identically here (positions tick for tick; no random draw in that stretch), so the core's
  skip mode needs nothing for it.
- **After the level:** a scene follows level 8 (a daughter scene, SDLPoP2 aee421a); the Chimera core built here
  (SDLPoP2 a575569) still plays the horse ride there. Either is skipped by Space; renders want the core updated.
- **Room 18's potion by room 3's shaft cannot be had** (Sergio's first route and doubt): room 18 is reached down a shaft from room 3 (its middle row has
  no floor: no way back up) and leads out only west, crawling through the tunnel (cols 0..2, then room 17's cols 8..9)
  to room 17's gate (row 2 col 3, shut). Its plates: row 2 col 2 (behind the gate) and the ledge at row 1 col 8, over
  the tunnel's end, grabbed only from col 7 facing right: the cutter's right half (0x0D; the blade spans cols 6..7,
  `blades.c`). The jump-up grab puts him at x 393 (col 7, past the blade's middle) for its first 12 frames (67..78), so
  the blade starts (if idle) and closes 2 ticks later: dead. Standing on it, it runs a 20-tick cycle, closed at 2 (its
  whole box), 16 (the upper half) and 17 (the lower); its box (y 152..159) cuts every jump frame and the hang. A grab
  survives only if its first frame comes at blade state 3..5 (frames 79, 80 and the hang are in col 8, out of its
  reach), and he cannot be standing ready on col 7 then: state 2 cuts anyone standing there, and from crouching or
  crawling the stand-up takes 4 ticks. With the blades off (god mode) the grab hangs 14 ticks after Up and the gate
  opens. An exhaustive search from room 17's entry (no state dropped, 220 ticks, 11 blade cycles) found no opening. A
  restart (Alt+A) puts back the level's starting max HP, so drinking then restarting gains nothing.
- **Room 2's gate** is open at the start (200) and slams ~3 ticks after he comes over room 3 (from room 4): room 3's
  plate (row 1 col 9, over its cutter at cols 7..8) opens it again (4, 8 .. 196, then its timer from 250 down). The
  script's step is the plate's press (0x800 in its modifier while he is on it): on "the gate open" a fast search,
  in room 3 before the slam, counted the gate still open from the start and stalled at it shut (the 60 GB runs).
- **Searches:** the game's hash now takes the modifiers of every room with a tile animation running
  (`princeOfPersia2.hpp`): a blade's phase lives there, and states a phase apart had been merged (rooms 3 and 22
  have cutters on this route): 991 before, 931 after (10 GB), 817 at 60 GB with the plate step below. Level 7's route crosses cutters too (rooms 3 and 8; level 6's
  are in rooms 19 and 21), searched before this fix: they play no role there (Sergio), no rerun. The steps after the sword require it (the win rule reads the
  next level only, and a later step's rule marks the earlier ones done: the first rerun skipped it).

## Level 9

Rooms (the ruins, then the desert): 22 23 on top of 9 10; 16 15 14 13 12 11 2 8 6 4 5 1 9 10; 3 17 18 7 19 20 21 below
8 6 4 5 1 9 10. Level 8's route starts level 9 in room 10 (row 1, x 328), max HP 5 (`"Max HP At Level": [9, 5]`).
The level ends on room 8's top row: running left off it (frame 0x28) across its left edge sets the next level
(`room.c` kid_exit_dir) and the horse ride follows. The big potion: room 13 (row 2 col 3, the desert); small ones
(rooms 9, 5, 4) are never drunk (a drink, sequence 0x4E, outside room 13 fails: not their tiles, since room 9's loose
floor past its gap falls on its potion). Ticks count from level 9's first tick to the level's end.

| Route | Big potions | Ticks | vs best |
|---|---|---|---|
| 10 9 1 (up) 5 4 6 8 (the top row) | none | **373** | - |
| 10 9 1 (down) 5 4 6 8 2 11 12 13 (the potion) 12 11 2 8 6 4 5 1 (up) 5 4 6 8 (the top row) | room 13 | 1080 | +707 |

The 373 (`0900/np.sol`, 20 GB) plays the same in Chimera's core (373 ticks compared) and is rendered
(`level9-no-potion.mp4`, the core rebuilt on SDLPoP2 3e2bf50: no scene after level 8 this time, the horse ride after 9).
The 1080 (`0900/pot.sol`, 60 GB) too (1080 compared; `level9-potion.mp4`): the potion at tick 332, two bites in room 6
on the way back (6 -> 4 hit points), the sword drawn in rooms 6 and 4 only (ticks 507..587 and 633..658), up in room 1,
the top-row path unbitten, the level left with 4/6.

- **Room 8's heads (the potion route):** going west and back on the bottom row wakes room 8's two heads (charid 7),
  and a woken head stays hunting in its room record while he is away: at the end, back on room 8's top row, they
  come up at him under the ledge he lands on from room 6 (the first searches died there). Sergio's way: on the way
  back wait a little in room 8 so that the heads come to its bottom left, then run on right. The script's "Room Heads Magnet" draws them there (`princeOfPersia2.hpp`: their live slots in the
  room, their records after); a lost hit point fails, and so does a drop from the top-row path once he has stood on
  room 1's top row (a hang from its ledges after a short jump stays allowed). The game options "Hash Room
  Characters" (the room records) and "Hash Stood On Rows" (the drop fail's flag) are on for this route only: hashing
  the records multiplies the states, and with them the 20 GB no-potion search lost its way.
- **The heads follow him (the way back):** woken going west (rooms 8, 4, 5), the heads hunt him from room to room and
  fly faster than he runs; with no sword two of them catch him in room 5 (cols 7..9) and their bite chains (skill 8 /
  9: 2 bites certain, up to 3 / 4, `heads.c` head_bite_lands) kill him from 6 hit points: a probe over every input
  from his return into room 6 found no way through, bites allowed or not. Sergio's fix: the sword on the bottom row
  east only, to stun them (knocked back), sheathed before the climb (it fails on the top-row path and going west);
  bites allowed there down to 3 hit points, none lost on the top-row path.

## Level 10

Rooms along the route (the temple): 22 11 8 14 12 (19 below 12). Level 9's no-potion route starts level 10 in room 22
(row 2, x 310, facing right), the horse ride skipped (Space), max HP 5. Room 32's big potion is far out of the way:
not taken (Sergio). Ticks count from level 10's first tick to level 11's (room 5, x 296).

| Route | Big potions | Ticks |
|---|---|---|
| 22 11 8 (up to its middle row) 14 (across) 12 (its loose floor dropped) 19 (the door) | none | **298** |

`1000/np.sol` (60 GB) plays the same in Chimera's core (297 ticks compared, to level 11; `level10-no-potion.mp4`);
`1000/level01020304050607080910.sol` chains it after levels 1..9 for level 11.

- **The shortcut:** room 12's bottom-row loose floor (col 1) dropped falls into room 19 onto the plate under it (row 2
  col 1), which opens the exit door (row 1 cols 4..5); then right, down through col 7's loose floor (0x1A) onto room
  19's middle row, into the door as it opens (it reaches the open mark, 0x2A, ~41 ticks after the floor lands: the
  wait). Room 14: down to its bottom row's pillar (col 6), then across: a short jump hangs from room 12's floor
  edge, his body in room 19, and climbs. Timeline: room 8 at 46 (up to its middle row: its bottom row is walled at
  col 2), 14 at 115, the hang 163..184, room 12's floor dropped at 204 (it lands on the plate at 226), room 19 at 230,
  the door open enough at 267, level 11 at 298.
- **Searches:** the first ran 448 (20 GB), 435 (60 GB): room 19, which the hang under room 12 enters, was penalized
  as a room ahead until its own step, so the search kept him in room 14 for ~100 ticks; its step now draws him up there
  instead (no penalty). Room 11's guard (charid 2) is spawned as he passes (room.c spawn_ok: never on a tick that is
  a multiple of 3).
- **Chimera movies after a skipped story scene:** the next level's first input goes on the new level's first frame.
  The headless line that ends a level also loads the next one, so after the horse ride the first level-10 tick is the
  line after it; a movie one frame late moves him a tick late against the game's tick counter, and the guard's spawn
  (on the counter's multiples of 3) comes at another moment (seen as a bump into its sword in room 11). After level 9
  here: Space on the frame after level 9's last, 24 blank frames, then level 10's inputs.

## Level 11

Rooms (the temple): 22 21 32 / 28 27 20 29 / 26 24 19 / 25 18 17 / 30 23 13 8 14 / 31 6 7 12 16 / 10 9 3 15 / 4 2 /
11 1 5 (rows top to bottom). Level 10's route starts level 11 in room 5 (row 2, x 298, facing left), max HP 5. The
big potion: room 3 (row 2 col 7). Sergio: the potion is quick, so only the potion route is routed. Ticks count from
level 11's first tick to level 12's (room 19, x 200).

| Route | Big potions | Ticks |
|---|---|---|
| 5 1 2 4 10 9 (jump, short) 2 (its right ledge) 9 3 (the potion) 12 7 6 13 18 17 (its plate) 18 13 23 25 18 24 (its plate) 26 28 27 22 21 32 29 (its plate: the exit door) 20 27 28 26 24 | room 3 | **2445** |

`1100/pot.sol` plays the same in Chimera's core (2444 ticks compared, to level 12; `level11-potion-v2.mp4`, with the
room in the overlay); `1100/level0102030405060708091011.sol` chains it after levels 1..10 for level 12. Searched in four
chunks: start -> room 3 482, room 3 -> 12 272 (Sergio: the loose floor broken first, the potion while it falls; 277 the
other way round), room 12 -> 23 495 (room 7's stall gone with the routegen fix: room 6's step had kept the -5000), room
23 -> the end 1196. The first complete solution was 2645. Room 27 takes 168 ticks either way (a focused search finds
no less): he climbs the col 6 pillar and must jump from x 338 facing left to knock room 22's loose floors down (from
x 348, where the climb leaves him, the jump misses), so he climbs down and up again to get there.

- **What the potion costs** (2026-10-07; Sergio: measured between two points common to both ways): from standing on
  room 9's middle row after the last climb (tick 466 of the route) to the first tick in room 12, **272 with the potion,
  110 without: 162 ticks** (`1100/a466.sol` to there, then `1100/mpot.sol` / `1100/mnp.sol`). Both knock the loose
  floor down first (ticks 20..33, gone at 44); without the potion he waits under it and climbs (it costs him a hit
  point as it falls: 4/5), with it he clips right, drinks (6/6 at 114) and comes back to the same climb at 218.
- **Room 9 -> 3** (Sergio): a running jump right from room 9's ledge falls short into room 2 from the top; catch its
  right ledge (col 8), climb back into room 9's right side, on into room 3.
- **Room 3** (the potion): through row 1's wall at col 2 by a wall clip, down at col 7 onto the potion; back the same
  way, the top row's loose floor (col 1) broken from below (it sticks on the plate under it), up into room 12 at col 0.
- **Gates:** room 12's top-row plate (col 4, the cutter right of it) opens room 6's gate (the way from room 7); room
  17's plate opens room 13's left gate for ~140 ticks (17 -> 18 -> down 18's hole -> 13's middle row -> the gate:
  searched as one stretch, the chunk boundary had him late); room 24's plate opens room 26's; room 29's (down at col 7
  first: its top row opens only there) opens the exit door; room 1's plate at the start opens room 20's gate, and
  again: col 6's loose floor in room 20, run over, falls all the way down onto it.
- **Room 20** (Sergio): a running jump left across the bottom row's gap (cols 2..5), caught at the far ledge (the
  hang counts as room 19, below: the script allows it), climbed; wait for the gate; left.
- **Room 23:** a guard (and a spawn point) by the climb at col 8: he climbs past it.
- **Searches:** the script's steps with no x target kept the -5000 a room ahead had been given (fixed in routegen:
  such a step clears its own room's magnets); the sword stance does not leave a room (`room.c` kid_exit_dir ignores
  its frames). The render crashed past ~64 different sounds (SDLPoP2's sound cache moved under the players' pointers:
  fixed in SDLPoP2 f528cfd).

## Level 12

Rooms: 4 21 / 2 5 1 3 11 13 / 10 9 8 19 7 12 20 / 17 6 16 15 14 22 18 24 23 25 26 (rows top to bottom; 4 above 5, 21
above 3). Level 11's route starts level 12 in room 19 (row 2, x 200, facing left), max HP 6. Sergio: the big potion is
far out of the way, not taken. The exit door: room 7 (row 2 col 3), opened by room 14's top-row plate (col 8) or room
24's (row 2 col 3). Ticks count from level 12's first tick.

| Route | Big potions | Ticks |
|---|---|---|
| 19 7 (wall clip) 14 (its plate: the exit door) 22 (down its left column) 14 (bottom row) 15 16 6 17 (room 10's loose floor broken from below) 10 9 (its top row) 8 (down its hole, sword clip through the gate) 19 7 (the door) | none | **735** |
| ... 14 (its plate; a sword wall clip left through its top row) 15 ... | none | 745 |

Sergio's route. `1200/np.sol` plays the same in Chimera's core (735 ticks compared, to level 13: room 3, x 264, 6/6);
`1200/level010203040506070809101112.sol` chains it after levels 1..11. Room entries (tick): 7 38, 14 90 (the plate),
22 116, 14 146, 15 206, 16 233, 6 260, 17 322, 10 460, 9 488, 8 588, 19 653, 7 702. The 745 run (`level12-745.mp4`):
7 37, 14 91, 15 169, 16 250, 6 275, 17 332, 10 470, 9 499, 8 598, 19 663, 7 712.

- **Shortcut** (Sergio): right into room 7 and through its bottom row's wall (cols 6..7) by a wall clip, down the col
  8 hole straight onto room 14's plate (the exit door opens at tick 91): a run into the wall, the sword drawn, turned
  to face left, a sword backstep carries him through the wall.
- **Room 14 -> 16, two ways:**
  - The clip left (745): the plate's row is wall left of it (cols 0..7): a sword wall clip (sheathing inside the wall,
    the game pushes him out tile after tile) into room 15's shaft, a 2-row fall (a hit point, a ~42-tick hard
    landing: on the floor at 179, standing at 221), room 16 at 250.
  - Down through room 22 (735): off the plate's ledge to the right with the sword's steps, the ledge caught, a 1-row
    drop (no damage), left along the bottom rows (room 14's cutter crawled under): room 16 at 233, full hit points.
- **Room 16** sets the pace of both: its gate and room 6's start open (0xC8); its closers (row 2 cols 6 and 1) shut
  them; its plate (col 3) opens room 6's further (0xFA). The crossing is two chained running jumps: over the col 6
  closer onto the plate, over the col 1 closer through room 6's gate in the air (25 ticks). Off that rhythm he lands on
  a closer (~50 ticks for the gate) or short of the gate, through it sword in hand (the first room 22 runs: 787). The
  room 22 run is brought onto the rhythm before room 14's cutter (x 417 between the jumps at tick 235, the 745 run's
  x 416 at 252): 17 ticks ahead there, 10 at the end (room 6's blades).
- **Room 17:** up the col 5 shaft by the gate tiles' ledges (col 6, three climbs), room 10's loose floor (its ceiling)
  knocked down from the top (facing left), then up: 138 ticks.
- **Room 8** (Sergio): its top-row plate is not worth it (the closer under the hole shuts the gate again): down the
  hole, a sword clip through the gate.
- **Room 19** (Sergio): the sword put away at once, then a run. Unrewarded, the search walked the whole room sword in
  hand (the sheathing gains no ground for ~20 ticks and was pruned): 903 ticks; a rule for the sword put away fixed it.
- **Searches:** a chunk ending on "first into the next room" fixes the jump rhythm there; rewarding a room's entry
  ranks every state in it above the ones still behind (pruned): for the rhythm search room 16's magnet continues
  room 15's instead. The 745 run's inputs do not replay after the room 22 opening (1 px off: he dies in room 6).
- **Movie:** level 12's first input goes on the row after its first frame (that row loads the level and runs no tick
  of it, as at level 11's start).

## Level 13

Rooms: one row, 4 29 13 28 2 32 19 11 10 9 20 3 (8 right of 3; 5 under 3, 16 17 18 22 under 9 10 11 19, 15 under
13, 24 over 19). Level 12's route starts level 13 in room 3 (row 1, x 264, facing left, 6/6). Room 4 holds the flame
(row 1 col 1); its plate (row 1 col 2) opens the three exit doors (room 28 row 1 cols 5..6, room 32 row 1 cols 1..2
and col 5). The big potion: room 29 (row 1 col 6), on the way. Ticks count from level 13's first tick to level 14's
(room 1, x 306, 7/7).

| Route | Big potions | Ticks |
|---|---|---|
| 3 (its loose floor: room 20's gate) 20 9 10 (its plate: room 19's gate) 11 19 32 2 28 13 29 (the potion) 4 (its plate, alive) 29 13 28 (the door) | room 29 | **775** |

Sergio's route, after level 12's 735 run (two chunks: to room 11 220, the rest 555; not yet optimized).
`1300/pot.sol` plays the same in Chimera's core (775 ticks compared; `level12-13.mp4`);
`1300/level01020304050607080910111213.sol` chains it after levels 1..12. Room entries (tick): 20 98, 9 139, 10 166,
11 217, 19 281, 32 353, 2 384, 28 409, 13 434, 29 464 (the potion drunk by 490), 4 534, 29 643, 13 668, 28 729.
The first solution, after level 12's 745 run, took 778 (`level13-778.mp4`); its inputs do not replay after the 735
run (off in room 19, dead in room 32), though the levels start alike: searched again from room 11.

- **Room 3:** room 20's gate (row 1 col 9, at room 3's left end) is opened by room 5's plate, under room 3's loose
  floor (row 1 col 7): right onto it, it falls onto the plate, back left through the gate: 100 ticks. (Straight left
  the search dropped the same floor from afar by three jumps' knocks: 140.)
- **Room 19:** from room 11's left end up onto its top row, over the shaft to the middle row's left end, through the
  gate room 10's plate opened: 76 ticks.
- **Room 4, the flame** (`shadow13.c`; measured with `flame13.c`, the prince flown into the room by the fly cheat):
  - A prince on the middle row at x <= 218 burns (sequence 0xE6, f24 0xC): a plain death, no shadow (900 ticks
    waited: the restart countdown runs out). His body presses the plate.
  - Killed there by the guard instead (one strike when his sword is not drawn), the shadow rises once the death wait
    is over (+129 ticks), jumps the pit, takes the flame (its landing presses the plate, +172), comes back and gives
    him his life back (+301: full hit points, f24 0xD: guards no longer attack). From standing two tiles right of the
    guard to back in room 29: 342 ticks.
  - The plate is an ordinary one: a live prince on its tile at x > 218 (the tile reaches x ~241), or hanging from
    its ledge, presses it. The exit door asks only to be open; the flame state is reset at each level's start.
    The route: a running jump into the pit, the plate's ledge caught (the plate pressed from the hang), up, turned,
    a standing jump back, the far ledge caught, up: 109 ticks in the room (534..643), no death.
- **Way back:** room 13's pit is 4 tiles once its loose floor fell (on the way in): jumped, the far ledge caught. The
  first search landed on the bottom row instead (cols 6..9, on into room 28's bottom row): a dead end under the exit
  door; those landings fail now.

## Level 14

Rooms: 7 8 (10 above 8) / 4 5 6 / 3 9 / 1 / 2 (columns: 4 over 3 over 1 over 2; 7 over 6). Level 13's route starts
level 14 in room 1 (row 1, x 306, facing right). The prince in room 2, not falling, his right side at x <= 305
(`final.c`): the scene with Jaffar at the window, the level loaded again, the prince in room 4 (row 1 col 5, facing
right, the sword drawn).

| Stretch | Ticks |
|---|---|
| 1 (down off the ledge's right end) 2 (crawling left: the scene) -> room 4 | **51** |

`1400/scene.sol`: a safe step right to the ledge's end, a turn, down its edge, a 1-row drop onto room 2's floor, a
crawl left to x 286. The search's database never filled: the fastest with these inputs. Chimera: ticks 1..50 compared;
the scene after level 13 is skipped by Space (level 14's first frame 22 movie rows after level 13's last input, its
first input on that row), the Jaffar scene by Space on the row after the trigger (room 4 appears 11 rows later).

### What the flame needs

11 hit points when the eight turns start (`spirit.c` turn_count, `final.c` spirit_cast; the drain measured): turns
4..8 cost a hit point and a point of the maximum each (6/6; the eighth needs more than 4 left), the body settling 3
more (3/3), a fireball 2 and at least 3 to cast (1 left); one fireball on Jaffar's row kills him. With 10: 2/2, no
cast. The run needs 8 big potions in all (3 at the game's start).

### Room 3's guards (the big potions of level 14)

Room 3 is under room 4: off the left end of room 4's floor, down the shaft at col 2, two ledges caught on the way (no
damage): the bottom row at tick 45. Back up: four climbs at col 3 (each from under its ledge, x ~250, facing right).

- At most two guards or bodies at a time; a body goes ~96 ticks after the death. A guard appears on the row (middle or
  bottom) the prince is not on: the first always on the bottom row, another with 1 chance in 21 a tick. 3..5 hit points.
- 6 ticks after a death the game draws what the body leaves on its tile: nothing, a small potion, a big one (1 in 3
  each; only on a plain floor tile, not the bottom row's cols 2..4).
- The search (`1400/gen.py` route `farm`, `1400/farm_table.sh`): the random seed is part of the state, a kill that
  leaves no big potion fails at once (Sergio: no kill for nothing, on the way back up either), so does one more kill
  than needed; rewards: the damage on both guards, the kills, a small pull to the nearest guard (Sergio), a pull to
  the potion, then 60 a pixel of height for the way back.

From 3 hit points at room 4's start (ticks from there; each potion from the first state that reached the one before:
estimates, not proven optima; `1400/farm3/`):

| Maximum | Reached at | This potion | Back on room 4's middle row at | The way back |
|---|---|---|---|---|
| 4 | 117 | 117 | **324** | 207 |
| 5 | 260 | 143 | **411** | 151 |
| 6 | 354 | 94 | **613** | 259 |
| 7 | 491 | 137 | **638** | 147 |
| 8 | 594 | 103 | **816** | 222 |
| 9 | 696 | 102 | **847** | 151 |
| 10 | 787 | 91 | **1009** | 222 |
| 11 | 906 | 119 | **1057** | 151 |

He alternates rows: a potion on the bottom row (a drop down: ~95), the next on the middle row (a climb first:
~120..143). From the middle row the way back is three climbs (~150); from the bottom row four, past the middle row's
guard, who is fought off but not killed (207..259). So an even number of potions is the cheap one: 2 cost 411, 4 638,
6 847, 8 1057; an odd one more costs 160..200.

### The ending: the flame, room 6's Jaffars, Jaffar (2026-10-07)

From room 4's middle row at 11/11, in segments (Sergio), ticks from there:

| Segment | Ticks |
|---|---|
| The eight turns on the spot: the spirit leaves the body (3/3) | 43 |
| 4 5 6: three of the four Jaffars cut down, the fourth leaves; up into room 7 | see below |
| 7 \| 8: Jaffar struck by the fireball; the level is won 4 ticks later | see below |

By where the spirit comes up into room 7 (its bottom row; room 6's top row has a left platform, cols 1..3, under room
7's gap at col 3, and a right one, cols 6..7, under the gap at col 8):

| Entry | Room 6 to standing in room 7 | Fight to the hit | Sum |
|---|---|---|---|
| Right platform, onto room 7's col 7 (facing left) | 277 | 205 | **482** |
| Left platform facing left, onto room 7's left platform (col 2) | 290 | 210 | 500 |
| Left platform facing right, onto room 7's middle platform (col 4; Sergio) | 300 | 199 | 499 |

**The right entry: 43 + 277 + 205 + 4 = 529 from room 4 to the win** (`1400/end3/`: `e1.sol`, `r7R.sol`, `fb8R.sol`;
Chimera: the level's 1639 traced ticks equal the headless replay, the final scene plays; `level14-ending-right.mp4`).
The three fights are the same from the middle row on (all go up at col 5, x 329, onto the middle row's col 6): the
right entry is on it at 42, the middle platform at 36, the left platform (a standing jump over the gap onto the
middle one) at 46. The entries differ by more: the left platform's comes 13 ticks after the right one's, the middle
platform's 10 more (the spirit comes onto room 6's left platform facing left: a turn under the gap, 8).

- **Entry searches end on stray keys:** the game keeps the last keys pressed in a climb, and a search that wins on
  the first standing frame pressed anything there. The middle platform's first fight began with two turns (a stray
  Left + Up), 209 instead of 199: `fightk.sh` TRIM=10 starts the fight 10 ticks before the entry's end (the right
  entry's is 205 either way, the left platform's 210; `end3/fb8C_t10.sol` starts 10 ticks before `r7C.sol`'s end).
  Its first search also cut down the bottom row's Jaffar and climbed two rows back (321): the bottom row is failed
  there as for the other two (300).
- **Room 6:** its Jaffars strike only the prince in his body, so the turns are made at once in room 4. One cut each;
  the search must see the fourth's leaving to its end (room 6's list empty) or no Jaffar waits upstairs.
- **Jaffar's round** (`final.c`, rooms 7 | 8 as one row of 20 columns): four waypoints, 0 room 8's top row col 7, 1 room
  8's bottom row col 4 (under its middle row's hole), 2 room 7's bottom row col 5, 3 room 7's top row col 6. Standing
  at one he leaves it as the spirit enters its zones: at 0 the top row (cols 1..18) sends him to 1, the middle row's
  cols 15..18 to 3; at 1 the bottom row to 0, the middle row's cols 14..18 to 2; at 2 the middle row's cols 2..9 to 1,
  the bottom row to 3; at 3 the top row to 2, the middle row's cols 6..10 to 0. On the way he does not look.
- **The fireball** (measured, `tools/probe.sh`, `tools/trp.c`): C casts (2 of the 3 hit points; the cast's first frame
  can be walked out of, the points gone); the fireball appears 11 ticks later, flies 10, 12, 14, then 16 px a tick
  along the spirit's row, over gaps, to the first wall. Its box starts 35 px ahead of where the spirit stood: Jaffar
  is hit only from 35 px or more on his left (27 or more on his right); from nearer it appears beyond him. The level
  is won 4 ticks after the hit (his death's frame 0x15F).
- **Why not at waypoint 1:** the top row sends him down (off it 6 ticks later: no shot there); he stands at waypoint 1
  49 ticks after, and the spirit coming onto his row sends him back up within 30 (a turn 8, a jump 15, a climb 6). The
  only way down there is the hole over him: a landing (13 ticks to stand; a wall bump makes it 9, there is none), then
  the spacing and a turn, then the cast's 11. Not enough, from either side (the first plan: 520 ticks, no kill).
- **The way (plan B):** up to room 8's top row from its middle row's col 2 (the climb's seventh tick sends him down),
  right along the top row, down its gap at col 8 onto the middle row's enclosed right part: he reaches waypoint 1 with
  the spirit in that zone and runs on to waypoint 2, the whole bottom row, then turns round there (8). The spirit runs
  left, down the hole behind him, stands (180), casts (181): the fireball (192) reaches him at waypoint 2 (205).
  Once the spirit's cast is over (1 hit point) Jaffar hunts: on the spirit's row he casts back, and the spirit dies
  11 ticks later (here he began at 199: 5 ticks to spare).
- **Searches:** the fight's state space fills the database (8M states by tick ~70, the seed not hashed), so the plan
  is in the route's steps (`1400/gen.py` routes `fb8`, `fb`; `fightk.sh`), each carrying Jaffar's place: he must have
  left waypoint 1 leftwards before the spirit goes down the hole (his turning round there fails). A fireball-to-Jaffar
  pull (Sergio; "Set Fireball Magnet") ranks the shots in flight. Steps that name no room need every magnet "keep".
  With room 7's top row allowed too (route `fb`: its col 9, over the gap, is reached 11 ticks sooner) the search
  still goes up in room 8: 205 again. Every fight ends alike (the spirit at x 268 on room 8's bottom row, the cast at
  181 from the right entry): the entry only moves the time the middle row is reached.

## The global potion plan (2026-10-07)

Eight big potions are needed (3 -> 11). With l of them from the levels and n = 8 - l from level 14's room 3 (its
table above: n potions and the way back cost 324, 411, 613, 638, 816, 847, 1009, 1057 for n = 1..8), cheapest level
potions first (level 13 ~45, level 7's room 7 +63, level 5's potion 2 +260, level 3's potion 1 +276, level 3's
potion 2 +384 more, level 8 +527, level 4 +580, level 6 +667; level 11's, measured since: +162 (with levels 13 and
7: 270 + 816 = 1086, no better); levels 10 and 12: far, not counted (Sergio)):

| From the levels | Their cost | From room 3 | Its cost | Total |
|---|---|---|---|---|
| none | 0 | 8 | 1057 | 1057 |
| 13 | 45 | 7 | 1009 | 1054 |
| **13, 7** | **108** | **6** | **847** | **955** |
| 13, 7, 5 (potion 2) | 368 | 5 | 816 | 1184 |
| 13, 7, 5, 3 (potion 1) | 644 | 4 | 638 | 1282 |
| ... and 3's potion 2 | 1028 | 3 | 613 | 1641 |
| ... and 8 | 1555 | 2 | 411 | 1966 |
| ... and 4 | 2135 | 1 | 324 | 2459 |
| ... and 6 | 2802 | 0 | 0 | 2802 |

**The plan: level 7's room 7 potion, level 13's, six from room 3** (5 -> 11, ending on its middle row). Two farmed
potions cost ~210 (a pair: one on each row), so a level's potion pays only under ~105, or two under ~210: levels 13
and 7 together cost 108; the next cheapest (260) saves 31. The plan holds unless the room 3 numbers are off by a
factor of two. To do: level 7 with its potion in the chain (1027), levels 8..13 searched again after it with the hit
points the run really has (3, then 4 from level 7; they were searched with 5), level 11 without its potion, level 14's
room 3 from 5.

## The global route (2026-10-08, the exploration closed)

Ticks in play (12 a second). Estimates: the levels after a change replay differently (the random numbers, the hit
points), so each is searched again in the chain.

| Level | Route | Max HP after | Ticks | Running total |
|---|---|---|---|---|
| 1 | Restart Level at the checkpoint | 3 | 433 | 433 |
| 2 | The sand puzzle on its best seed | 3 | 102 | 535 |
| 3 | Left exit (1 5 7 8 11 13) | 3 | 476 | 1011 |
| 4 | Slow fall, the top-row slide | 3 | 592 | 1603 |
| 5 | No potion: round the bottom, the bridge, the carpet | 3 | 1436 | 3039 |
| 6 | The sword, the head's push into room 7, left door | 3 | 426 | 3465 |
| 7 | 32 31 (the wall) 7 **(its big potion)** ... 2 16 28 20 21 8 21 | 4 | 1027 | 4492 |
| 8 | No potion (16 13 4 3 2 12 20 22 11 9 1 24 23) | 4 | 817 | 5309 |
| 9 | No potion (10 9 1 5 4 6 8, the top row) | 4 | 373 | 5682 |
| 10 | The shortcut (22 11 8 14 12 19) | 4 | 298 | 5980 |
| 11 | The route without room 3's potion | 4 | ~2283 | 8263 |
| 12 | 19 7 (clip) 14 (the plate) 22 14 15 16 6 17 10 9 8 19 7 | 4 | 735 | 8998 |
| 13 | To the flame room alive, **room 29's big potion** on the way, room 28's door | 5 | 775 | 9773 |
| 14 | The scene 51, **six big potions from room 3** ~847, the ending 529 | 11 | ~1427 | **~11200** |

~11200 ticks, 15 min 33 s of play; in Chimera ~920 movie frames more (the boot, the scenes skipped, level 8's sword
scene). What is estimated, not measured:

- **Level 11** without its potion: the potion route's 2445 less the potion's 162 (measured between two common points).
- **Level 14's room 3 from 5 hit points:** the 847 is six potions and the way back from 3 (3 -> 9); it hangs on the
  random numbers (a potion 1 kill in 3, the guards' rows).
- **Levels 8..13 were searched with 5 hit points**; the run has 4 there (level 11's falling floor costs one, level
  12's route none).
- **Level 13** is two chunks, not optimized; level 5's routes from before the smoothed y may hold waits.
