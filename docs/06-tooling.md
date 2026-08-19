# 6. Tooling Reference

JaffarPlus ships two executables — `jaffar` (the search engine) and `jaffar-player` (replay,
inspection, and rendering) — plus a helper for turning solutions into video. This chapter is the
flag-by-flag reference.

- [`jaffar` (the engine)](#jaffar-the-engine)
- [`jaffar-player`](#jaffar-player)
- [Headless screenshots](#headless-screenshots)
- [Rendering a solution to video](#rendering-a-solution-to-video)
- [Solution files](#solution-files)
- [Environment overrides](#environment-overrides)

## `jaffar` (the engine)

```
jaffar <configFile> [--dryRun]
```

| Argument | Description |
|----------|-------------|
| `configFile` | Path to the `.jaffar` configuration to run. Required. |
| `--dryRun` | Load and validate the entire configuration (driver, emulator, game, rules, inputs) **without** initializing NUMA, loading trace files, or running the search. Prints `Finished dry run successfully.` on success. |

`--dryRun` is the recommended first step on any new or edited configuration — it catches missing,
mistyped, and **unrecognized** keys quickly and host-independently (it constructs the driver, game,
and emulator, which is where both the required-key and unknown-key checks run). See the
[Configuration Reference](02-config-reference.md).

On a real run, the exit line reports one of `Solution found.`, `Engine ran out of states.`, or
`Maximum step count reached.` — see [Getting Started](01-getting-started.md#reading-the-output) and
[Search Concepts](04-search-concepts.md#states-and-the-state-database) for what each means.

## `jaffar-player`

Replays a solution against a configuration, optionally rendering it.

```
jaffar-player <configFile> <solutionFile> [options]
```

| Flag | Default | Description |
|------|---------|-------------|
| `configFile` | — | The `.jaffar` configuration (required). |
| `solutionFile` | — | The `.sol` input sequence to replay (required). |
| `--reproduce` | off | Play from the start of the solution. |
| `--reload` | off | Reload the solution from the beginning after reaching the end. |
| `--exitOnEnd` | off | Exit when the last input is consumed (instead of waiting). |
| `--unattended` | off | Don't print the interactive prompt or wait for keypresses (use in scripts/CI). |
| `--disableRender` | off | Do not open/render the game window. |
| `--frameskip <n>` | `1` | Render every `n`-th frame (display/screenshot cadence). |
| `--initialSequence <file>` | — | Play this sequence as a prefix before the main solution. |
| `--runCommand <cmd>` | — | Run a single command and exit. |
| `--screenshotDir <dir>` | — | Write per-frame screenshots (BMP) into this directory. Requires rendering enabled. |
| `--screenshotSteps <list>` | all | Comma-separated step numbers to capture (empty = every rendered frame). |
| `--printFinalState` | off | Print the final game state on exit. |

A typical scripted, non-interactive replay:

```bash
jaffar-player game.jaffar solution.sol --reproduce --unattended --exitOnEnd
```

## Headless screenshots

Screenshot capture is implemented per emulator via `Emulator::saveScreenshot` and currently backed
by the **QuickerSDLPoP** core (Prince of Persia), which writes `step_NNNNNN.bmp` files. Cores
without a screenshot backend simply ignore the request.

To capture frames without a visible window, use SDL's offscreen video driver:

```bash
SDL_VIDEODRIVER=offscreen \
  jaffar-player game.jaffar solution.sol --reproduce --unattended --exitOnEnd \
  --screenshotDir /tmp/frames --screenshotSteps 840,841,842
```

Each requested step lands as a `.bmp` in `/tmp/frames`. Convert to PNG with any image tool (the
project's tooling uses Python's Pillow).

## Rendering a solution to video

[`examples/sdlpop/analysis/make_video.py`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/examples/sdlpop/analysis/make_video.py) drives
`jaffar-player` headlessly and assembles the captured frames into a video. The output extension
selects the encoder:

- `.gif` — animated GIF via Pillow (no extra dependencies; some mobile viewers show it static).
- `.mp4` / `.mov` / `.m4v` — H.264 (plays inline on phones); needs
  `pip install imageio imageio-ffmpeg`.
- `.webm` — VP9.

```bash
# Render frames 840-920 of a solution at 3x scale to an MP4:
examples/sdlpop/analysis/make_video.py 0300/script.jaffar 0300/solution.sol /tmp/climb.mp4 \
    --start 840 --end 920 --scale 3
```

Useful options: `--start`/`--end` (frame window — capturing only the window you need is much
faster), `--fps`, `--scale` (integer nearest-neighbour upscale), `--player` (path to a specific
`jaffar-player`), and `--workdir` (directory to run from so the config's relative paths resolve).

## Solution files

A `.sol` file is the input sequence the engine found. `jaffar` writes them via:

- `Driver Configuration` > `Save Intermediate Results` > `Best Solution Path` / `Worst Solution Path`
  (periodic, during the run), and
- the `Trigger Save Solution` rule action (`Path`), which captures the solution to a specific
  state/checkpoint when its rule fires.

Feed a `.sol` to `jaffar-player` to watch or render it, or use it as an `--initialSequence` prefix.

## Environment overrides

For testing and benchmarking without editing the config:

| Variable | Effect |
|----------|--------|
| `OMP_NUM_THREADS` | Number of OpenMP worker threads. |
| `OMP_PROC_BIND` / `taskset` | Pin threads to cores (recommended for production and reproducibility). |
| `JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT` | Override the worker thread count. |
| `JAFFAR_DRIVER_OVERRIDE_DRIVER_MAX_STEP` | Override `Driver` > `Max Steps`. |
| `JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB` | Override the state-database size budget. |
| `JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB` | Override the hash-database size budget. |
| `SDL_VIDEODRIVER=offscreen` | Render without a visible window (for headless screenshots/video). |

See [Search Concepts & Tuning](04-search-concepts.md) for when and why to reach for these.
