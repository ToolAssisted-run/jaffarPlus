# JaffarPlus User & Developer Manual

JaffarPlus is a high-performance, general-purpose best-first search optimizer, specially
tailored for producing tool-assisted speedruns (TAS). You give it a game (through an emulation
core), a description of which inputs are allowed, and a reward function that says what "better"
means; it searches the reachable game states in parallel and returns the best input sequence it
can find (for a TAS: usually the fastest path to a win).

This manual is the missing piece between the [project README](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/README.md) (what JaffarPlus is
and which cores it supports) and the 700+ example configurations under [`examples/`](https://github.com/ToolAssisted-run/jaffarPlus/tree/master/examples):
it explains *how* to drive the tool and *why* it behaves the way it does.

## How this manual is organized

The chapters are ordered for a first-time reader. The first half is for **users** (people who run
searches and write configurations); the second half is for **contributors** (people who extend the
engine or add new games).

| # | Chapter | Audience | What you'll learn |
|---|---------|----------|-------------------|
| 1 | [Getting Started](01-getting-started.md) | User | Build JaffarPlus, run your first search with no ROM required, read the output, replay a solution. |
| 2 | [Configuration Reference](02-config-reference.md) | User | Every section and key of a `.jaffar` file: type, default, meaning. |
| 3 | [Rules, Conditions & Rewards](03-rules-and-rewards.md) | User | How to steer the search: properties, conditions, reward actions and magnets. |
| 4 | [Search Concepts & Tuning](04-search-concepts.md) | User / Contributor | How best-first search, state hashing, and the NUMA/threading model work — and which knobs to turn. |
| 5 | [Adding a Game or Emulator](05-adding-a-game.md) | Contributor | Register a new game or emulator (now fully automatic) and expose its properties and actions. |
| 6 | [Tooling Reference](06-tooling.md) | User / Contributor | `jaffar`, `jaffar-player`, headless screenshots, video rendering, and environment overrides. |
| 7 | [Understanding the Output](07-understanding-output.md) | User | What every field of `jaffar`'s per-step report means, and how to read a run at a glance. |
| 8 | [Jaffar Player Interface](08-player-interface.md) | User | `jaffar-player`'s interactive commands, options, and common workflows: replay, navigate, extract a save-state, screenshots. |

An auto-generated **[C++ API reference](https://toolassisted-run.github.io/jaffarPlus/api/)** (Doxygen)
covers the engine internals in `source/`; see [Adding a Game or Emulator](05-adding-a-game.md) for
the entry points.

## The 60-second tour

```bash
# 1. Build with the ROM-free test core
meson setup build -Demulator=TestEmulator
ninja -C build

# 2. Validate a configuration without running a search
./build/jaffar docs/examples/gridwalker.jaffar --dryRun

# 3. Run the search (finds the optimal 8-move path across a 5x5 grid)
./build/jaffar docs/examples/gridwalker.jaffar

# 4. Replay the solution it found
./build/jaffar-player docs/examples/gridwalker.jaffar /tmp/jaffar.gridwalker.best.sol --reproduce --unattended --exitOnEnd
```

Everything above runs with no game ROM and no emulator submodule — it uses the in-repo
`TestEmulator`. Start with [Getting Started](01-getting-started.md) for the full walkthrough.

## A note on accuracy

The configuration keys documented here are validated against the source in continuous integration
by two checks (run via `ninja test`, suite `docs`):

- **Forward** (`DocsExamples`): every example configuration under [`docs/examples/`](examples) must
  pass `jaffar --dryRun`, so a renamed or removed key breaks the example — and the build.
- **Backward** (`DocsConfigReference`): a coverage check confirms that every configuration key the
  engine *core* (`source/*.hpp`) reads is documented in
  [the configuration reference](02-config-reference.md), and flags documented core keys that have
  disappeared from the code. (Emulator- and game-specific keys live in `emulators/` and `games/` and
  are documented as patterns rather than checked key-by-key.)

If you find a discrepancy, it is a bug in the docs *or* the check — please report it.
