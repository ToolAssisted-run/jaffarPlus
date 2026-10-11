# Prince of Persia 2 (DOS, 1.1) TAS, version 3 (2026-10-11)

IGT 10351 ticks from level 1's first tick to the final scene's start (game time 14:22.583): 162 less than version 2
(`../v2`, 10513), 468 less than version 1 (`../v1`, 10819).

What changed from version 2: level 14 only (1210 ticks for 1372). The eight turns are made on room 4's lower tile,
right after the climb out of room 3 (Sergio's): the body left over the tile's edge falls back into room 3 with the
flame, never lies down, and so does not take the 3 hit points its lying down costs. 9 at the turns will then do for
11 (the flame at 4 of 4, one fireball): two potions fewer. The farm is version 2's up to the fourth potion's drink;
from there the way up the shaft, the turns, the fall and the climb back, rooms 5 to 7 and Jaffar were searched.

- `pop2-tas.chimeraProject`: the run from the boot to Jaffar's death, 11288 frames, a marker a level; the SDLPoP2
  core's package of commit 0153ff1 as the versions before (`../v1/README.md`).
- `run_cut.mov`: its movie. `l01.sol` .. `l14.sol`: the levels as the search core plays them (seed 7881336).

Level ticks: 433, 101, 387, 590, 1375, 396, 974, 495, 378, 299, 2256, 714, 743, 1210. See `../../ROUTING.md`
("Version 3").
