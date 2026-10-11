# Prince of Persia 2 (DOS, 1.1) TAS, version 4 (2026-10-11)

IGT 10302 ticks from level 1's first tick to the final scene's start (game time 14:18.500): 49 less than version 3
(`../v3`, 10351), 517 less than version 1 (`../v1`, 10819).

What changed from version 3: level 14's fight with Jaffar (166 ticks for 215; the level 1161). The flame (4 of 4
after the body's drop) casts a fireball as soon as it is up in room 7: with 2 left it is the shadow, and Jaffar
comes for a shadow instead of walking his round away (Sergio's). With him on its row it drinks room 8's bottom-row
potion (3: the flame again) and casts the second fireball at him.

- `pop2-tas.chimeraProject`: the run from the boot to Jaffar's death, 11239 frames, a marker a level; the SDLPoP2
  core's package of commit 0153ff1 as the versions before (`../v1/README.md`).
- `run_cut.mov`: its movie. `l01.sol` .. `l14.sol`: the levels as the search core plays them (seed 7881336).

Level ticks: 433, 101, 387, 590, 1375, 396, 974, 495, 378, 299, 2256, 714, 743, 1161. See `../../ROUTING.md`
("Version 4").
