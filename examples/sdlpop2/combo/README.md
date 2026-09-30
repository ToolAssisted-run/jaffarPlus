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
