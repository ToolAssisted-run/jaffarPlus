# Levels 1 and 2 in one run

`seed151.sol` plays a new game started with random seed 151: level 1 in 460 ticks (`0100/level01.sol`), then level 2
in 102 ticks (`0200/seeds/seed18.sol`). Level 3 starts at tick 562.

The seed was found by replaying the fastest level 1 and level 2 solutions one after the other under many seeds: with
seed 151 both hold, the guards of level 1 behave as in the solution, and level 2's sand tiles give the solution's trick.

## For Chimera's SDLPoP2 core, from the boot

Piano roll inputs (paste at frame 0), one line a frame, `|commands|P1 Up Down Left Right Shift Ctrl|`:

- `chimera-boot-to-level3-restart.txt`: level 3 at frame 620. Level 1 in 433 ticks with the Restart Level key at the
  checkpoint (`0100/level01-restart.sol`), then level 2 in 102 ticks. Settings: Random Seed 25, Sound Device fm, cheats
  off.
- `chimera-boot-to-level3.txt`: level 3 at frame 646, without the restart. Settings: Random Seed 48, Sound Device fm,
  cheats off.

The game advances its seed once at start-up, so the setting is the seed one step before the level 1 seed the
solutions need (25 gives 7881336, 48 gives 12803635). Space skips the title and the story scenes; the copy protection
before level 3 is answered with the arrows and Shift.

- `chimera-boot-to-level4-right-glitch.txt`: the restart movie above (one row shorter: level 3's inputs start on the
  row level 3 begins), then level 3's right exit without potions (`0300/right.sol`, 594 ticks), then Space to skip the
  story scene after level 3; level 4 at frame 1225. Settings: Random Seed 25, Sound Device fm, Version 1.1, cheats
  off. The prince crosses room 15's top row of walls with his sword out (frames 812..823). Chimera draws one random
  number more than QuickerSDLPoP2 between levels 2 and 3 (the copy protection), so a level 3 solution that meets the
  random numbers (the left exit's) does not play the same here; this one does not.
- `chimera-boot-to-level4-right-glitch-mt32.txt`: the same with Sound Device roland (655 more frames at the start
  while the MT-32 gets its timbres): level 3 at frame 1275, room 15's wall clip at frames 1467..1478, level 4 at 1880.
- `chimera-boot-to-level5-mt32.txt`: Sound Device roland, Random Seed 25, Version 1.1: levels 1 and 2 (restart combo),
  level 3's left exit (476), Space, level 4's slow-fall route (628, chunks), Space. Level 3 at frame 1275, level 4 at
  1762, level 5 at 2401. Level 4's inputs start on its first frame, 10 frames after the Space that skips the scene.
