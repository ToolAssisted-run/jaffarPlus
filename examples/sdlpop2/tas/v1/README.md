# Prince of Persia 2 (DOS, 1.1) TAS, version 1 (2026-10-10)

IGT 10819 ticks from level 1's first tick to the final scene's start (game time 15:01.583).

- `pop2-tas.chimeraProject`: the run from the boot to Jaffar's death, 11756 frames, a marker a level. It is made
  with the SDLPoP2 core's package of commit bf617e9 (SDLPoP2 3f323c6), which Chimera must have in its Cores folder:
  `sdlpop2-bf617e900301bf83c468890d45600b98a226bae2.chimeraCore` (SHA-1 39EA822F9858EE1F0FEED1A75888A4B5C8949BC8),
  the asset of https://github.com/ToolAssisted-run/chimera-core-sdlpop2/releases/tag/nightly-2026-10-10 (the `dev`
  release of that day holds the same file). The project asks for the game's own files (1.1) and a Roland MT-32's
  two ROMs with PRESET40.DEF as firmware, by SHA-1; it carries none of them.
- `run_cut.mov`: its movie (the core runner's letters, a row a frame).
- `l01.sol` .. `l14.sol`: the levels as the search core plays them, a line a tick (seed 7881336).

Level ticks: 433, 101, 387, 590, 1375, 396, 974, 810, 369, 299, 2256, 714, 743, 1372. See `../../ROUTING.md`
("The final run") for the route, the searches and the DOS check.
