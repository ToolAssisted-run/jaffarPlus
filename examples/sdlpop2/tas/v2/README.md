# Prince of Persia 2 (DOS, 1.1) TAS, version 2 (2026-10-11)

IGT 10513 ticks from level 1's first tick to the final scene's start (game time 14:36.083), 306 less than version 1
(`../v1`, 10819).

What changed: level 8 goes the direct way (Sergio's: rooms 16 13 4 5 10 19 21 9), through room 9's shut gate with
the sword drawn (495 ticks for 810). The later levels are version 1's wherever the game still plays them the same
on the new run before them: level 9's last room was searched again (its two heads move another way on the new
random numbers: 378 for 369), level 10 is the same file, level 11 is the same but for its guard fight in room 26
(105 ticks searched again, the same length, joined back on the stand above the fight), and levels 12, 13 and 14 are
the same files: by level 12's start the run has drawn as many random numbers as version 1 had (4826), so the game
is in the same state from there on, the final level's farm and fight included.

- `pop2-tas.chimeraProject`: the run from the boot to Jaffar's death, 11450 frames, a marker a level. Made with the
  SDLPoP2 core's package of commit 0153ff1, as version 1's (see `../v1/README.md` for the file and what the project
  asks for).
- `run_cut.mov`: its movie (the core runner's letters, a row a frame).
- `l01.sol` .. `l14.sol`: the levels as the search core plays them, a line a tick (seed 7881336).

Level ticks: 433, 101, 387, 590, 1375, 396, 974, 495, 378, 299, 2256, 714, 743, 1372. See `../../ROUTING.md`
("Version 2") for the method and the checks.
