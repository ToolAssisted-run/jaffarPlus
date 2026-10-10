# Prince of Persia 2 (DOS, 1.1) TAS, version 1 (2026-10-10)

IGT 10819 ticks from level 1's first tick to the final scene's start (game time 15:01.583).

- `pop2-tas.chimeraProject`: the run from the boot to Jaffar's death, 11756 frames, a marker a level. It is made
  with the SDLPoP2 core's package of commit 0153ff1 (SDLPoP2 989c907: the drawing fixed for a game played from the
  start), which Chimera must have in its Cores folder:
  `sdlpop2-0153ff151b54d29e0c5bb87fd660aabcd2ce4e08.chimeraCore` (SHA-1 362A2B45B9E4E15E5FAF28C876032952A419B98E),
  the asset of the core's `dev` release (https://github.com/ToolAssisted-run/chimera-core-sdlpop2/releases/tag/dev)
  until its next commit, and of its dated nightlies from `nightly-2026-10-11` on (a nightly is kept for good; the
  same commit built twice gave the same file). The game plays the same on the package of bf617e9
  (`nightly-2026-10-10`), which the project named before: only the pictures differ. The project asks for the
  game's own files (1.1) and a Roland MT-32's two ROMs with PRESET40.DEF as firmware, by SHA-1; it carries none.
- `run_cut.mov`: its movie (the core runner's letters, a row a frame).
- `l01.sol` .. `l14.sol`: the levels as the search core plays them, a line a tick (seed 7881336).

Level ticks: 433, 101, 387, 590, 1375, 396, 974, 810, 369, 299, 2256, 714, 743, 1372. See `../../ROUTING.md`
("The final run") for the route, the searches and the DOS check.
