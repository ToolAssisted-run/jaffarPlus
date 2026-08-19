# Changelog

All notable changes to JaffarPlus are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

## [2.0.0] - 2026-06-19

A large release consolidating a rewritten engine core, a complete documentation set, broad
emulator support, and a permissive license. Highlights below.

> **Note on licensing.** JaffarPlus's own source code is now distributed under the **MIT License**.
> The emulator cores it builds against are included as separate git submodules and are licensed under
> the **GPL** — a binary you build links one of those cores and is therefore a GPL derivative work.
> See the README for details.

> **Note on submodules.** A source archive from a GitHub release does **not** include submodules.
> Clone with `git submodule update --init <core>` (and `extern/jaffarCommon`) before building.

### Licensing
- Relicensed JaffarPlus's own source under the **MIT License**, and documented that built binaries
  are GPL because of the linked emulator cores (previously the `LICENSE`, README, and build metadata
  disagreed).

### Documentation
- Added a complete **user & developer manual** under `docs/` (getting started, full `.jaffar`
  configuration reference, rules/conditions/rewards, search concepts & tuning, adding a game or
  emulator, and a tooling reference), illustrated with schematic diagrams.
- Published the manual online via **mkdocs-material on GitHub Pages**, with a generated **Doxygen
  C++ API reference** served alongside it under `/api`.
- Added **anti-drift CI checks** that keep the configuration reference in sync with the code (every
  documented core key is verified against the source, and shipped example configs must pass
  `--dryRun`).

### Build & developer experience
- **Automatic game and emulator registration** — adding a game or emulator is now just dropping a
  header into the right directory; the include/detection lists are generated at configure time (no
  manual list editing).
- Added the **`-Dgame=`** option to compile a single game instead of all games of a core
  (`-Demulator=` selects the core).
- Clearer configuration and command-line **error messages** (invalid keys/operators/actions now list
  the valid options).
- `--dryRun` validates a full configuration without running the engine, host-independently.

### Engine improvements
- **NUMA-aware two-tier hash deduplication** (a per-domain L1 in front of a shared global L2) is now
  the default, with explicit size/budget controls — the state-deduplication path scales with core
  count instead of contending on a single shared structure.
- **Robust multi-threaded execution** on unpinned / oversubscribed machines: per-thread and
  per-NUMA-domain identity is taken from the dense OpenMP thread id rather than `sched_getcpu()`,
  fixing crashes and spurious "ran out of states" failures; CI pins threads to match the engine's
  NUMA model.
- **Removed differential compression** of states entirely, along with the state-corruption bugs that
  surrounded it — the engine is simpler and the stored state is now what the game/emulator serializes.
- **Throughput work** in the hot loop: batched base-state popping, atomic-free per-thread statistics
  counters (reduced once per step), a free-slot cache, and a drain buffer.
- **Pre-decoded inputs**: inputs are registered to numeric indices once and referenced by index,
  instead of re-decoding input strings on every step.
- **`Bypass Hash Calculation`** option for games where the game can supply a faithful state hash more
  cheaply than the engine's MetroHash pass.
- **Checkpoints** (`Trigger Checkpoint` + tolerance) and **Candidate Input Sets** (probe extra inputs
  per state, gated by `Test Candidate Inputs`) added to the rule/runner model.
- Per-emulator **state-block disable/enable** (`Disabled State Properties`) consolidated into the
  common emulator class, so any core can shrink the searched state by excluding unused memory blocks.
- Fine-grained hot-loop profiling is now a **compile-time option** (`-DdetailedProfiling`, off by
  default) with negligible cost otherwise.

### Tooling
- **Headless frame screenshots** in the player (`--screenshotDir` / `--screenshotSteps`) and a
  `make_video.py` helper to turn a solution into a video.
- A `--printFinalState` oracle and richer playback diagnostics (not-allowed-input and repeated-state
  reporting; first win/fail step).

### New emulator cores
Added since 1.0 (joining the existing NES/QuickerNES, SNES/QuickerSnes9x,
Genesis+SegaCD+SMS+GameGear+SG-1000/QuickerGPGX, Atari 2600/QuickerStella+Atari2600Hawk,
Prince of Persia/QuickerSDLPoP, Super Mario Bros/QuickerSMBC, Arkanoid/QuickerArkBot cores):

- **QuickerGambatte** — Game Boy / Game Boy Color.
- **QuickerMGBA** — Game Boy Advance.
- **QuickerDSDA** — Doom / Doom II.
- **QuickNES** — the original QuickNES core, alongside QuickerNES.
- **TestEmulator** — an in-repo, ROM-free cursor-on-a-grid core used to test the engine itself.
- **PipeBot** — a self-contained puzzle core.
- The Another World interpreter was split into **QuickerRAWGL** (most ports) and **QuickerNEORAW**
  (DOS), replacing the earlier single core.

### New game integrations
Around **45 new games** were added across the cores, including (a selection):

- **NES** — Metroid, Donkey Kong, Ninja Gaiden III, Gimmick!, Batman, Double Dragon, Excitebike,
  Ice Climber, Kung Fu, Marble Madness, Pac-Man Championship, Street Fighter 2010, Side Pocket,
  Pinball, Pipe Dream, Lunar Ball, Beetlejuice, Darkwing Duck, Indy Heat, Saint Seiya, Micro
  Machines, Best of the Best, Arkanoid II.
- **Sega Genesis** — Sonic the Hedgehog, Micro Machines, Segapede, Shove It, Best of the Best,
  Prince of Persia (incl. USA), and more.
- **SNES** — Arkanoid: Doh It Again, Super Off Road, Best of the Best.
- **Atari 2600** — Alien, Galaxian, Space Invaders, Spider-Man.
- **Game Boy / Color** — A Slime Travel, Prince of Persia. **Game Boy Advance** — Toll Runner.
- **Sega Master System** — Snail Maze. **Game Gear** — Prince of Persia.
- **Doom** (via QuickerDSDA), plus **PipeBot** and the **GridWalker** test game.

### Examples
- The bundled, ready-to-run `.jaffar` configurations grew from **13 to over 700**, spanning every
  supported platform.
- **Prince of Persia**: added a "Disable Non-Gameplay RNG" option and gameplay-tile hashing controls,
  and reconstructed the published **world-record TAS** scripts/solutions for the current engine.

### Testing & CI
- Added a ROM-free **TestEmulator + GridWalker** harness that exercises the engine's full execution
  path with a provably-optimal solution, plus unit tests for properties/conditions, rules/input sets,
  and the hash database.
- Added **AddressSanitizer/UndefinedBehaviorSanitizer**, detailed-profiling, and TestEmulator
  coverage CI jobs, and pinned the clang-format style check.

[2.0.0]: https://github.com/ToolAssisted-run/jaffarPlus/releases/tag/v2.0.0
