# 5. Adding a Game or Emulator

This chapter is for contributors. It shows how to add a new **game** (just writing one header — no
lists to edit) and a new **emulator core** (a header plus two short build wirings). Both rely on the
same automatic class registration.

- [How registration works](#how-registration-works)
- [Anatomy of a game class](#anatomy-of-a-game-class)
- [The methods you implement](#the-methods-you-implement)
- [Exposing properties](#exposing-properties)
- [Reward and custom actions (magnets)](#reward-and-custom-actions-magnets)
- [Building and running your game](#building-and-running-your-game)
- [Adding a new emulator core](#adding-a-new-emulator-core)
  - [Wiring the build](#wiring-the-build)
  - [The emulator interface](#the-emulator-interface)
  - [Input handling](#input-handling)
  - [Building and running your core](#building-and-running-your-core)

## How registration works

Games and emulators follow a uniform shape: a class `class <Name> final : public <Base>` inside a
known namespace, compiled behind a per-directory preprocessor guard. At configure time,
[`genRegistry.py`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/genRegistry.py) globs the source tree, finds those classes, and generates two
headers — an `#include` block and a `DETECT_*` block — that `games/gameList.hpp` and
`emulators/emulatorList.hpp` pull in. The generated files live in the build directory, so a
single-game build (`-Dgame=`) never disturbs another build directory.

The practical upshot: **drop a new `.hpp` into the right `games/<platform>/` directory and re-run
`meson setup`** — it is detected automatically. The directory determines the platform guard (e.g.
`games/nes/` → `__JAFFAR_ENABLE_NES`), so your game compiles only for the core(s) that define it.

## Anatomy of a game class

The smallest complete example in the repo is the test game
[`games/test/gridWalker/gridWalker.hpp`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/games/test/gridWalker/gridWalker.hpp). Its shape:

```cpp
#pragma once
#include "testEmulator/testEmulator.hpp"   // the emulator this game runs on
#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>

namespace jaffarPlus { namespace games { namespace test {   // games::<platform>

class GridWalker final : public jaffarPlus::Game
{
public:
  // The string matched by "Game Name" in the config:
  static __INLINE__ std::string getName() { return "Test / GridWalker"; }

  GridWalker(std::unique_ptr<Emulator> emulator, const nlohmann::json& config)
    : jaffarPlus::Game(std::move(emulator), config)
  {
    // Read any game-specific config keys here:
    _goalX = jaffarCommon::json::getNumber<uint8_t>(config, "Goal X");
    _goalY = jaffarCommon::json::getNumber<uint8_t>(config, "Goal Y");
  }

private:
  // ... the overrides below ...
};

}}} // namespace
```

Two contracts matter for registration:

- The class must be `final : public jaffarPlus::Game` and live in `namespace jaffarPlus::games::<dir>`.
- `static getName()` returns the exact string users put in `Game Configuration` > `Game Name`. The
  "game not recognized" error lists all names compiled into the build, so pick something
  descriptive.

Any custom config keys (like `Goal X` above) are read in the constructor — and they become part of
your game's documented schema, so add them to the [Configuration Reference](02-config-reference.md).

## The methods you implement

`Game` declares seven pure-virtual methods every game must override *(source: `source/game.hpp`)*:

| Method | Purpose |
|--------|---------|
| `registerGameProperties()` | Declare the named properties the config can reference (see below). |
| `advanceStateImpl(input)` | Advance the game by one input/step (usually calls `_emulator->advanceState(input)` plus bookkeeping). |
| `serializeStateImpl(serializer)` | Persist any game-owned state not already in the emulator's save-state. |
| `deserializeStateImpl(deserializer)` | Restore that game-owned state. |
| `calculateGameSpecificReward()` | Return the game's contribution to a state's reward (magnets, distance heuristics). |
| `computeAdditionalHashing(hashEngine)` | Fold any extra game-owned bytes into the state hash (often empty if all dedup keys are registered properties). |
| `parseRuleActionImpl(rule, type, json)` | Parse game-specific rule actions; return `false` for unrecognized types. |
| `printInfoImpl()` | Print game-specific fields in the status banner. |

There are also optional hooks worth knowing:

- `stateUpdatePostHook()` — recompute derived values after every state change/deserialize (GridWalker
  uses it to recompute distance-to-goal).
- `getStateInputHash()` / `getDirectStateHash()` — fine-grained hashing for candidate-input testing
  and for `Bypass Hash Calculation`; keep them keyed on the same identity as your `Hash Properties`.

## Exposing properties

`registerGameProperties()` maps memory into named, typed properties. You typically grab pointers
into emulator memory and register them:

```cpp
void registerGameProperties() override
{
  auto position = (uint8_t*)_emulator->getProperty("Position").pointer;
  registerGameProperty("Pos X", &position[0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
  registerGameProperty("Pos Y", &position[1], Property::datatype_t::dt_uint8, Property::endianness_t::little);
  registerGameProperty("Distance", &_distance, Property::datatype_t::dt_int32, Property::endianness_t::little);
}
```

Once registered, a property name can be used in `Hash Properties`, `Print Properties`, conditions,
and magnets. Datatypes and endianness are listed in the
[Configuration Reference](02-config-reference.md#property-datatypes). The error on an unknown
property name lists every registered property, which is handy while authoring configs.

## Reward and custom actions (magnets)

The engine forms a state's reward from rule `Add Reward` actions **plus** whatever
`calculateGameSpecificReward()` returns. GridWalker just returns `-(distance to goal)` so closer is
better. Richer games sum their magnets here.

To add **magnets** (or any custom action `Type`), implement `parseRuleActionImpl`. It is a chain of
string comparisons; return `true` once you recognize and handle an action, `false` otherwise:

```cpp
bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
{
  if (actionType == "Set Player Pos X Magnet")
  {
    auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
    auto position  = jaffarCommon::json::getNumber<uint8_t>(actionJs, "Position");
    rule.addAction([=, this]() { _playerPosXMagnet = pointMagnet_t{.intensity = intensity, .position = position}; });
    return true;
  }
  return false; // unrecognized -> let the base class report it
}
```

Then in `calculateGameSpecificReward()`, add `intensity * -|current - position|` for each active
magnet (see `games/sdlpop/princeOfPersia.hpp` for the canonical pattern, including room-gating).
Use the consistent naming convention `Set <Thing> Magnet` and document the new action's keys in
the configuration reference / chapter 3.

## Building and running your game

No list edits are needed — just configure and build:

```bash
# Build only your new game on its core:
meson setup build-mygame -Demulator=QuickerNES -Dgame=myGame
ninja -C build-mygame
./build-mygame/jaffar myGame.jaffar --dryRun   # validate the config first
```

`-Dgame=` matches the header file stem (case-insensitive). Omit it to build all games of the core.
If your game isn't detected, confirm the file is in the right `games/<platform>/` directory, the
class is `final : public jaffarPlus::Game`, and you re-ran `meson setup` (the registry is generated
at configure time).

## Adding a new emulator core

An emulator core wraps a console/game engine behind JaffarPlus's `Emulator` interface so the search
can save/load state, advance a frame, and read memory. It uses the same automatic-registration model
as a game — a class `final : public Emulator` in `namespace jaffarPlus::emulator`, under
`emulators/<core>/` — plus two short build wirings, because each build compiles exactly one core
(selected by `-Demulator=`).

The minimal, ROM-free reference implementation is
[`emulators/testEmulator/testEmulator.hpp`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/emulators/testEmulator/testEmulator.hpp) — read it
alongside this section.

### Wiring the build

Two edits register a new core (say `MyCore`, in `emulators/myCore/`):

1. **Add it as a build option** — append `'MyCore'` to the `emulator` combo's `choices` in
   [`meson_options.txt`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/meson_options.txt), so `-Demulator=MyCore` is accepted.

2. **Wire its compile guards** in [`emulators/meson.build`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/emulators/meson.build):

   ```meson
   if emulator == 'MyCore'
   jaffarCPPFlags += '-D__JAFFAR_USE_MYCORE'          # registers the emulator class
   jaffarCPPFlags += '-D__JAFFAR_ENABLE_MYPLATFORM'   # enables games written for this platform
   subdir('myCore')
   endif
   ```

   - `__JAFFAR_USE_<CORE>` is the **registration guard**. [`genRegistry.py`](https://github.com/ToolAssisted-run/jaffarPlus/blob/master/genRegistry.py)
     derives it from the directory name, uppercased — `emulators/myCore/` → `__JAFFAR_USE_MYCORE`.
     (A few legacy names have overrides at the top of `genRegistry.py`, e.g. `quickerArkBot` →
     `__JAFFAR_USE_ARKBOT`; new cores should just follow the default convention.)
   - `__JAFFAR_ENABLE_<PLATFORM>` is the guard that **games** are compiled behind (see
     [How registration works](#how-registration-works)), so it decides which games build for this
     core. Reuse an existing platform if your core runs the same games as another (e.g. both NES
     cores define `__JAFFAR_ENABLE_NES`), or introduce a new one and tag your games' directory with
     it in `genRegistry.py`'s `GAME_DIR_GUARD`.

That is the only manual wiring. Once `__JAFFAR_USE_MYCORE` is defined and you re-run `meson setup`,
the class is detected and registered automatically — no list to edit.

### The emulator interface

The core is a class in `namespace jaffarPlus::emulator` whose **constructor only parses
configuration** (so `--dryRun` validates without loading a ROM), with a `static getName()` returning
the exact string users put in `Emulator Configuration` > `Emulator Name`:

```cpp
namespace jaffarPlus { namespace emulator {

class MyCore final : public Emulator
{
public:
  static std::string getName() { return "MyCore"; }

  MyCore(const nlohmann::json& config) : Emulator(config)
  {
    // Parse config keys only (ROM path, initial state, etc.). No heavy init here.
  }

private:
  // ... the overrides below ...
};

}} // namespace
```

It implements the pure-virtual `Emulator` interface *(source: `source/emulator.hpp`)*:

| Method | Purpose |
|--------|---------|
| `initializeImpl()` | Load the ROM/game data and bring the core to its initial state. Called after construction; skipped on dry runs. |
| `advanceStateImpl(input)` | Advance the emulation by one frame, applying the decoded `input`. |
| `serializeState(serializer)` / `deserializeState(deserializer)` | Save / restore the searched emulator state — this is what defines a "state" (and its size). |
| `getProperty(name)` | Expose a region of emulator memory as a named, typed property (pointer + datatype) so games and configs can read it. |
| `getInputParser()` | Return the core's `jaffar::InputParser`, which decodes the input strings from `Allowed Input Sets`. |
| `printInfo()` | Print core-specific information in the status banner. |

Cores that render also implement the video hooks (`initializeVideoOutput` / `finalizeVideoOutput`,
`enableRendering` / `disableRendering`, `updateRendererState`, `serializeRendererState` /
`deserializeRendererState`, `getRendererStateSize`, `showRender`) and may override `saveScreenshot`
to support the screenshot/video tooling ([chapter 6](06-tooling.md#headless-screenshots)). A
headless-only core can stub these minimally — see how `testEmulator` does it.

### Input handling

Inputs in a `.jaffar` config are strings (e.g. `|U|`, or `U.L.F` for a console pad). The core's
`InputParser` — returned by `getInputParser()` — defines that string format and decodes each string
into the core's native input type. The `testEmulator`'s `inputParser.hpp` is the minimal example: it
maps single pipe-delimited characters to grid moves.

### Building and running your core

```bash
meson setup build-mycore -Demulator=MyCore
ninja -C build-mycore
./build-mycore/jaffar myConfig.jaffar --dryRun   # validate before a full run
```

If the core isn't recognized at runtime, the "not recognized" error lists every core compiled into
the build — confirm `__JAFFAR_USE_MYCORE` is defined (step 2) and that you re-ran `meson setup` (the
registry is regenerated at configure time). Remember to document your core's `Emulator
Configuration` keys in the [Configuration Reference](02-config-reference.md).
