# 1. Getting Started

This chapter takes you from a fresh clone to a completed search and a replayed solution. The whole
walkthrough uses the in-repo **`TestEmulator`**, so you need **no game ROM and no emulator
submodule** — it works on any machine.

- [Prerequisites](#prerequisites)
- [Getting the source](#getting-the-source)
- [Building](#building)
  - [Choosing an emulator core](#choosing-an-emulator-core)
  - [Building a single game](#building-a-single-game)
- [Your first search](#your-first-search)
- [Reading the output](#reading-the-output)
- [Validating a configuration](#validating-a-configuration-without-running)
- [Replaying a solution](#replaying-a-solution)
- [Where to go next](#where-to-go-next)

## Prerequisites

JaffarPlus is a C++20 project built with Meson and Ninja. You will need:

| Dependency | Purpose | Notes |
|------------|---------|-------|
| A C++20 compiler | Building | GCC or Clang. The binaries are compiled with `-march=native`. |
| [Meson](https://mesonbuild.com) + [Ninja](https://ninja-build.org) | Build system | `pip install meson ninja` works. |
| `libnuma` (+ headers) | NUMA-aware memory allocation | The `jaffar` binary links against it directly (`dependency('numa')`). On Debian/Ubuntu: `apt install libnuma-dev`. |
| NCurses | Console UI (optional) | On by default; disable with `-DuseNCurses=false`. |
| Emulator-specific libs | Per core | Some cores (e.g. QuickerSDLPoP) need SDL2. The `TestEmulator` needs nothing extra. |

A real game search additionally needs the **game's data** (a ROM, disk image, or game files) and
usually an **initial state file** — these are *not* shipped with the repository. The `TestEmulator`
used in this chapter needs neither.

## Getting the source

```bash
git clone https://github.com/ToolAssisted-run/jaffarPlus.git
cd jaffarPlus
```

JaffarPlus uses git submodules under `extern/` — the shared `jaffarCommon` library plus one
submodule per emulator core. You only need the submodules for the core you intend to build:

```bash
# Always needed:
git submodule update --init extern/jaffarCommon

# Needed only for the core you build, e.g. for Prince of Persia:
git submodule update --init extern/quickerSDLPoP
```

The `TestEmulator` is in-repo and needs no submodule beyond `jaffarCommon`.

## Building

JaffarPlus is configured per *build directory*. A build directory is tied to a single emulator
core (and, optionally, a single game), so you typically keep one build directory per core you work
with.

```bash
meson setup build -Demulator=TestEmulator
ninja -C build
```

This produces two executables in `build/`:

- **`jaffar`** — the search engine. Takes a `.jaffar` configuration file and searches for a solution.
- **`jaffar-player`** — replays and inspects solutions; can render frames and capture screenshots.

### Choosing an emulator core

The `-Demulator=` option selects which emulation core is compiled in. The available choices are:

```
QuickNES  QuickerNES  QuickerNESArkanoid  NesHawk  QuickerGPGX  QuickerSDLPoP  QuickerSnes9x
QuickerStella  Atari2600Hawk  QuickerSMBC  QuickerNEORAW  QuickerRAWGL  QuickerArkBot
QuickerMGBA  QuickerGambatte  QuickerDSDA  PipeBot  ExciteBikeNative  TestEmulator
```

`NesHawk` is a C++ translation of BizHawk's cycle-accurate NesHawk NES core
([QuickerNesHawk](https://github.com/ToolAssisted-run/QuickerNesHawk)). It is slower than `QuickerNES`
but validated byte-exact against the reference emulator, so it is the core to use when a search
result must be confirmed on ground-truth NES hardware timing rather than an approximation.

Each core builds the games written for it. For example:

```bash
meson setup build-nes  -Demulator=QuickerNES     # all NES games
meson setup build-pop  -Demulator=QuickerSDLPoP  # Prince of Persia
```

To switch an *existing* build directory to a different core, reconfigure it:

```bash
meson configure build -Demulator=QuickerNES
ninja -C build
```

### Building a single game

A core can have many games, and rebuilding all of them when you only care about one is wasteful.
Use `-Dgame=` to compile just one, identified by its header file stem:

```bash
# Compile only the 'sprilo' game of the QuickerNES core
meson setup build-sprilo -Demulator=QuickerNES -Dgame=sprilo
ninja -C build-sprilo
```

`-Dgame=` matches the game's source file name (case-insensitive); leaving it empty (the default)
builds every game of the selected core. Game and emulator registration is fully automatic — see
[Adding a Game](05-adding-a-game.md) — so there is nothing to "wire up" by hand.

## Your first search

The repository ships a self-contained puzzle, [`docs/examples/gridwalker.jaffar`](examples/gridwalker.jaffar),
that runs on the `TestEmulator`. The "game" is a cursor on a 5×5 grid that can move up, down, left
or right; the goal is to reach the bottom-right corner `(4,4)` from the top-left `(0,0)`. The
optimal solution is exactly 8 moves, which makes it a perfect first run.

```bash
./build/jaffar docs/examples/gridwalker.jaffar
```

The engine explores the reachable grid positions and, because best-first search returns a shortest
path, finds an optimal 8-move solution. By default it stops on the first win
(`"Stop Frames After First Win": 0`) and writes the solution to `/tmp/jaffar.gridwalker.best.sol`
(set by `"Best Solution Path"`).

## Reading the output

A run prints a banner describing the loaded game and emulator, then a periodic status report, and
finally an exit line. The most important fields:

- **Step** — the current search depth (how many inputs deep the frontier is).
- **State count / database usage** — how many distinct states are being held, and how full the
  state database is.
- **Best reward / best solution length** — the reward of the best state found so far and how many
  inputs it took to reach it.
- **Exit Reason** — one of:
  - `Solution found.` — a win state was reached.
  - `Engine ran out of states.` — the frontier emptied before a win (no solution reachable under
    the given inputs/rules, or the state database was too small).
  - `Maximum step count reached.` — hit the `"Max Steps"` limit first.

## Validating a configuration without running

Before committing to a long search, check that a configuration parses and builds the full engine
(driver, emulator, game, rules, inputs) with `--dryRun`:

```bash
./build/jaffar docs/examples/gridwalker.jaffar --dryRun
```

A dry run loads and validates the *entire* configuration but does **not** initialize NUMA, load
trace files, or run the search — so it is fast and host-independent. On success it prints
`Finished dry run successfully.`; on a malformed or incomplete configuration it reports the offending
key. This is the same check CI runs against every documented example (see
[the accuracy note](README.md#a-note-on-accuracy)).

> **Tip — unrecognized keys are rejected.** The parser reads keys by name and **fails on any key it
> does not recognize**, in every section, reporting `[Error] Unrecognized key(s) in <section>: '...'`.
> A misspelled or stale key is a hard error, not a silent no-op — so `--dryRun` catches typos
> immediately. If a key is rejected, check its spelling and nesting against the
> [Configuration Reference](02-config-reference.md).

## Replaying a solution

A `.sol` file is a sequence of inputs. Replay it with `jaffar-player`:

```bash
./build/jaffar-player docs/examples/gridwalker.jaffar /tmp/jaffar.gridwalker.best.sol \
    --reproduce --unattended --exitOnEnd
```

- `--reproduce` plays from the start of the solution.
- `--unattended` skips the interactive prompt (good for scripts/CI).
- `--exitOnEnd` quits when the last input is consumed.

For cores with a renderer you can watch the playback in a window (omit `--disableRender`), step
frame-by-frame interactively, or capture frames to disk with `--screenshotDir`. See the
[Tooling Reference](06-tooling.md) for every player flag and the video-rendering helper.

## Where to go next

- To understand every knob in a `.jaffar` file: [Configuration Reference](02-config-reference.md).
- To learn how to *steer* a search toward a goal: [Rules, Conditions & Rewards](03-rules-and-rewards.md).
- To make a search faster or fit in memory: [Search Concepts & Tuning](04-search-concepts.md).
- To add your own game: [Adding a Game](05-adding-a-game.md).
