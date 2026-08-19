
[![Build and Run Tests](https://github.com/ToolAssisted-run/jaffarPlus/actions/workflows/make.yml/badge.svg)](https://github.com/ToolAssisted-run/jaffarPlus/actions/workflows/make.yml) [![codecov](https://codecov.io/gh/SergioMartin86/jaffarPlus/graph/badge.svg?token=B9KMR864ZP)](https://codecov.io/gh/SergioMartin86/jaffarPlus)

# JaffarPlus

![](jaffar.png)

**JaffarPlus is a high-performance, parallel search engine for producing tool-assisted speedruns (TAS).**

You point it at a game (through an emulation core), describe which inputs are legal and what "better"
means (a *reward*), and it explores the reachable game states across all of your CPU cores to find
the best input sequence it can — for a TAS, usually the fastest route to a win. It is a
*reward-guided breadth-first search*: it explores by increasing solution length, so the first
solution it finds is a shortest one, while always expanding the most promising states first.

## Highlights

* **Fast and parallel** — built for many-core CPUs, with NUMA-aware memory and hash-based
  deduplication of already-seen states so it scales to large search spaces.
* **Multi-platform** — one engine drives 15+ emulation cores: NES, SNES, Sega Genesis / Master
  System / Game Gear, Game Boy / Color / Advance, Atari 2600, Doom, Prince of Persia, and more.
* **Reward-guided** — you steer the search declaratively with rules, conditions, and *reward
  magnets* in a JSON config; no recompilation needed to retune a search.
* **Extensible** — adding a new game is dropping one header into `games/`; registration is fully
  automatic. Any emulator exposing save/load state + advance-frame can be wrapped behind a common API.
* **Reproducible** — every solution replays deterministically and can be rendered to frames or video
  for analysis.

## Quick start (no ROM required)

JaffarPlus ships a self-contained puzzle on its in-repo test core, so you can build and run a real
search on any machine:

```bash
# Build with the ROM-free test core
meson setup build -Demulator=TestEmulator
ninja -C build

# Validate the configuration, then run the search (finds the optimal 8-move path on a 5x5 grid)
./build/jaffar docs/examples/gridwalker.jaffar --dryRun
./build/jaffar docs/examples/gridwalker.jaffar

# Replay the solution it found
./build/jaffar-player docs/examples/gridwalker.jaffar /tmp/jaffar.gridwalker.best.sol \
    --reproduce --unattended --exitOnEnd
```

See **[Getting Started](https://toolassisted-run.github.io/jaffarPlus/01-getting-started/)** for the full walkthrough, including how to
build for a specific console core and game.

# Documentation

📖 **Browse it online:** the manual is published at
**https://toolassisted-run.github.io/jaffarPlus/**, with the C++ engine API reference at
**https://toolassisted-run.github.io/jaffarPlus/api/**.

A complete user & developer manual lives in **[`docs/`](https://toolassisted-run.github.io/jaffarPlus/)**:

| Chapter | What it covers |
| ------- | -------------- |
| [1. Getting Started](https://toolassisted-run.github.io/jaffarPlus/01-getting-started/) | Build JaffarPlus, run your first search (no ROM), read the output, replay a solution. |
| [2. Configuration Reference](https://toolassisted-run.github.io/jaffarPlus/02-config-reference/) | Every section and key of a `.jaffar` file: type, default, and meaning. |
| [3. Rules, Conditions & Rewards](https://toolassisted-run.github.io/jaffarPlus/03-rules-and-rewards/) | Steering the search: properties, conditions, reward actions, and magnets (illustrated). |
| [4. Search Concepts & Tuning](https://toolassisted-run.github.io/jaffarPlus/04-search-concepts/) | How the search, state hashing, and NUMA/threading work — and which knobs to turn. |
| [5. Adding a Game or Emulator](https://toolassisted-run.github.io/jaffarPlus/05-adding-a-game/) | Register a new game or emulator and expose its properties and actions. |
| [6. Tooling Reference](https://toolassisted-run.github.io/jaffarPlus/06-tooling/) | `jaffar`, `jaffar-player`, headless screenshots, video rendering, environment overrides. |
| [7. Understanding the Output](https://toolassisted-run.github.io/jaffarPlus/07-understanding-output/) | What every field of `jaffar`'s per-step report means, and how to read a run at a glance. |
| [8. Jaffar Player Interface](https://toolassisted-run.github.io/jaffarPlus/08-player-interface/) | `jaffar-player`'s interactive commands, options, and workflows: replay, navigate, extract a save-state, screenshots. |

A C++ **API reference** for the engine internals is generated from the source with Doxygen — browse
it [online](https://toolassisted-run.github.io/jaffarPlus/api/), or build it locally with
`doxygen Doxyfile` (output in `build/doxygen/html/`).

# Built-in Emulator Support

## Consoles

| Console                  | Core(s)                                                                      |
| --------                 | -------                                                                      |
| Atari 2600               | [QuickerStella](https://github.com/ToolAssisted-run/quickerStella)             |
| Atari 2600               | [Atari2600Hawk](https://github.com/CasualPokePlayer/libAtari2600Hawk)        |
| NES                      | [QuickerNES](https://github.com/ToolAssisted-run/quickerNES)                   |
| NES                      | [QuickerNesHawk](https://github.com/ToolAssisted-run/QuickerNesHawk)           |
| SNES                     | [QuickerSnes9x](https://github.com/ToolAssisted-run/quickerSnes9x)             |
| Sega Genesis             | [QuickerGPGX](https://github.com/ToolAssisted-run/quickerGPGX)                 |
| Sega CD                  | [QuickerGPGX](https://github.com/ToolAssisted-run/quickerGPGX)                 |
| Sega SG-1000             | [QuickerGPGX](https://github.com/ToolAssisted-run/quickerGPGX)                 |
| Sega Master System       | [QuickerGPGX](https://github.com/ToolAssisted-run/quickerGPGX)                 |
| Gameboy Advance          | [QuickerMGBA](https://github.com/ToolAssisted-run/quickerMGBA)                 |
| Gameboy / Gameboy Color  | [QuickerGambatte](https://github.com/ToolAssisted-run/quickerGambatte)         |

## Game-Specific

| Game                   | Core(s)                                                          | Target                       | Notes |
| ---------------------- | ---------------------------------------------------------------- | ---------------------------- | ----- |
| Prince of Persia       | [QuickerSDLPoP](https://github.com/ToolAssisted-run/quickerSDLPoP) | LibTAS+PCem                  | Many PoP ports share this same (Apple II / DOS) game logic |
| Another World          | [QuickerNEORAW](https://github.com/ToolAssisted-run/QuickerNEORAW) | DOS                          | This AW interpreter only works with DOS files |
| Another World          | [QuickerRAWGL](https://github.com/ToolAssisted-run/quickerRAWGL)   | Multiple                     | This AW interpreter works with most AW ports |
| Super Mario Bros (NES) | [QuickerSMBC](https://github.com/ToolAssisted-run/quickerSMBC)     | Bizhawk 2.9.2                | Inaccurate in transitions, but good for solving levels |
| Arkanoid (NES)         | [QuickerArkbot](https://github.com/ToolAssisted-run/QuickerArkBot) | Bizhawk 2.9.2 (NesHawk Core) | |
| Doom                   | [QuickerDSDA](https://github.com/ToolAssisted-run/quickerDSDA)     | Doom / Doom II               | |
| Sokoban                | [QuickerBan](https://github.com/SergioMartin86/quickerBan)       | Sokoban (all)                | |

Author
=============

- Sergio Martin (eien86)
  + Github: https://github.com/SergioMartin86
  + Twitch: https://www.twitch.tv/eien86
  + Youtube: https://www.youtube.com/channel/UCSXpK3d6vUq58fjgF5jFoKA
  + TASVideos: https://tasvideos.org/Users/Profile/eien86

- A list of TAS movies produced by eien86 using JaffarPlus can be found [here](https://tasvideos.org/Subs-List?user=eien86&statusfilter=6)

- Contributions via pull requests are highly appreciated.

- Thanks to:
  + This work is based on [Jaffar](https://github.com/SergioMartin86/jaffar), a solver for the original Prince of Persia (DOS).
  + TASVideos' staff (judges, encoders, admins, etc)
  + The Bizhawk development team (YoshiRulz, feos, Morilli, CasualPokePlayer, NattTheBear, Alyosha, feos, zeromus, and many others)
  + Dávid Nagy and all SDLPoP developers
  + Gregory Montoir and Fabien Sanglard (authors of Fabother World)
  + Eke-Eke and all Genesis Plus GX developers
  + Shay Green, Christopher Snowhill and all QuickNES developers
  + sbroger (a.k.a Chef Stef), developer of Arkbot
  + Mitchell Sternke, developer of SMB-C
  + sinamas, et al. for Gambatte-Speedrun
  + Vicki Pfau, et al. for MGBA
  + Alexander Lyashuk (mooskagh, crem) for kickstarting the idea of creating a TASing bot.
  + The authors of DSDA
  + The authors of the third party libraries used.

- JaffarPlus's own source code is distributed freely under the [MIT License](LICENSE) for any purpose and use, as long as:
  + The license and proper credits to its author are preserved
  + If you publish a TAS or any public work using this software, I'd appreciate you mention it and linking this repository in your description

- A note on the emulator cores: JaffarPlus builds against emulation cores that are included as
  separate git submodules and are licensed under the GPL (v2 or v3, depending on the core). The MIT
  license above covers only the source code in this repository. A binary you build links one of
  those cores and is therefore a derivative work under that core's GPL — so if you choose to
  distribute such a binary, it must be distributed under the corresponding GPL terms.

- Development note: parts of this software were developed with the assistance of AI tools (Anthropic's Claude / Claude Code).
