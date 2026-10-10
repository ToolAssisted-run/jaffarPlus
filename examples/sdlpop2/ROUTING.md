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
- **The way back** from the plate is the bottom row: out of the plate's alcove (room 26, row 1 cols 7..8) by its left
  end, a row down, then running jumps over the downed skeletons through 28, 27, 25 and 24 to room 22's door. The
  scripts pulled up there after the door opens (for a way back along the top row, which there is none). 15 and 100 GB
  got through all the same; at 300 GB (12 M states) the database filled with the alcove's states, the falls scored
  63 less and were dropped: 300 steps in room 26, twice. Room 26 now pulls down and left, the rooms after it left
  only (`gen.py`). A magnet pointing the wrong way is hidden by a small database and fatal in a wide one.
- **Checking a script before a wide search:** `tools/rewardcheck.py SCRIPT SOLUTION` replays a known solution through
  the script and lists where its reward dips below what it had (the old 592 solution replays unchanged on the new
  core and chain). The old magnets: 41 deep for 29 steps from the alcove's exit, the fatal one; the new: gone. Left:
  36 for 47 steps while he drinks the slow-fall potion (step 117, early: the database is not full yet), and with the
  door's own magnets 49 over the stairs' 17 ticks (the first in scores less than those behind him): the entry step
  now pulls along the stairs, right and up (`routegen.py`; level 4's run had the old ending and got through it).
  What makes a dip fatal is a place to stand that scores above it, wide, and with something that keeps its states
  apart (level 4: the downed skeletons' timers, a draw every tick, the sword's moves): the database then fills with
  that place within a few steps (worst reward 6309 -> 12209 in six). A dip in mid-air, or with only the take-off edge
  above it, is harmless.
- **Watching a search:** `watch_level.sh TAG` ends when the search does, or when it stalls: 40 steps with the best
  reward no higher and the whole database within 200 of it (the two failed runs: called at step 449, 250 steps
  before they gave up; the good runs: never. The best reward alone stands still for 225 steps of the slow fall).
- **DOS, a dry run** (the stalled run's first 388 ticks and a short search's 207 from there; 596 with the level's
  end): both disks play it, room, x, y, frame and hit points equal on all 1527 ticks, level 5 loading on the same
  tick (the effect's hold included). The random seed is one draw ahead of the core's for one tick, twice: where an
  ambient piece ends (level 4's ticks 76, MT-32 disk only, and 366, both disks), DOS draws the next piece a tick
  before the core does. The pieces' ends are real time, a fraction of a tick either way; nothing else drew in the
  first of those ticks, and the drips' draw in the second changed nothing seen. Not a defect of the solution, but a
  draw by a guard in such a tick would differ: each level's DOS check has to look for it.
- **The run's level 4** (300 GB, `0400/script-slowfall-cut.jaffar`, 2026-10-08): **590** with its end (`tas/l04.sol`;
  the search's own lines `0400/l04b.sol`), the slow-fall rooms, 3 -> 2 hit points at tick 48 and 1 at 285. Up on line
  565, the stairs from tick 568, the level-end piece starts in tick 576, `M` on line 577, level 5 loads on tick 590
  (13 ticks after the `M`, as level 3: no effect held it). The old solution took 592 to the tick the level is decided,
  the dry run 596. DOS, both disks: every tick's room, x, y, frame and hit points equal (1521 compared: the chain and
  level 5's first ten ticks), level 5 loading after tick 1511 on both; the seed's one-draw transients are those of the
  dry run, on the same ticks (997 on the MT-32 disk, 1287 on both: the ambient pieces start with the level).
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
- **Before the final run's search** (2026-10-08): the old 1436 solution replays unchanged on the new core and chain,
  and `tools/rewardcheck.py` on it showed three places where the script pulled off the way: room 2 (the jump up
  that bumps the loose floor scored a point less a frame: no pull down now), room 4 (the magnet stood 60 px before
  the point he falls from: moved to it), and room 14, the one of level 4's kind: the gate at row 1 col 2 is shut and
  the sword frames' push takes him through it, backing off 46 px to the left first, 13 ticks scored down while the
  states at the room's entry kept the top. Room 14 now pulls left until the push throws him into the gate, has no
  magnet for that tick, and pulls to the potion once he is past it. Left: the two jump, grab and climb crossings
  (rooms 12 and 4, ~20 deep for 17 ticks, nothing standing above them).
  On the run's chain (`l01020304cut.sol`, another seed) the old solution still replays, level 6 loading on tick
  1436, with the same dips: the run's search is `script-none-run.jaffar` (tag `l05b`, launched 2026-10-08 13:54 UTC,
  after the first one's flood: the final run's section).

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
| 7 \| 8: Jaffar struck by the fireball; the level is won 4 ticks later (the final scene starts 7 after the hit: the run counts those) | see below |

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
  is won 4 ticks after the hit (his death's frame 0x15F); the game ticks 3 more before the final scene (the run's
  count: 7 after the hit).
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

## The final run (from 2026-10-08)

Each level searched again with its route above and a 300 GB database (`run_level.sh`, 12.1 M states), on the core
rebuilt from SDLPoP2's latest (e927d0f; quickerSDLPoP2 cf963ae): the exploration ran on SDLPoP2 3e2bf50, before the
level ends waited for their music and before the game's sound calls were checked against the code, the two things the
DOS check of levels 1..4 had found. Sound device: the Roland MT-32 (Sergio). In the Chimera core the game is the same
on either device; the MT-32's start-up takes 655 frames more.

- **Level 1's end on the new core:** level 2 starts at tick 538, not 433 (the music, 105 ticks; no random number is
  drawn in the wait). The old chain's level 1 with Random Seed 25 still gives level 2 its best puzzle (variant 0, right
  tile 4: 102 ticks); `tools/seedscan.c` finds such seeds for any level 1 solution (37 of the first 400 settings keep
  level 1; 15 puzzles, 3 variants by 5 tiles).
- **DOS (the oracle, `prince yippeeyahoo LEVEL1`, the seed poked at tick 1; `tools/orun.sh`, `tools/ocmp.py`):** level 1
  (433, the restart) on the Sound Blaster disk and on the MT-32 disk: 538 ticks to the story scene on both, every tick's
  room, x, y, frame, hit points and random seed equal to the core's (537 compared), the seed 901854189 at the end. The
  MT-32 disk loads level 1 653 frames later.
- **Chimera:** the core's runner built on the same SDLPoP2 (`run-native-e927`): 105 more ticks after level 1, then the
  story scene. The project file is written by the core's own `waterbox/tests/make-project.py` from a movie in the
  runner's letters.
- **What counts is the in-game time** (Sergio): the game clock, a tick of play at a time while the prince lives,
  counted from level 1's first tick (the game's own clock stays stopped until the story scene after level 4; the
  Chimera core's "IGT Ticks" adds the ticks before it, its `igt_from_level_1` setting, the game unchanged). So the
  searches' ticks are the measure; the boot and the story scenes cost nothing; the level ends' music waits count
  (Music On/Off during the piece ends them: the next section, which the run uses; this table is the run without it).

| Level | Ticks | IGT at its end | First row in Chimera | DOS (MT-32 and Sound Blaster disks) |
|---|---|---|---|---|
| 1 | 433 + 105 (the music) = 538 | 538 | 664 | every tick equal |
| 2 | **101** (was 102) | 639 | 1212 | every tick equal (639 ticks, the seed 1640912200 at the end) |
| 3 | **387** (was 476) + 94 (the music) = 481 | 1120 | 1380 | every tick equal (1120 ticks, level 4 loads) |
| 4 | searching (`0400/script-slowfall-run.jaffar`) | | 1872 | |

- **Level 1** at 300 GB: 433 again; the solution kept is the old one (`tas/l01.sol`), which Random Seed 25 ties to
  level 2's sand trick.
- **Level 2:** its fast solutions (101..107) do not hang on the puzzle's right tile but on a trick of the sand tiles
  that few seeds allow (the old 102 holds for 5 settings in 4000, with tile 4 in two variants; the plain solutions
  take 136..148 and hold for most seeds of their tile). A 40 GB search from the real chain misses it; 300 GB: 101
  (`tas/l02.sol`).
- **The copy protection** after level 2 (its question is drawn): the second symbol of the top row here (Space,
  Right, a free row, Shift); level 3's first frame is row 1380.
- **Level 3** at 300 GB: 387 (`tas/l03.sol`), 89 less than the 100 GB run's 476. Same rooms (1 5 7 8 11 13); in room 13
  it takes two hits from the skeleton on the bottom row (3 -> 1), presses the door's plate (row 2 col 2) with the sword
  frames' push and brings the skeleton down, then climbs to the door. The hit points cost nothing: a level starts at
  full health (game.c: `start_hp = Kid.f13`). The story scene after it: Space on row 1861, level 4's first row 1872.
- **The project:** `tas/pop2-tas.chimeraProject` (`tools/mkproject.py`: the released core package 4d44f96, MT-32,
  Random Seed 25, a marker a level), from `tas/run.mov`: 1872 rows, to level 4's first. The runner built from the same
  sources plays it to level 4 (IGT 1120 there), level 3's 480 rows equal to the search core's ticks. The headless
  Chimera built here is a month old and refuses the project's "id" key, which the current one knows: not yet played
  through Chimera itself.
- **Videos** of the final run: `~/pop2render/l8/render-run.sh NAME MOVIE START` on jaffanator2 (the runner on SDLPoP2
  e927d0f with the raw picture dump put back, `~/pop2render/gate-rawvideo.patch`; MT-32, Random Seed 25).

### Alt+M ends the level-end wait (2026-10-08, Sergio's question; the run uses it)

The game's Music On/Off key (Alt+M, 1611:07D4), switched off, stops the piece the MIDI channel took last, and while
the level-end music plays that piece is it: the level ends on the tick the next level is decided, with no wait. The
music has to be on when the piece starts; switched off before, the piece plays all the same and cannot be stopped.
DOS (1.1; the MT-32 and the Sound Blaster disks agree):

| Level | Alt+M at | The level ends after tick | Without |
|---|---|---|---|
| 1 | 400 (before the piece, queued at 430) | 538 | 538 |
| 1 | 400 and again at 445 (on) | 538 | 538 |
| 1 | 431 | **433** | 538 |
| 1 | 434 / 445 (in the wait) | 434 / 445 | 538 |
| 3 | 374 or 376 (on the stairs; the piece starts in tick 373) | **387** | 481 |

Alt+S (the sound) does not shorten the wait. SDLPoP2 only flipped the flag (so an earlier test here in the core saw no
effect): fixed in SDLPoP2 a5ee4ee (`music_toggle`, sound.c; local commit), the search core regenerated with the key
as an input (quickerSDLPoP2 f5f8763, local: `M` in the key's place of an input string, `|M|......|`), the Chimera
core's button is "Music On/Off" (the movie letter `m`).

**The run's rule (Sergio): the music off during each level-end piece, on again on the next level's first tick.**
- The `M` goes on the first tick the piece plays: the piece is queued, and started, in the tick after the one that
  leaves the prince on the stairs' frame 0xE0, so `M` is two lines after that one (level 3: frame 0xE0 after tick
  372, the piece starts in 373, `M` on line 374). A line earlier it switches the music off before the piece starts,
  which then plays to its end (level 4's dry run: 6A, the skeletons' music, was stopped instead).
- What is left of the wait is the effect playing (169B:0541 `level_end_effect_playing`: any effect but 4, 7 and 0x36
  holds a level's end). The caverns' drips (effects 0x17 / 0x18, drawn at random every few ticks) do: level 3 loads
  on the tick it is decided (387), level 4's dry run a tick later. So the end depends on the random numbers too.
- Every line after the door's Up press is emptied: the prince inside the door (frame 0) is still under the running
  control, and a fresh press of the direction behind him walks him out again (seen in DOS with the Alt flag held
  over a leftover Right; nothing held is the clean way).
- The next level's first input carries `M` (music on).
- **The key is pressed alone.** It is Alt+M, and with Alt held the arrows are not the prince's: Chimera's core (a
  keyboard as DOS has it: Alt down, M, Alt up) drops a direction on the same row, while the search core's input
  would take both (level 6's first tick, a test: `mL` leaves him standing in Chimera, `|M|L.....|` starts his run in
  the search core). So on the ticks the search has to press the key it is offered that input only (`|M|......|`,
  the game class, from level 6's search on), and `tools/cutlevel.py` makes any `M` line the key alone and checks the
  run is the same. Levels 2 to 5 have it alone already.
- **The search plays it all by itself** with `"Music On At Start": true` in the game's configuration (`gen.py ROUTE
  --run`): every input offered carries the music key on the first tick the level-end piece plays (music on), and
  while the music is off outside a level's end (the level's first tick). Its win is the next level loaded
  ("Current Level"), not decided ("Next Level"), so the effect's hold and the random numbers behind it are part of
  what it minimises. (Levels 1 to 3 were searched before this, to the tick the next level is decided; they load on
  that tick.)
- `tools/cutlevel.py GAMEDIR SEED CHAIN.sol LEVEL.sol OUT.sol [NEWCHAIN.sol]` makes the run's lines of a solution of
  either kind (puts the `M` in if it is missing, empties the lines after Up, adds empty lines until the next level
  loads) and checks them tick for tick against the solution (`trs`). Level 3's search file gives the chain's lines.
  A level without stairs (5, 9: no level-end music, the music stays on, the next level's first input has no `M`)
  is kept as it is and checked.
- **Level 6's start, tried ahead** (the run's chain to level 4, level 5's old solution, which replays on it, the
  story scene skipped by Space): Chimera's runner and the search core agree on 460 ticks of level 6 with its old
  solution, the seed too (1775488991 after its first tick: the ambient sound's draw), and that old solution plays
  to the door there (426): level 5's old solution leaves the old chain's seed, from another seed at its start.
- Levels 1 to 3 keep their solutions: no random number is drawn in level 1's wait, level 2 has none, and with the
  music back on its first tick level 2 and level 3 replay tick for tick (the fixed core against the old chain; DOS
  against the fixed core: `CUT3d.script`, both disks, every tick equal up to level 4's first ticks). Level 4 starts
  with another seed (level 3 draws every tick), so it is searched from the new chain.
- Chains: `0400/l010203cut.sol` (921 lines; the last is the tick level 4 loads in), `0500/l01020304cut.sol` (1511),
  `0600/l0102030405cut.sol` (2886), `0700/l010203040506cut.sol` (3282).
  Movie: `tas/run_cut.mov` (4054 rows: level 1 from row 664, level 2 from 1107, level 3 from 1275, level 4 from 1673,
  level 5 from 2273, level 6 from 3658, level 7's first row 4054, IGT 3282 there), built against the runner on the fixed SDLPoP2
  (`~/pop2render/bin/run-native-a5ee`; a level further with `tools/mvextend.py`). The project file waits for a core
  release with the fix (the released one, on SDLPoP2 37112da, plays the music out).
- **A level of the run, step by step** (jaffanator2, `~/jaffarPlus-run/examples/sdlpop2`): `gen.py ROUTE --run --init
  CHAIN.sol --db 300000 --maxsteps N --out script-ROUTE-run.jaffar`; `tools/rewardcheck.py` of it with the level's old
  solution; `run_level.sh DIR SCRIPT TAG`, `watch_level.sh TAG`; `tools/cutlevel.py` (the run's lines and the next
  chain); DOS on both disks (`sol2script.py` with `NOALT=1 SYNC=CORE.txt`, Space at the scenes after levels 5 and
  9, `orun.sh`, `tools/ocmp3.py`: the next section but one); `tools/mvextend.py`; the video
  (`~/pop2render/l8/render-run.sh NAME MOVIE START`).
- **The same as two scripts** (from level 5's end on): `start_level.sh N ROUTE TAG MAXSTEPS` (the run's script from
  the chain, the launch) and `post_level.sh N TAG cut movie video dos`. The next level is launched right after
  `cut` (the machine is the searches'), the rest runs beside it: `movie` is `tools/mvextend.py` with the search
  core's trace (`CORE`), which compares Chimera's core with the search core on every tick of the level (room, x,
  frame, random seed) and lays a scene inside a level (levels 8 and 14) by the runner's tick count standing still;
  `dos` runs both disks at once. A search's progress as a video: `tools/mvpartial.py PREV.mov BEST.sol OUT.mov`
  (a copy of the running search's `/tmp/jaffar.run.TAG.best.sol`), then `render-run.sh`.
- **The project** (`tas/pop2-tas.chimeraProject`, from level 5 on the run itself, with the music cut): written by
  `post_level.sh N TAG project` from the run's movie, a marker a level. It pins a **locally built core package**
  (`~/pop2tas/sdlpop2-a5ee-local.chimeraCore`, SHA-1 804B3038...; the core repository's main 4d44f96 with SDLPoP2
  e927d0f + the Music On/Off fix, built with Chimera's miniBox on jaffanator2: `~/pop2render/core-pkg`, its diffs
  in `~/pop2tas/core-src`), because no release has the fix yet; once one has, the same command with that package
  gives the final file. Before it is written the packaged core, sandboxed (`run-wbx` with `core.wbx`, what Chimera
  loads), plays the movie against the native runner: every row's level, room, x, frame, seed, tick count and IGT
  equal (3658 rows through level 5, 4054 through level 6, 5028 through level 7).
- **The project in Chimera** (`chimera_check.sh PROJECT PACKAGE MOVIE`, part of the `project` stage): Chimera's own
  headless runner, `chimera-run --project`, built from upstream's main of 2026-10-08 (e5fbb9c; a worktree
  `~/chimera-new` on jaffanator2: the month-old build there refused the project's "id" key), takes the project and
  the local package, the game's files and the MT-32's ROMs given as firmware (`--firmware ID=PATH`, an id a file
  name), and plays it: status OK, 5028 frames (through level 7), and the core's memory domains after the last
  frame (Game State, Kid, Characters, Level, Mobs, Trobs) are byte for byte the native runner's on the same
  movie. Not opened in the frontend's window (no display there).
- **The whole run's project (2026-10-10):** 11756 frames, from the boot to Jaffar's death and the final scene's
  first 20 rows (`tas/run_cut.mov`); 15 markers: a level each, and "The end (IGT 10819)" on row 11736, where the
  count stands. It still pins the local package. The packaged core, sandboxed, against the runner: all 11756 rows
  equal; Chimera's runner: status OK, game time 15:01.583, every memory domain equal after the last frame.
  (`MARK="11736:The end (IGT 10819)" ./post_level.sh 14 l14 project`.)
- **Level 14** is searched in segments (`1400/run14.sh`): the scene, room 3's six potions as one search, the eight
  turns, room 6 up into room 7, the fight; each starts on the chain and the segments found before it.
  - **The scene: 51 ticks** (`1400/l14scene.sol`): the exploration's inputs with the music key on tick 2. Level 14 is
    the one level that starts standing (frame 15) with the music off (level 13's end switched it off), so the
    script's rule, the key alone on the first tick, costs a tick there: 52 found (`l14scene52.sol`), tick 1's step
    right being what counts. Tick 2 is inside that step (frames 121 to 132), where no key is read: the key there
    changes no position, nor does it on 44 of the 51 lines (`trs`, each tried). The other levels with the key on
    their first line (4, 5, 7 to 9, 11 to 13) start in a turn (frame 45), where it is free; levels 6 and 10 start
    with the music on.
  - **Room 3's potions, the first search (stopped at step 973): five drunk by tick 539, the sixth never.** The
    first state with each potion on the floor / drunk: 102 / 127, 215 / 241, 310 / 335, 418 / 442, 517 / 539; the
    sixth on the floor at 637. Its kill left him sword in hand on the potion's own tile (the middle row's col 5, x
    317 to 322; the tile's middle, where the potion's pull ends, is 322). Putting the sword away steps him back 10
    px: those states ranked 20 under the ones fencing on the tile, and the full database dropped them for 300
    steps (the watcher's rules alarm). By hand from there (Down, 14 empty lines, Shift): 11 of 11 38 ticks later,
    so nothing in the game is in the way. The fix (`1400/gen.py` `farm`): the sword away while the potion lies
    there is a step of its own (the game clears the flag on the key: `control.c` `sword_sheathe`), 22 steps for
    16. Searched again from the scene; the first run's log is `~/pop2run/l14farm_stall.log`, its script
    `1400/script-farm-run-stall.jaffar`. (A search launched while the one just stopped still frees its database
    dies at once for want of memory: `run14.sh` now looks at the memory available.)
  - **The second search (stopped at step 726): the sixth potion drunk later every step.** First states: 102 / 127,
    216 / 241, 314 / 336, 428 / 452, 522 / 545, the sixth on the floor with the sword away at 647. The best state at
    step 704 had crouched by it 27 ticks before the drink (11 of 11 at 706, replayed), the one at 709 longer. The
    cause is the step after the last potion: its pull up, 60 a pixel from above the room, starts ~10000 below on
    the middle row, for a step's 1000. A state at 11 ranked 9200 under the ones yet to drink, the last of a full
    database (best 37500, worst 28314). The same drop stood behind the first search's sixth potion (-9146 on its
    hand-finished path), so the sword's 20 was not all of that stall. The exploration's returns never met it: each
    was a search of its own from the first state at the target, all its states past the step. The fix: a step
    carries its own reward (`routegen.py` `step(reward=)`), the last potion's 16000 and room 4's 3000 (its row is 20
    px further from its target than room 3's pull was at the change of rooms: 1200 for 1000);
    `tools/rewardcheck.py` on the hand-finished path through the new script: no dip over 44. Log
    `~/pop2run/l14farm_trap2.log`, script `1400/script-farm-run-trap2.jaffar`.
  - **Taken up from the fifth potion** (`run14.sh farmB`, `gen.py farm --hp 10 --in3`): `l14farmA.sol` is the first
    search's way to 10 of 10 at tick 544 (its two kept solutions: 544 and 546; the second search's: 550), `farmB`
    the sixth potion and the way back from there; `l14farm.sol` is the two.
  - **Room 3's potions: 790 ticks** (`l14farm.sol`; `farmB` 246, 41 minutes). The maximum goes up at ticks 133, 246,
    335, 447, 544 and 662 (5 to 11; hits taken at 51, 268 and 468, each filled by the next potion, none after the
    last). The sixth: the guard killed at 632, the sword put away at once, the potion on the floor at 637, drunk on
    the middle row (the first search's kept way: the kill at 635, by hand ~675). Back: straight from the drink into
    the first jump up (tick 667), room 4 at 727 (the step's 3000 less the 1077 the change of rooms costs), its middle
    row at 779, standing at 790: 128 ticks from 11 of 11 (the table's way back from the middle row: 151). The
    exploration's estimate for six potions and back was 847.
  - **The eight turns: 43 ticks** (`l14e1.sol`, under a minute's search; the exploration's 43): the spirit out of
    the body at 6 of 6.
  - **Room 6 up into room 7: 276 ticks** (`l14r7.sol`, 46 minutes; the exploration's 277): room 6 at 49, its Jaffars
    cut down at 92, 153 and 208 (the exploration's third at 221), the fourth gone at 249, room 7 at 265, standing on
    its bottom row's col 7 at 276 (stray keys on the last ticks, as there: the fight starts 10 ticks before).
  - **The fight: 215 ticks** (`l14fight.sol`, 30 minutes, the seed not hashed; the exploration's 215 with the same 10
    ticks of the climb): room 8 at 76, Jaffar sent down at 115, the middle row right of the hole at 138, down the
    hole behind him at 174, the cast at 191 (1 of 3), the fireball out at 202, Jaffar struck at 215.
  - **Level 14: 1372 ticks, the run 10819.** `1400/run14.sh cut`: 51 + 790 + 43 + 266 (276 less the 10 the fight
    takes up) + 215 + 7 = 1372 lines (`1400/l14cut.sol`, `tas/l14.sol`; the whole run `1500/l0102..14cut.sol`, 10819).
    The 7: the game ticks on for 7 ticks after the hit (his death counts to 6: `final.c`; frame 0x15F, where the
    level counts as won, is the third), then the final scene starts and the tick count stands: the runner's Tick
    last moves on row 11735, its IGT stands at 10819 from row 11736 on (9447 on level 14's first row, 10353). The
    exploration's "4 ticks to the win" was the frame's flag, not the scene's start. Chimera's core against the
    search core: all 1371 ticks compared equal (the Jaffar scene inside takes Space and 11 rows, 10404 to 10414).
  - **A variation tried and left (Sergio, 2026-10-10): the body dropped out of room 4.** The eight turns on room
    4's lower tile (the bottom row's one floor tile, col 3, a gap on either side), right out of room 3.
    `spirit.c` leave_body moves the prince 16 px on when the tile behind him is a gap ("placed off a gap": the one
    in front is not looked at then), and the body's record is written from his after the move: near the tile's
    front edge both are over it, and body and spirit fall into room 3 (`gen.py` route `drop`: 56 ticks from the
    room's entry, `1400/l14drop.sol`; Chimera's core the same on all its ticks; video `level14-body-drop.mp4`). The
    body stays on room 3's top row in its falling frame (106) and never settles, so the spirit keeps 6 of 6. The
    spirit does not stay behind (`drop2`: none found in 160 steps); it turns as it goes and can catch the tile's
    ledge (`dc`, stopped at step 105: hanging under the tile 3 ticks after the eighth turn), so the way back up
    would have cost ~25 ticks, not the ~58 of landing in room 3. Not worth it (Sergio): the searches were stopped,
    the run is version 1 (`tas/v1`, tag `pop2-tas-v1`; `tas/` holds the same files). Not tried in DOS.
- **Level 13** at 300 GB (`l13`, 2 h 21 min): 743 with its end (`tas/l13.sol`; Up on line 721, `M` on 730, level 14
  loads on 743), 32 less than the old 775, no hit taken; room 29's big potion drunk at tick 456 (5 of 5). Rooms 3,
  20 (tick 83), 9 (110), 10 (136), 11 (191), 19 (255), 32 (326), 2 (353), 28 (380), 13 (405), 29 (430), 4 (502),
  29 (611), 13 (636), 28 (700).
  - **Room 20's gate taken in a running jump: 15 ticks.** Both ways are on room 3's loose floor by tick 29. The old
    way left it with the sword out, put the sword away (ticks 33 to 58), came back in a running jump, stood before
    the gate from 82 to 89 and went through in a standing jump: room 20 at 98. The solution turns on the floor at a
    run (ticks 24 to 36), runs back, waits farther right (crouched from the run, ticks 48 to 63, standing to 67) and
    runs at the gate: a running jump from x 233 at tick 75 passes it in flight, room 20 at 83.
  - **Room 20 in 27 for 41:** he lands running (tick 85) and takes the room in two more running jumps (91 to 101,
    103 to 113, the second into room 9 at 110); the old way stood up from its standing jump (108), ran, jumped once
    and ran on.
  - The rest within five ticks a room (rooms 32 and 13 take 4 and 5 less, room 10 4 more, room 13 on the way back
    3 more, room 28 with the door 3 less): 32 at the door.
  Chimera's core against the search core: all 742 ticks equal. A story scene follows level 13: Space on the row
  after the level's last (10331), level 14's first row 22 rows later.
- **Level 12** at 300 GB (`l12`, 2 h 19 min): 714 with its end (`tas/l12.sol`; Up on line 692, `M` on 701, level 13
  loads on 714), 21 less than the old 735, no hit point lost. Rooms 19, 7 (tick 36), 14 (90), 22 (116), 14 (164),
  15 (189), 16 (216), 6 (243), 17 (301), 10 (439), 9 (464), 8 (567), 19 (631), 7 (681). The old way to the tick
  through the clip, the plate and the drop into room 22 (standing on its bottom row at tick 139), then:
  - **Room 14's cutter jumped, not crawled under: 17 ticks.** The old way ran at once, was in room 14 at 146 and
    crawled under the cutter (cols 6 and 7) from tick 156 to 184: room 15 at 206. The solution stands 15 ticks in
    room 22 (the sword drawn and put away, ticks 140 to 154), runs, is in room 14 at 164 and takes the cutter in a
    running jump (x 400 to 318, ticks 167 to 172), then the old way's second jump: room 15 at 189. Room 16's rhythm is the
    old one 17 ticks earlier (27 ticks in it).
  - **Room 6's first cutter** in 58 for 62 (he waits crouched before it and crawls less). The rest within three
    ticks either way (rooms 10, 9, 8, 19): 21 at the door.
  Chimera's core against the search core: all 713 ticks equal. No story scene follows level 12.
- **Level 11** at 300 GB (`l11`, 7 h 50 min): 2256 with its end (`tas/l11.sol`; Up on line 2234, `M` on 2243, level 12
  loads on 2256), 11 less than the old way stitched for the run (2267: the potion route's rooms less 178, room 3's
  chunk from the route without the potion; the plan's ~2283 was the potion route less 162). Rooms 5, 1 (tick 23), 2
  (135), 4 (150), 10 (297), 9 (326), 2 (369), 9 (422), 3 (475), 12 (577), 7 (711), 6 (775), 13 (851), 18 (950), 17
  (975), 18 (998), 13 (1026), 23 (1077), 25 (1147), 18 (1201), 24 (1297), 26 (1417), 28 (1570), 27 (1652), 22
  (1820), 21 (1848), 32 (1873), 29 (1886), 20 (1985), 19 (2009), 20 (2032), 27 (2105), 28 (2132), 26 (2157), 24
  (2205). 2 of 4 hit points at the door: the drop in room 29 (tick 1947) and the fall into room 26 (2160). The old
  way within a tick through room 17's plate (975 for 974), then:
  - **Room 18's hole without its hit: 7 ticks** (rooms 18, 13 and 23 in 149 for 157). The old way ran into the hole
    and landed two rows down on room 13's middle row (col 4): a hit point and 30 ticks crouched, room 23 at 1071,
    and there 29 ticks to the jump up at col 8 (the sword drawn, three steps back, sheathed). The solution stops at
    the hole's right edge, has the sword out from tick 1012 to 1021, goes over the edge and catches room 13's top-row
    floor (col 7) on the way down; a drop to the middle row, a turn, the hole jumped standing: room 23 at 1077, 6
    later, but the jump up 16 ticks after the entry (two turns), 7 ahead (1093 for 1100); room 25 at 1147 for 1154.
    The way with the hit was in the database (step 1072's best state is in room 23 with 3 of 4): it was not first
    in room 25. The 4 GB test of the hit (below: 91 ticks with it, 96 without) stopped at room 23's entry.
  - **Room 26** (the guard) in 153 for 157; **rooms 19 and 20** (the hang, the wait for the gate) in 96 for 97.
  - **An entry again, not a way:** step 2096's best state was past room 20's gate already (room 27, x 444,
    crouched, having gone down to room 19's hang a second time), 9 ticks before the solution runs under it (tick
    2105). Up from a crouch takes 10 ticks or more: he stands there at 2106 at the earliest, when the runner is past
    him (x 423) at full speed.
  Chimera's core against the search core: all 2255 ticks equal. No story scene follows level 11.
- **Level 10** at 300 GB (`l10`, 52 min): 299 with its end (`tas/l10.sol`; Up on line 277, `M` on 286, level 11 loads
  on 299), one more than the old 298: rooms 22, 11 (tick 22), 8 (47), 14 (117), 12 (164), 19 (167), 12 (185), 19
  (231; 3 of 4 hit points at the door). **The tick is room 11's guard on this chain's draws.** He comes in from
  room 8 (x 482) on an odd tick once the prince is past x 242, and his steps are drawn. The old inputs on the run's
  chain (`trh`): the guard in at tick 29, advancing from tick 42 (x 431, 429, 427, 413), the prince's third jump
  bumps into him at tick 45 and he is dead at 57; on the old chain that jump passed. The solution's first line is
  empty: started a tick later the prince is a tick further when the guard comes in (tick 31), the guard stands at
  x 445 through the jump and is passed (room 8 at 47 for 46). After that it is the old way (room 8 in 70 for 69,
  room 14 in 47 for 48). A level 9 ending on another seed might give the tick back (its 17 draws are events, room
  entries and the loose tiles, not ticks; the scene after it draws nothing): not tried, it would take level 9 found
  again for that seed and every level from 10 on searched again.
  Chimera's core against the search core: all 298 ticks equal. No story scene follows level 10.
  A 30 GB search to room 14 on the run's seed had it at tick 114 for the solution's 117: the run jump on room 8's
  middle row taken at once (x 247) and a second into room 14, which lands on the last of its three floor tiles (col
  2), past the take-off of the jump down to the bottom row's col 6 (x 191: from further right the fall goes by
  col 6 into room 13). An entry again, not a way (level 8's room 20).
- **Level 9** at 300 GB (`l09`, 1 h 9 min): 369 (`tas/l09.sol`; no stairs, the level ends three ticks into the jump
  off room 8's top row and level 10 loads on tick 369), 7 less than the old solution, whose 373 lines end on that
  jump's first frame in the air (376 by this count): rooms 10, 9 (tick 30), 1 (54), 5 (127), 4 (154), 6 (217), 8
  (269), every room the old one's ticks but room 4 (63 for 70): up on its top row (x 327, tick 191) the old way
  jumped the two loose tiles (cols 4 and 3) from a stand and started its run after them, this one starts the run
  over them and jumps from x 211. Chimera's core against the search core: all 368 ticks equal. **A story scene
  follows level 9**: Space on the row after its last line, level 10's first tick 25 rows after that line.
- **Level 8** at 300 GB (`l08b`, 2 h 35 min; the first search's 885 was not kept, see below): 810 with its end
  (`tas/l08.sol`; Up on line 788, `M` on 797, level 9 loads on 810), 7 less than the old 817: rooms 16, 13 (tick
  22), 4 (45), 3 (78), 2 (179), 12 (247; the old solution 251), 20 (359), 22 (402), 11 (473), 9 (589 for 596: room 11
  in 116 for 119), 1 (693), 24 (719), 23 (744; two hit points lost on the way down, 2 of 4 at the stairs). From room
  12 on it is the old way tick for tick but for room 11's three. Room 2 is entered 25 ticks before the old
  solution's 204 and left 4 before: the jump down room 3's left end starts two ticks earlier and lands across the
  border, so the landing's 20 ticks are room 2's (`tools/roomcmp.py`: room 2 "21 longer", room 3 25 shorter). **The
  sword scene** (line 622) is laid out by `tools/mvextend.py` as in the dry run: Space on the row after the line's,
  line 623 87 rows later; Chimera's core against the search core: the 808 other ticks equal. No story scene follows
  level 8.
  A room entered early is not a way that is faster: a 30 GB search to room 20 had it at tick 348, this one's best at
  step 381 at 351, the solution at 359. The early ones take the run jump on room 12's middle row at once, land in
  room 12 and run into room 20, whose floor ends three tiles in (cols 9..7); run off its end he lands on the bottom
  row at tick 366, x 339, the solution (the run first, the jump from room 12's last tile over that floor) at tick
  372, x 287: six ticks later and 52 px further on, the same within a tick. For a progress video of a running
  search `mvextend.py` takes an unfinished level (env `PARTIAL`: nothing pressed after it, no next level looked
  for, a scene inside laid out all the same).
- **Level 7** at 300 GB (`l07b`, 3 h 17 min; the first search failed, see below): 974 with its end (`tas/l07.sol`; Up
  on line 952, `M` on 961, level 8 loads on 974), 53 less than the old 1027, all of it before the potion: rooms 32,
  31 (tick 23), 7 (100), **the big potion at tick 233** (the old solution: 287, after the head's bite in the wall),
  17 (262), 12, 10, 12, 6 (431), 3, 29, 2 (572), 16 (714), 28, 20, 21 (817; a hit point lost on the way down, 3 of 4
  at the door), 8 (888: the door's plate), 21 (949). Chimera's core against the search core: all 973 ticks equal.
  **No story scene follows level 7** either. In DOS the scene probe fires once more after the script's end (scene 7,
  the intro): left standing at level 8's start he is dead by its tick 36 (the core too), and the game goes back to
  its title. Level 8's loose floors (rooms 4, 8, 10, 17: row 1 col 5, row 1 col 2, row 0 col 9, row 0 col 2), looked
  at before its launch: none in a small potion's column (room 8 col 0, rooms 20 and 24 col 3), so its potion-tile
  fail rules cannot fire by a falling floor.
- **Level 6** at 300 GB (`l06`, 57 min): 396 with its end (`tas/l06.sol`; Up on line 374, `M` on 383, level 7 loads
  on 396), 30 less than the old 426: the same rooms (27 3 6 18 6 4 17 4 7 16), room 7 at tick 285 for 297 and room
  16 at 329 for 355. The old solution does not replay on this chain (the guard at the door kills it: other random
  numbers); room 4's push shows in the reward as single ticks 40 to 80 down (the prince's and the head's records
  change places on those ticks), which the wide search took in its stride. **No story scene follows level 6**:
  level 7's first tick is the row after level 6's last, and the level's number changes in the load tick itself
  (`tools/mvextend.py` presses nothing there).
- **Level 5** at 300 GB (`l05b`, 4 h 23 min): 1375 (`tas/l05.sol`), 61 less than the old 1436, all of it in rooms
  14 to 16 (room 13 at tick 460 as before, room 17 at 747 for 808) and a tick more at the lift. No stairs: the
  level ends on the lift (room 27 is level 6's), the music stays on, level 6's first line has no `M`. Chimera's
  core against the search core: all 1374 ticks equal, the seed too (4143532311 at level 6's load). The story
  scene after it: Space on the row after the level's last, level 6's first row 3658.

| Level | Ticks | IGT at its end | First row in Chimera | DOS (both disks) |
|---|---|---|---|---|
| 1 | 433 | 433 | 664 | every tick equal |
| 2 | 101 | 534 | 1107 | every tick equal |
| 3 | 387 | 921 | 1275 | every tick equal |
| 4 | **590** | 1511 | 1673 | room, x, y, frame, hit points equal on every tick; the seed a draw ahead for one tick, twice |
| 5 | **1375** (was 1436) | 2886 | 2273 | in segments (MT-32 20, Sound Blaster 9): room, x, y, frame, hit points equal on all 2896 ticks; the draws apart by one ambient draw on the ticks that end a segment |
| 6 | **396** (was 426) | 3282 | 3658 | in segments (MT-32 22, Sound Blaster 11): all 3292 ticks equal in position, level 7 loads on the same tick |
| 7 | **974** (was 1027) | 4256 | 4054 | in segments (MT-32 29, Sound Blaster 18): all 4266 ticks equal in position, level 8 loads on the same tick |
| 8 | **810** (was 817) | 5066 | 5028 | in segments (MT-32 35, Sound Blaster 18): all 5076 ticks equal in position, the sword scene left by Space at the scene probe's sixth hit, level 9 loads on the same tick |
| 9 | **369** (was 376) | 5435 | 5925 | in segments (MT-32 35, Sound Blaster 18): all 5445 ticks equal in position, the scene after level 9 left by Space at the scene probe's seventh hit (`SCENES="5 6 7"`, the DOS stage's default from level 9 on), level 10 loads on the same tick |
| 10 | **299** (the old 298 and room 11's guard, see above) | 5734 | 6319 | in segments (MT-32 39, Sound Blaster 22): all 5744 ticks equal in position, level 11 loads on the same tick; on the Sound Blaster disk DOS runs by itself from tick 3800 to 5566, the guard's passage (level 10's ticks 22 to 47) inside it |
| 11 | **2256** (the old way stitched: 2267) | 7990 | 6618 | in segments (MT-32 66, Sound Blaster 62): all 8000 ticks equal in position, level 12 loads on the same tick; the new segments are nearly all level 11's, where the seeds part by one draw and meet again a tick or two later (once after 17, ticks 7006 to 7023 on the MT-32 disk) |
| 12 | **714** (was 735) | 8704 | 8874 | in segments (MT-32 72, Sound Blaster 68): all 8714 ticks equal in position, level 13 loads on the same tick |
| 13 | **743** (was 775) | 9447 | 9588 | in segments (MT-32 72, Sound Blaster 70): all 9457 ticks equal in position, no draw of level 13 off on either disk; the scene after it is the probe's eighth hit (ax=3; Space, as the run presses it), level 14 loads |
| 14 | **1372** in segments (`1400/run14.sh`): the scene 51, room 3's six potions 790, the eight turns 43, room 6 up into room 7 266, the fight 215, 7 to the final scene | **10819** (the final scene's start) | 10353 | in segments, the lateness meter cleared at every tick (see below; MT-32 76, Sound Blaster 87): all 10820 ticks DOS plays equal in position, none of level 14's draws off; the scene with Jaffar at the window is the probe's ninth hit (ax=5; Space), the final scene its tenth (ax=B), on the tick after the last: the game is won in DOS on both disks |

The table further up is the same run with the music left alone (1120 at level 4's start).

### DOS and the random numbers: the run is checked in DOS in segments (2026-10-08)

Tried ahead of the run: its chain to level 4, then the old solutions of levels 5 and 6 (which replay on it: level 7
loads on tick 3373), in DOS on both disks, the story scene after level 5 skipped by Space as the run does (Space in
the keyboard buffer at the scene's start). **Unsynced, DOS leaves the core in level 5**: positions equal to its end
(but for the rope bridge's sway, a pixel, on three ticks), yet the seed two draws ahead there; in level 6, three
draws apart, room 16's head kills him at tick 396.

- **Whose draw:** the ambient music's next piece (`sound.c` ambient_update, 1611:03CC). It is drawn at the end of the
  pass in which the piece is found over, and that moment moves with the drawing's time in DOS (tens of ms); the
  core's model has it 5 ms into the pass (SDLPoP2 docs/FINDINGS.md 5.11: "a piece ending within ~40 ms of a check can
  go either way"). With 25..46 ms of the piece left at a check DOS was a tick early in 5 cases of the run's first
  2200 ticks (levels 4 and 5), and the two disks disagree between themselves on three of them. A tick early means
  another random number for the piece, so another piece, with its own length, and from then on the draws come on
  other ticks. Levels 1 to 4 had the early draws too (ticks 997, 1287) but the same pieces after them.
- **It hangs on the machine's speed** (measured in the oracle: watches on the driver's 240 Hz interrupt, DS:1FBE,
  on the free 60 Hz count, DS:24E4, and on the music channel's busy flag, DS:209E; a probe on each pass's sound
  phase, 1611:04D0). A pass is 20 interrupt periods (24 in a fight), and the sound phase comes 0.5 to 3 periods
  into it on the oracle's 486 (22000 cycles a ms); but in some rooms every third pass draws 5 to 10 periods, and a
  pass that draws a new room takes 20 to 61 (so it runs late, and the next one starts off the grid). At ten times
  the cycles the same room changes still take 7 to 14. No fixed model in the core can follow that, so (Sergio) it
  is not pursued: the core keeps its model (every pass on time, the sound phase 5 ms in), the run is the core's.
- **The check (Sergio): in segments.** The oracle's script writes the core's seed at every tick's start
  (`sol2script.py` with `SYNC=CORE.txt`). Where DOS's own seed is the core's the write changes nothing, so DOS plays
  by itself until a tick whose draws differ, and a new segment starts after it; the snapshot is taken before the
  write, so every tick's draws are compared. `tools/ocmp3.py SNAP CORE.txt` lists the position mismatches, the
  segments and the ticks that end them. The same chain, MT-32 disk: **24 segments, all 3383 ticks equal in room,
  x, y, frame and hit points, level 7 loading on the same tick**; the 23 ticks that end a segment differ by a
  single ambient draw each (11 where the core drew a piece and DOS did not: all of them the core's ambient draws;
  12 where DOS drew one more; 36 of the core's 47 ambient draws fall on the same tick in DOS). The Sound Blaster
  disk: 18 segments, all equal.
- **The lateness meter, and level 14's room 3 (2026-10-10).** DS:2BA4 (169B:05A1) goes up on a pass that took a
  timer tick more and down on one in time; the core's is always 0 (every pass on time: `glue.c` frame_on_time). On
  the oracle's 486, 2 to 13 ticks in 500 start with it at 1: the passes that draw a new room. Torches and the
  ambient effects only draw less often then, but level 14's tiles 0x28 (room 3), 0x29 and 0x2A (rooms 6 to 8)
  animate another way while the game is late, with other draws (`final.c` anim_tile28: a delay of 30..90 for a
  showing after 1..3), and in room 3 those come ahead of the guards' draws in the tick. Found on the run's level
  14 through the eight turns: the pass that draws room 3 is late (the run's tick 9527), the tile takes the late
  branch at 9528 and its draws fall on other ticks than the core's from then on; at 9907 one of the core's comes
  before the guard's strike draw, so DOS's guard strikes and the core's does not; his body ends at x 289 for 291,
  across a tile's edge, the potion lies on col 3 for col 4, and at 9925 Shift picks nothing up in DOS (he turns
  round, the opponent behind him). Both disks alike, the prince's position equal to tick 9924. With the meter
  cleared at every tick's start (`sol2script.py` with `LATE0=1`: a machine that is never late, as the core is)
  **DOS plays the six potions and the eight turns as the core does: 10341 ticks, no position mismatch on either
  disk**, and the ticks whose draws differ fall from 218 to 75 (MT-32) and from 216 to 86 (Sound Blaster), none
  of them in level 14. The run is that of a machine on which no pass is late; on the oracle's 486 as it stands
  room 3 goes another way. **The whole run that way (`./post_level.sh 14 l14 dos`, `LATE0` its default for level
  14): 10820 ticks, no position mismatch on either disk, the final scene's probe hit on the next tick.** At 60000
  cycles a ms (`oracle-run --setting cpuCycles=60000`, the meter left alone) fewer passes are late (0 to 4 in 500)
  but room 3's still is, and the same tick 9925 parts. **At 220000 cycles a ms, ten times the preset, the meter
  left alone, the pass that draws room 3 is in time and DOS plays room 3 as the core does**: the run through the
  eight turns, MT-32 disk, all 10336 ticks its script holds equal in position, and only 5 ticks' draws off in all
  (levels 5 and 7: 1748 / 1749, 3696 / 3697, 3800), so DOS runs by itself from tick 3801 to the end, 6500 ticks
  with no seed written that was not its own; 4 ticks of the run still start late (one each in four of its 500-tick
  stretches), none where it matters. (The whole run at that speed, both disks, was started and stopped with the
  project's close: not measured past the eight turns.) (Tools: the oracle's snapshot holds DS:2900..6BFF after every tick, the level's rooms,
  records and tiles among it; `tools/trh.c` built with `-DDRAWS` names each draw's caller, but for `guard.c`'s own.)

### The later levels' scripts, checked before their searches (2026-10-08)

While level 5 searched, every later level's script (the generator's, as the run will use it) was replayed against its
old solution with `tools/rewardcheck.py` (the old chains and the exploration tree's player: the reward's shape hangs
on the script and the path only). Each `gen.py` now takes the run's hit points with `--run` (3 at level 7, 4 from its
potion on, 5 after level 13's; no "Max HP At Level"). What the old solutions showed, and what was changed:

| Level | Route (`gen.py`) | Found | Changed |
|---|---|---|---|
| 6 | `sw-push-L` | Never run as one search (two chunks). Its steps described the crawl tunnel; the solution goes 6, the hang (room 18's top row), 6, 4, down its hole into 17, the sword, up into 4: 36 and 125 ticks 5000 down | `sword_by4()`: the steps of that way; the push's step asks for the sword; once inside the wall (x < 140) is a step; no room 7 without the sword; the door's stairs pull right and up |
| 7 | `r1c-pot` | Room 7 pulled to the potion along its middle row: the wall's states 135 above the bottom row's for 50 ticks, col 8's above the drinking ones; the hang over room 12's hole 116 down for 18 ticks; room 2's head-kill credit lost for one tick | `potion7clip()`: down and right, up at col 7, the potion at x 429; the hang's magnet at y 30; the head dead is a step |
| 8 | `np` | Room 11's top row pulled to col 2's left end, the jump up is at x 236 | The magnet there |
| 9 | `np` | The top row's gaps (rooms 4 and 8) are jumped short: 18 ticks hanging 60 below the gap's near edge; a runner on the middle row outscores the hang | A weaker pull up in those rooms; the middle row left of a hang, and anything below, fails |
| 10 | `np` | Room 12 pulled to its loose floor: running on from it 145 down for 11 ticks; the hang 33 down | Room 12 pulls right; the hang's magnet at y 30; room 19 counts by the door's side only |
| 11 | `np` | No wrong way on the potion route's solution, nor on the room 3 chunk without it; the hang under room 20's floor 68 down | The hang's magnet at y 30; room 17 after the exit's plate fails on `np` too. **Hit points: see below** |
| 12 | `via22` (the 735 solution, `np.sol`, goes by room 22) | Room 6 counts once past its gate (x < col 8): the jump's three ticks in it before that scored 5000 down | Room 16's step keeps room 6 on its own scale (x 120) |
| 13 | `pot` | The same at room 20's gate: two ticks 5000 down | Room 3's step keeps room 20 on its scale |

**Level 5's first search at 300 GB flooded** (stopped at step 333, 2026-10-08). The exit is not a carpet but the
pit trap under the ceiling's mouth (lever5.c): crouching on the trap lifts him whatever the mouth does, and under a
shut one he dies at the top. The trap lies under room 3's middle row, which the route comes along at tick ~265, and
the wide search found the crouch on it at tick 279. The lift's step asked for the sequence only; a step marks every
step before it done, so 23 steps' rewards went to a ride to his death, and its states (50 ticks each, by the
million) took the database's top, the worst reward kept jumping from 1500 to 6400. The old searches never crouched
there. Now the lift's step asks for the mouth open, the lift under a shut mouth fails (`0500/gen.py`), and
`watch_level.sh` also ends on a jump of four or more rules done in one step. Relaunched as `l05b`.

**Level 7's first search at 300 GB sat in room 7's wall** (`l07`, 1300 steps, 4 of 57 rules done from step 89 to
the end, 4 h 20). Coming out of room 31's wall the way down to room 7's bottom row is its loose floor (row 2 col 1):
off the wall's right end (x ~197) he lands on it in sword stance (the head has seen him; y 152 to 182 in one tick,
no hit point lost), or, falling plainly, breaks through it into room 22 (kid.c `check_fall_landing`: a fall breaks
a loose floor at once). Either way the floor falls, onto room 22's small potion (row 0 col 1) right under it, whose
tile becomes rubble (0x0A to 0x0E) 19 ticks after he lands. The small potions' tiles are fail rules ("never drunk":
the tile must stay 0x0A), so every state failed 19 ticks after the bottom row, and the wall's states with a fall begun
(rule 4, his row 2) held the database's top for 1200 steps. The old solution never shook the floor: the head bit him
inside the wall as he dropped (tick 182, 5 hit points then), which set him on the bottom row at x 177, in the wall,
and he crossed the floor in sword stance, whose frames do not press it. That bite hung on the old chain's seed (the
head's first idle draw kept it on the middle row); the same moves on the run's chain: the head drifts down first,
comes back to the middle row and settles idle there, no bite.
Found by replaying the old solution on the 3 October core beside today's (`tools/trh.c`: the room's characters and
records a tick, every random draw with its caller), then by a 30 GB search to "standing on the bottom row" (116
ticks) and on to the potion, which ran out of states at step 149, the tick room 22's tile changed. Fixed in
`0700/gen.py`: the r1c routes do not ask for room 22's potion (the room is off them: he cannot drink it), and the
bottom row's steps ask for him standing (y 182), so a fall past it is no step. The 30 GB test then drinks room 7's
potion at tick 229 (the old solution: 287). Relaunched as `l07b`, which found the level in 974 (the potion at tick
233). `watch_level.sh` now also ends on the rules standing still (the best state's rules done the same as 300 steps
earlier): this one would have been called at step 389, not 1300.

**Level 8's first search at 300 GB came in 68 ticks behind the old solution** (`l08`, 885 for the old 817, 2 h 49;
no alarm: its rules went on step after step). Room by room against the old solution (the exploration tree's core
and chain, `~/pop2tools/trl`): the same to tick 244, room 12 at 264 for 251, room 20 at 450 for 363, and from
there 19 ticks better (room 11's magnet, room 9). Not the chain's random numbers: the old solution's inputs from
tick 245 on, put after the new one's first 244, play on the run's chain as they did (room 12 at 251, its middle
row at 297, room 20 at 363). The search had the way and dropped it.

Room 12 is entered out of room 2's wall onto its bottom row's one floor tile (col 8; cols 6..7 are the shaft down
to room 6, a fail). The way up: a jump under the two tiles over the shaft (row 1 cols 6..7, 0x0F: they break, tick
269), a second one that hangs on col 8's ledge and climbs: 46 ticks at x 410..414 with the reward standing, since
the room's step pulled left and nothing else. A state in sword stance at the floor's left end (x 393..405; he
comes out of the wall with the sword in hand) scores up to 21 more for as long as it likes, and such states are
as many as the ways to have got there. They filled the database, the jumps were dropped, and the solution found is
one of them that sheathed late (the jumps from tick 332). `tools/rewardcheck.py` shows nothing here: the way's own
reward never dips, it is others that stand above it.

Fixed in `0800/gen.py`: the bottom row pulls to the jump's place and up (x 414; the smoothed y scores every frame
of a jump up), the broken tiles are a step (pull to x 410, up), the middle row another (left). On the old inputs
the reward now climbs through both jumps (one dip of 14 over the first jump's landing, where every state is in
the same frames). A 30 GB search to room 20 with it: tick 348 (the old solution 363). Relaunched as `l08b`: 810
ticks, room 12 at 247 and its middle row at 293 (the old solution 251 and 297); the level's entry is above.

Three checks came of it. `watch_level.sh` ends on **a room entered late**: with a reference (`mkref.sh TAG N
OLDSOL`: an old solution's rooms and the ticks it enters them on) the best solution so far is replayed every
minute and its rooms compared in order, 30 ticks late is the alarm (this search: room 20, at step 450 of 885).
`tools/waitcheck.py SCRIPT TRACE [MIN [MAP]]` lists where a known solution stands in one place, the step in force
there (as far as the trace tells) and where it pulls, with the room's tiles; `!` is a jump up or a hang under a
pull along the row and none up. It marks the old script's room 12 (34 ticks at x 410..415, pull to -200, no y) and
not the new one's. On the old solutions of levels 9 to 13 with the run's scripts: no `!` in force but two with
nothing to stand on beyond them (level 8's room 22 and level 13's room 19: a wall); the jumps under a pull up and
one along the row (level 11 has some twenty) have a wall or a hole on the pull's side, or the pull is a tile off
(15 px, made up by the jump's own 2 a tick). And every search's solution is compared room by room with the old one
before it is kept (`tools/roomcmp.py REF TRACE`: this search's rooms 2 and 12, 13 and 74 ticks longer).

The sword scene in DOS, tried on this search's solution: Space at the scene probe's sixth hit (`SCENES="5 6"`, the
DOS stage's default from level 8 on; without a key the scene does not end), level 8 reloaded 119 frames later, all
5151 ticks equal in position on both disks, level 9 loading on the same tick, no scene after level 8.

What recurs (for any new script):

- **A hang reads the room below** (its top row, y 56). That room needs the step's magnet too ("keep"), and its y a
  little above the hang (30): the climb's smoothed y rises 4 px a tick, so hanging and climbing gain. At y 56 the
  climb's ticks lose 4 each; far above (-50, the usual "up") the hang scores less than the gap's near edge.
- **A step with a place in its condition** (past a gate, a row) leaves the room's first ticks without a step: the
  room is one ahead, 5000 down, and a full database drops every state that enters. The step before it has to keep
  that room on its own scale (the next room left: x 120 for a pull to the left edge).
- **The magnet stands where he acts** (the tick he drinks, jumps up, takes the sword), not at the tile's end: a
  place to stand between the two scores above the ticks of the act.
- **The Opponent HP Magnet loses its credit for a tick now and then** (level 7's tick 685, level 11's 1638: it read
  what the prince himself had lost, as if the opponent's slot held his record that tick). A kill that matters is a
  step (the opponent's room, its char id, 0 hit points), which also ends the magnet.
- **A step marks the steps before it done.** Its condition must not be met early by a way that ends badly: the
  search then pours into that way (level 5's lift; level 6's push step without the sword, met on his first pass
  through room 4). A late step carries what makes it the route's (the mouth open, the sword, a room seen).
- **A fail rule on a tile says what it reads, not what it means.** "The potion's tile stays" also fails a potion
  buried by a falling floor (level 7's room 22 under room 7's loose floor), a tick long after the cause and in
  another room. The old solution passing the script (rewardcheck) proves nothing about ways it did not take. A
  search that stands at the same rules for hundreds of steps: look at what fails its states (a small search to the
  next step ends "out of states" on the tick of the rule).
- **Where the way stands still, nothing reachable may score above it.** A jump up, a hang, a climb's start: the
  reward stands unless the step pulls up (the smoothed y), and under a pull along the row every state further along
  it outscores the way for those ticks; a wide database fills with them (sword stance at any x) and drops it
  (level 8's room 12: 88 ticks). The step pulls to the jump's place and up, and what the jump changes (a tile, the
  row) is the next step. `tools/waitcheck.py` lists the candidates on a known solution.
- **A dip is no trap when the place above it is on the way too** (level 6's door: the states nearer the door enter as
  soon as it opens), or is the search's own to-and-fro in a wall (levels 6, 7, 8: the head's push, the clips).

**Level 11's hit points.** The route without the potion takes four hits on its old path: room 3's loose floor as
it falls (the chunk without the potion), the fall into room 13 from room 18's hole, the drop in room 29, the fall
from room 28 into 26. The exploration had 5; the run has 4. The search fails a dead prince, but it keeps the fastest
states, which take each hit as it comes: the ones that spared a hit point further back are behind and dropped. Two
tests (4 GB, the old chain, a fail on any hit):

- **Room 3's floor is stepped away from at no cost:** room 9's ledge to room 12 in 111 ticks unhurt, 110 with the
  hit. (The first try sat 200 ticks in room 3: the step after the floor's fall pulled to col 0's left end, where the
  jump up catches nothing; the hit had thrown him to the right x, ~178..184. The magnet is there now.)
- **The fall into room 13** (a medium landing: a hit point and ~28 ticks crouched): room 17's plate to room 23 in 91
  ticks with it, 96 unhurt (down by the hole's edge, a row's drop).

So the run's script (`np --run`) fails any hit up to room 3 and leaves the three falls: 4 -> 1 at the door. (The run's
solution takes two of them, 2 of 4 at the door: room 18's hole is gone down without the hit and that is the faster
way from room 23 on. See the final run's level 11.)

### The level ends' music (found 2026-10-08)

A level's end waits for its music, and the wait is long: the piece is queued at the stairs' frame 0xE0 (level 1: the
railing's frame 0x95) and the next level loads when it has played out (sound.c `level_end_sound_playing`, game.c
`frame_after_tick`). By level kind (`kind_music`): 0x1C, 107.4 ticks (levels 1, 3, 4: 105 / 94 / 94 after the level
is decided); 0x1D, 223.4 ticks (the ruins, levels 6..8: ~210); 0x1E, 180.6 ticks (the temple, levels 10..13: ~167).
Level 2 has none; levels 5 and 9 end without stairs. About 1590 ticks of the run are these waits (105 + 2 x 94 +
3 x 210 + 4 x 167), which the exploration's times (~11200) did not count.

**Any other music piece started in the meantime ends the wait** (the pieces share the MIDI channel; "playing" asks
whether the channel still holds that piece). Checked in DOS (MT-32 disk, `WC1.script`): music 0x65 written into the
queue word (DS:0880) at tick 445 of level 1's wait: the story scene comes after tick 446, not 538.

**A prince not under his usual control at the stairs' frame 0xE0 never starts the music** (control.c `control`: the
stairs' handler is not reached when he is dead, when his action is 4, 5 or 9, or with his sword drawn; the sequence
plays on and decides the next level all the same). Checked in the core and in DOS (MT-32 disk, `WC3.script`): his hit
points written to 0 on level 3's stairs (tick 370): level 4 loads after tick 387, not 481, at full health. No way
to get there in play is known yet: every hit found so far (swords, rocks, falling floors) also starts another
sequence, and a prince dead inside the door (frame 0) plays no death music.

What queues music in play (`sound_1611_01a8`): an opponent's death (fight.c `dead_char_music`: a guard 6 ticks after
it dies, a skeleton when its fall's sound is over, a head when its 18.6-tick cry is over; not on level 1), a head hit
and falling back (heads.c, music 0x92, once a head), the exit door starting to open (mobs.c, 0x1B), a potion drunk
(0x65), level 4's skeletons getting up once the door is open (0x6A), the prince's own death (6 ticks after it), and
places of levels 5, 8, 9, 13, 14. All the effects (`play_sound`) are digital and do not touch the MIDI channel.
Useful facts: a falling loose floor takes 100 hit points from an opponent under it (mobs.c: a kill with a delay); a
dead opponent's count runs only in the drawn room; the next level starts at full health whatever happens in the wait.

The search reads the wait off the game: with `"Level End Lookahead": true` in the game's configuration, from the
stairs' first frame on the property "Level End Wait" is the number of ticks to the next level's load, played ahead
without input (`updateLevelEndWait`). A cut search wins on a short wait and fails the others, so the door's
states do not fill the database (`0300/script-left-cut.jaffar`: from level 3's tick 262, 222 steps, 100 GB).

| Level | Wait | Cut search |
|---|---|---|
| 1 | 105 | nothing queues music there |
| 3 | 94 | none found (the skeleton is on the row below the door: its death's music comes 41 ticks before the stairs) |
