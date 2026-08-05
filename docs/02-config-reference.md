# 2. Configuration Reference

A `.jaffar` file is a single JSON object with five top-level sections. This chapter documents every
key the engine reads in each section. The reward/rule system (the `Game Configuration` > `Rules`
array) is large enough to have its own chapter — see [Rules, Conditions & Rewards](03-rules-and-rewards.md) —
but the structural keys are listed here for completeness.

```json
{
  "Driver Configuration":   { ... },
  "Engine Configuration":   { ... },
  "Emulator Configuration": { ... },
  "Game Configuration":     { ... },
  "Runner Configuration":   { ... }
}
```

All five sections are required. Throughout this chapter, **key names are case- and
space-sensitive** and must match exactly (including parentheses, e.g. `Max Size (Mb)`).

> **Unknown keys are rejected.** The parser reads keys by name and throws on any key it does not
> recognize, in every section: `[Error] Unrecognized key(s) in <section>: '...'`. A typo or a stale
> key is a hard error, not a silent no-op. Validate with `jaffar --dryRun`, which runs this check.

- [Driver Configuration](#driver-configuration)
- [Engine Configuration](#engine-configuration)
- [Emulator Configuration](#emulator-configuration)
- [Game Configuration](#game-configuration)
- [Runner Configuration](#runner-configuration)

---

## Driver Configuration

The driver owns the top-level run loop: when to stop and what to checkpoint to disk.
*(Source: `source/driver.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Stop Frames After First Win` | number | no | Stop condition on wins: `0` stops the run as soon as the first win state is found; `N > 0` keeps the search expanding `N` more steps past the first win before stopping (useful with `Win State Collection` to harvest alternate endings); absent = never stop on wins (run to `Max Steps`). |
| `End On First Win State` | boolean | no | **Legacy alias** of `Stop Frames After First Win` (`true` → `0`, `false` → absent), kept so archived configs retain their exact semantics. New configs should use `Stop Frames After First Win`. |
| `Max Steps` | number (uint32) | yes | Maximum search depth (steps) to execute. Use a generous bound; `0` means no step limit. Can be overridden for testing via the `JAFFAR_DRIVER_OVERRIDE_DRIVER_MAX_STEP` environment variable. |
| `Save Intermediate Results` | object | yes | Periodic checkpointing of the best/worst solutions found so far (see below). |
| `Win State Collection` | object | no | Save every win state's solution to disk, deduplicated by a configurable tuple of game properties (see below). |

### Driver Configuration → Save Intermediate Results

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Whether to periodically write the best/worst solutions to disk during the search. |
| `Frequency (s)` | number (float) | yes | How often (in seconds) to write the intermediate files. Only used when `Enabled` is `true`. |
| `Best Solution Path` | string | yes | File path for the best (highest-reward) solution found so far. |
| `Worst Solution Path` | string | yes | File path for the worst solution found so far. |

> Older example configs used to carry `Best State Path` / `Worst State Path` here. The engine never
> read them, and they are now **rejected** as unrecognized keys — they have been removed from the
> shipped examples.

### Driver Configuration → Reference Reward Floor

Optional. Compares the search against a reference solution's per-step reward trace. The trace
comes from `Solution File` (a .sol replayed internally through the live reward function — the
recommended form) or `Path` (a precomputed one-reward-per-line file, legacy).

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Enables the run-level **cancel** check: the run stops (`bestBelowReference`) when the best state falls below the reference at the current step beyond `Tolerance`. |
| `Tolerance` | number | yes | Allowed slack for the cancel check (set ≥ the reference's largest one-step reward transient). |
| `Step Grace` | number | no | Compare against the reference G steps earlier (bounded time slack for discrete-jump rewards). Default 0. |
| `Solution File` | string | one of | Reference .sol; its per-step trace is computed at init by replaying it through this run's own runner/game. |
| `Path` | string | one of | Precomputed reward trace file (legacy). |
| `Cancel If Reference Below Worst` | boolean | no | Also cancel when the reference falls below the worst kept state (evicted from the frontier). Default false. |
| `Below Worst Margin` | number | no | Margin for the below-worst check (typically the Reference Pinning bonus). Default 0. |

The reference trace defined here is also the input to the engine-side `Reference Reward Prune`
section (see Engine Configuration) — a floor with `Enabled: false` plus a `Solution File` is a
valid way to supply the trace for pruning without the cancel check.

### Driver Configuration → Win State Collection

Saves the input history of **every** win state whose dedup key is new, as
`<Path Prefix><hexkey>.sol`, plus a `<Path Prefix>manifest.txt` line (`key <hexkey> step <N>`)
per capture. The dedup key is the concatenated raw bytes of the listed game properties at the
win state — e.g. the four RNG bytes, so each distinct frozen RNG (each distinct next-stage
board, in games where the next layout is drawn from the RNG) is captured exactly once. Progress
appears in the per-step log as `Win States Collected: N/max`; reaching `Max Files` terminates
the run with exit reason `Win collection complete`. Typically paired with
`"Stop Frames After First Win": N` to bound the harvest window.

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Whether to collect win-state solutions. |
| `Dedup Properties` | array of strings | yes | Game property names whose concatenated bytes form the dedup key (e.g. `["RNG State 1", "RNG State 2"]`). |
| `Path Prefix` | string | yes | Path + basename stem for the saved `.sol` files and `manifest.txt`. |
| `Max Files` | number | no | Cap on distinct solutions to save; hitting it ends the run (default 1000). |

---

## Engine Configuration

Sizing of the two in-memory databases that drive the search: the **state database** (the frontier
of states to expand) and the **hash database** (visited-state deduplication). The mechanics are
explained in [Search Concepts & Tuning](04-search-concepts.md); the keys are:
*(Source: `source/engine.hpp`, `source/stateDb.hpp`, `source/hashDb.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Base State Batch Size` | number | no | How many base states each worker pulls from the per-NUMA queue per lock acquisition (clamped to 1–16). **When omitted it is auto-tuned**: at startup the engine times a burst of state advances and picks a batch worth roughly 200 µs of work, so cheap cores get a large batch (amortizing the queue lock) and heavy cores get a small one (better load balance). Measured: SDLPoP/TestEmulator → 16, Genesis/GBA/A2600 → 1. Set this explicitly to override the auto-tuned value. Pure performance knob — it does not change which states are explored or the result. |

### Engine Configuration → State Database

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Max Size (Mb)` | number | yes | Memory budget (in MB) for the state database, across all NUMA domains. When full, the search can no longer hold new states. Override for testing via `JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB`. |

### Engine Configuration → Hash Database

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Whether to deduplicate already-visited states by hash. Almost always `true`; disabling it makes the search re-explore states. |
| `Max Store Count` | number | yes* | Number of rolling hash-store generations to keep. Read only when `Enabled` is `true`. |
| `Max Store Size (Mb)` | number | yes* | Memory budget (in MB) for the hash database. Read only when `Enabled` is `true`. Override via `JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB`. |

\* Required when `Hash Database` > `Enabled` is `true`.

### Engine Configuration → Reference Pinning

Optional. Anchors the exact reference lineage in the search: a produced state whose hash matches
the reference's state at its depth (within `Lookahead` steps ahead) and byte-verifies against the
exact captured reference states (available when the driver's floor uses `Solution File`) is
"pinned" — once per depth, it bypasses hash deduplication and receives `Bonus` extra ordering
reward, so the reference lineage provably survives. The log reports
`Reference Pin Hits (cumulative): N (deepest D / L)`; D tracking the step count 1:1 proves the
reference line is being carried. **When combined with `Reference Reward Prune`, set `Bonus` to
roughly the prune `Tolerance`**: the trace-riding reference is the floor of the admitted band,
and a small bonus lets DB-full eviction (which drops the lowest-ranked states) kill it first.

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Whether to pin the reference lineage. |
| `Path` | string | yes | Per-depth hash file, produced by `jaffar-player <config> <ref.sol> --dumpHashes <path>`. |
| `Bonus` | number | yes | Ordering-reward bonus for the pinned state at each depth (≈ prune Tolerance when pruning). |
| `Lookahead` | number | yes | Also pin states that reach the reference state up to this many steps early. |
| `Lookahead Bonus` | number | yes | Extra bonus per step of earliness. |

### Engine Configuration → Reference Reward Prune

Optional **greedy pruning mode**: every produced state whose reward falls below the reference
trace at its depth (beyond `Tolerance`) is dropped in the engine workers — only lineages
decisively keeping pace with the reference survive, so the frontier stays small and exploration
goes deep. A polish mode for refining a known solution, not for exploration. Win states and the
pinned reference lineage are exempt. The per-depth trace itself is supplied by the driver's
`Reference Reward Floor` section (`Solution File` or `Path`; the floor's own `Enabled` may be
`false` to prune without the cancel check). Dropped states are reported as
`Dropped States (Below Reference)`.

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Enabled` | boolean | yes | Whether to prune produced states that fall below the reference trace. |
| `Tolerance` | number | yes | Allowed slack below the trace before a produced state is dropped. |

---

## Emulator Configuration

This section selects the emulation core and tells it where to find the game data and the initial
state. Exactly one key is common to *all* cores; the rest are **core-specific**.
*(Source: `source/emulator.hpp` for the common key; each emulator's header under `emulators/` for
the rest.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Emulator Name` | string | yes | Which core to instantiate. Must match a core compiled into this build (see `-Demulator=`). The error message on a mismatch lists the cores available in the current build. |

The remaining keys depend on `Emulator Name`. Below are the in-repo `TestEmulator` (used throughout
this manual) and the common pattern shared by the ROM-based cores.

### TestEmulator (`"Emulator Name": "TestEmulator"`)

A ROM-free emulator: a cursor on a rectangular grid. Used by the test suite and the examples here.
*(Source: `emulators/testEmulator/testEmulator.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Grid Width` | number | yes | Width of the grid. |
| `Grid Height` | number | yes | Height of the grid. |
| `Start X` | number | yes | Initial cursor column (must be `< Grid Width`). |
| `Start Y` | number | yes | Initial cursor row (must be `< Grid Height`). |

Its inputs are single pipe-delimited characters: `|U|`, `|D|`, `|L|`, `|R|`, and `|.|` (no-op).

### ROM-based cores (common pattern)

The console cores (QuickerNES, NesHawk, QuickerSnes9x, QuickerGPGX, QuickerStella, QuickerGambatte, …) share
a similar set of keys. The exact set varies per core — consult the core's header under `emulators/`
— but the recurring keys are:

| Key | Type | Description |
|-----|------|-------------|
| `Rom File Path` | string | Path to the game ROM/image. |
| `Rom File SHA1` | string | Expected SHA1 of the ROM, validated on load. |
| `Initial State File Path` | string | Emulator save-state to start the search from. |
| `Initial Sequence File Path` | string | An input sequence to replay before the search begins (sets up the starting position). Often optional/empty. |
| `Initial RAM Data File Path` | string | Raw RAM image to load at start (core-dependent size). |
| `Disabled State Properties` | array of strings | Names of save-state memory blocks to *exclude* from the searched/serialized state (shrinks state size — see [tuning](04-search-concepts.md#making-states-smaller)). |

### QuickerSDLPoP (Prince of Persia)

The Prince of Persia core has its own keys for the game files and for controlling
randomness/determinism. *(Source: `emulators/quickerSDLPoP/quickerSDLPoP.hpp` and
`extern/quickerSDLPoP/source/sdlpopInstanceBase.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `SDLPoP Root Path` | string | yes | Directory containing the SDLPoP game files. |
| `Levels File Path` | string | yes | Path to the levels data file (`LEVELS.DAT`-style); may point to a custom level. |
| `Game Version` | string | yes | `"1.0"` or `"1.4"`. |
| `Initial State File` | string | yes | Save-state to start from. |
| `Initial Sequence File Path` | string | no (default empty) | Input sequence to replay before search. |
| `Override RNG Enabled` | boolean | yes | Force a fixed RNG seed. |
| `Override RNG Value` | number | yes | The RNG seed used when the override is enabled. |
| `Override Loose Tile Sound Enabled` | boolean | yes | Force the loose-tile sound state. |
| `Override Loose Tile Sound Value` | number | yes | Value used when the above is enabled. |
| `Initialize Copy Protection` | boolean | yes | Whether to run the copy-protection setup. |
| `Disable Non-Gameplay RNG` | boolean | no (default false) | Stop cosmetic animations (torch flicker, etc.) from consuming RNG. Recommended for exploration: it keeps the searched state stable so combat/RNG hashing does not explode. |

---

## Game Configuration

The game layer interprets emulator memory as named **properties**, decides what counts as a win or
fail through **rules**, and assigns **rewards** that steer the search. This section's structural
keys are below; the rule/condition/reward grammar is detailed in
[Rules, Conditions & Rewards](03-rules-and-rewards.md). *(Source: `source/game.hpp`,
`source/rule.hpp`, `source/condition.hpp`, `source/property.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Game Name` | string | yes | Which game class to instantiate (must be compiled into this build). |
| `Frame Rate` | number (float) | yes | The game's playback frame rate (used by tooling/playback). |
| `Bypass Emulator State` | boolean | yes | If `true`, the game manages its own state instead of using the emulator's save-state. Normally `false`. |
| `Print Properties` | array of strings | yes | Property names to display in status output. Each must be a registered property of the game. |
| `Hash Properties` | array of strings | yes | Property names that define a state's identity for deduplication. Two states with identical hash-property values are treated as the same state. Choosing these well is the single most important tuning lever — see [tuning](04-search-concepts.md#choosing-hash-properties). |
| `Rules` | array of objects | yes | The win/fail/checkpoint/reward logic. See below and [chapter 3](03-rules-and-rewards.md). |

### Game Configuration → Rules → (rule object)

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Label` | number | yes | Unique numeric id for the rule, referenced by other rules' `Satisfies`. |
| `Conditions` | array of objects | yes (may be empty) | All conditions must hold (logical AND) for the rule to fire. |
| `Actions` | array of objects | yes (may be empty) | What happens when the rule fires. |
| `Satisfies` | array of numbers | yes (may be empty) | Labels of other rules to also mark satisfied when this rule fires (rule chaining). |

### Game Configuration → … → Conditions → (condition object)

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Property` | string | yes | Name of a registered property (left operand). |
| `Op` | string | yes | Comparison operator (see list below). |
| `Value` | number, string, or boolean | yes | Right operand. A **number**/**boolean** is an immediate value; a **string** names another property to compare against. |

Valid `Op` values (case-sensitive): `==`, `!=`, `>`, `>=`, `<`, `<=`, `%0` (property modulo Value
equals zero), `%N` (modulo is non-zero), `BitTrue` (the bit at position Value is set), `BitFalse`
(that bit is clear). *(Source: `source/condition.hpp`.)*

### Game Configuration → … → Actions → (action object)

Every action has a `Type`; some types take extra keys. The **core** action types (available to all
games) are: *(Source: `source/game.hpp`.)*

| `Type` | Extra keys | Effect |
|--------|-----------|--------|
| `Add Reward` | `Value` (number) | Adds `Value` to the state's reward. |
| `Trigger Win` | — | Marks the state as a win (a solution). |
| `Trigger Fail` | — | Marks the state as failed (pruned). |
| `Trigger Checkpoint` | `Tolerance` (number) | Marks a checkpoint with the given tolerance (see [chapter 3](03-rules-and-rewards.md)). |
| `Trigger Save Solution` | `Path` (string) | Writes the current solution to `Path` when the rule fires. |

Games may register **additional** action types — most notably *reward magnets*, which continuously
pull the search toward a target value of some property (e.g. `Set Kid Pos X Magnet`). These are
game-specific and documented per game; the pattern is described in
[Rules, Conditions & Rewards](03-rules-and-rewards.md#reward-shaping-and-magnets).

### Property datatypes

Properties are registered by each game's C++ code (not in the config), but conditions and magnets
refer to them by name. A property's datatype is one of: `UINT8`, `UINT16`, `UINT32`, `UINT64`,
`INT8`, `INT16`, `INT32`, `INT64`, `BOOL`, `FLOAT32`, `FLOAT64`, with `Little` or `Big` endianness.
*(Source: `source/property.hpp`.)* To discover a game's property names, see
[Adding a Game](05-adding-a-game.md) or read the game's header under `games/`.

---

## Runner Configuration

The runner sits between the engine and the emulator: it defines which inputs are legal in a given
state, how state hashing behaves, and frame-skipping. *(Source: `source/runner.hpp`,
`source/inputSet.hpp`.)*

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Allowed Input Sets` | array of objects | yes | The legal inputs, possibly gated by conditions (see below). This is what defines the branching factor of the search. |
| `Test Candidate Inputs` | boolean | yes | Whether to also evaluate the `Candidate Input Sets` (used to probe additional inputs). |
| `Candidate Input Sets` | array of objects | yes (may be empty) | Same structure as `Allowed Input Sets`; only used when `Test Candidate Inputs` is `true`. |
| `Hash Step Tolerance` | number | yes | How many consecutive steps a state may persist before re-hashing for deduplication. `0` hashes every step (most precise, most states); higher values collapse near-identical frames (fewer states). See [tuning](04-search-concepts.md#hash-step-tolerance). |
| `Bypass Hash Calculation` | boolean | yes | If `true`, take the state hash from the game directly instead of computing it. Advanced; normally `false`. |
| `Store Input History` | object | yes | Whether/how many inputs to record per state (needed to reconstruct solutions). |
| `Frameskip` | object | yes | Frame-skipping behavior. |
| `Show Allowed Inputs` | boolean | yes | Debug: print the set of allowed inputs each frame. |
| `Show Empty Input Slots` | boolean | yes | Debug: print placeholder lines for unavailable inputs. |

### Runner Configuration → Store Input History

Selects *how* each state remembers the path of inputs that produced it. The search needs this to write
the winning `.sol` (and the intermediate best/worst solutions). One strategy is chosen with `Type`:

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Type` | string | yes | One of `None`, `Raw`, or `Trie`. No default — it must be set explicitly. |
| `Max Size` | number | for `Raw` and `Trie` | Maximum number of inputs recorded. For `Raw` it sizes the per-state buffer (longer solutions are truncated). For `Trie` it bounds only the reconstructed solution/snapshot buffer; the live search path is unbounded. Omit for `None`. |

```jsonc
"Store Input History": { "Type": "Trie", "Max Size": 5000 }   // recommended default
"Store Input History": { "Type": "Raw",  "Max Size": 5000 }   // classic, self-contained
"Store Input History": { "Type": "None" }                      // no solutions, smallest state
```

#### How the strategies store the path

Because the search branches, the many states it holds at once share large common prefixes. Suppose it is
holding three states reached by these input paths (`R`=Right, `L`=Left, `A`/`B`=other inputs):

```
  start ─R─R─┬─A─┬─L   → state P   (path: R R A L)
             │   └─R   → state Q   (path: R R A R)
             └─B       → state W   (path: R R B)
```

**`Raw`** — every state carries its *entire* path, in a fixed `Max Size`-wide bit-packed buffer:

```
  P: [R][R][A][L]·············   (· = unused, padded to Max Size)
  Q: [R][R][A][R]·············
  W: [R][R][B]················
        └──┴──┘  the shared "R R (A)" prefix is duplicated in every state
```

**`Trie`** — the paths live *once* in a shared tree of moves; each state stores only a 4-byte id of its
leaf node (plus a 4-byte step count):

```
  shared trie:                 states (8 bytes each):
    root─R─R─┬─A─┬─L  «nL»        P → nL
             │   └─R  «nR»        Q → nR
             └─B      «nB»        W → nB
        the "R R A" prefix is stored ONCE and shared by P and Q
```

**`None`** — only the step count is kept; the inputs are discarded:

```
  P: [count=4]   Q: [count=4]   W: [count=3]      (no inputs → no .sol)
```

#### Trade-offs

|                         | `None`            | `Raw`                                   | `Trie`                                       |
|-------------------------|-------------------|-----------------------------------------|----------------------------------------------|
| Per-state footprint     | 4 B (count)       | fixed `Max Size × bits/input` (e.g. ~4 KB at 8000 steps) | 8 B (node id + count)            |
| Total path memory       | ~none             | grows as *states × depth* (prefixes duplicated) | *shared prefixes* → size of the path tree (far smaller) |
| Reconstruct a `.sol`?   | no                | yes                                     | yes                                          |
| Shared structure        | none              | none (each state self-contained)        | one reference-counted trie (sharded → contention-free) |
| Path length limit       | counter only      | capped at `Max Size` (longer truncates) | unbounded live; `Max Size` only caps the reconstructed `.sol` |
| Search throughput       | baseline          | baseline                                | ~same as `Raw` at full thread count          |

- **`None`** — *Pros:* smallest possible state, zero path bookkeeping. *Cons:* no solution can be written.
  *Use when* you only care whether a win is reachable (benchmarks, reachability checks), not the inputs.
- **`Raw`** — *Pros:* dead simple and fully self-contained — no shared structure, no cross-thread
  coordination, perfectly predictable memory. *Cons:* every state pays for a full `Max Size`-wide buffer
  regardless of its actual depth, and sibling states duplicate their shared prefixes, so total memory
  balloons on deep/wide searches; solutions longer than `Max Size` are truncated.
  *Use when* searches are short/shallow, or you want the simplest behavior and memory is not the bottleneck.
- **`Trie`** — *Pros:* tiny per-state footprint (8 bytes, independent of depth); total path memory scales
  with the *shared* path tree instead of states × depth, so it is far smaller exactly on the large
  searches where it matters; the live path is unbounded. *Cons:* it is a shared, reference-counted
  structure (more machinery internally); writing a solution walks the leaf node back to the root — an
  `O(depth)` step, but only at snapshot/solution time, not during the search. **Its total memory is *not*
  bounded by the State DB and has a hard ceiling that very deep searches can hit — see the caution below.**
  *Use when* searches are deep or large (the common case) — this is the recommended default. The smaller
  per-state size also lets more states fit in the database (see *State Size* in
  [Understanding the Output](07-understanding-output.md)).

> [!IMPORTANT]
> **The `Trie` pool grows *unbounded* and has a hard ceiling — keep an eye on it.** Unlike `Raw` (whose
> history lives *inside* each state slot and is therefore bounded by `State Database → Max Size (Mb)`), the
> `Trie` is a **separate shared structure** that grows roughly as **live-states × path-depth** (minus shared
> prefixes) and is **not counted in the State DB budget**. Two things to watch:
>
> - **It has a hard node-storage ceiling.** The node pool is capped at a fixed maximum. The engine's startup
>   RAM guard reserves that ceiling, and the search now **stops *gracefully*** when the pool nears it — exit
>   reason *"Input-history trie neared its hard memory ceiling"*, with the best result saved — instead of a
>   worker hitting the wall mid-step and terminating the whole run. If you hit this, switch to `Raw` or
>   **reduce the State DB size** (fewer live states ⇒ the trie grows more slowly and reaches deeper first).
> - **Prefix-sharing fades with depth, so `Trie` can lose to `Raw` on very deep searches.** `Raw`'s per-state
>   cost is *fixed* (`Max Size`-wide); the trie's grows ~linearly with depth as deep frontier states stop
>   sharing tails. Past a crossover depth the trie can use **more** total memory than `Raw` would — while
>   `Raw` stays bounded by the State DB (no separate pool, no ceiling).
>
> **Watch the `Input History (shared): … Mb … = … Mb total (raw would be … Mb)` line** in the per-step output
> ([Understanding the Output](07-understanding-output.md)): if the trie's total climbs toward its ceiling, or
> rises above the *“raw would be”* figure, prefer `Raw` and size the State DB to the RAM you have. `Trie`
> stays the better default for short-to-moderate-depth searches with healthy prefix sharing.

### Runner Configuration → Frameskip

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Rate` | number | yes | Frames to advance per input step in addition to the input frame (`0` = no skipping). Larger values search coarser but faster. |
| `Use Custom Input` | boolean | yes | If `true`, play `Custom Input` on the skipped frames; if `false`, repeat the last input. |
| `Custom Input` | string | yes | The input to apply on skipped frames when `Use Custom Input` is `true`. Validated at startup. |

> Note: the frame-skip settings live **inside** the `Frameskip` object. A top-level
> `Frameskip Rate` key (as seen in some examples) is *not* read.

### Runner Configuration → Allowed Input Sets / Candidate Input Sets → (input-set object)

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `Conditions` | array of objects | yes (may be empty) | Conditions (same grammar as rule conditions) that must all hold for these inputs to be offered. An empty array means "always available". |
| `Inputs` | array of strings | yes | The inputs offered when the conditions hold. Each string is an emulator-specific input encoding (e.g. `\|U\|` for the TestEmulator, `U.L.F` or `\|..\|........\|` for console cores). |
| `Stop Input Evaluation` | boolean | yes | If `true`, no later input set is considered once this one matches (short-circuit). |

Input sets let you restrict the search to sensible moves in each situation — the foundation of
making a search tractable. See [Rules, Conditions & Rewards](03-rules-and-rewards.md) for how
conditions are written and [Search Concepts & Tuning](04-search-concepts.md) for why a tight input
set matters.

---

## A complete annotated example

[`docs/examples/gridwalker.jaffar`](examples/gridwalker.jaffar) is a minimal but complete configuration that
exercises every section above with no ROM. It is worth reading top to bottom alongside this
reference: it defines a 5×5 grid (`TestEmulator`), offers the four movement inputs unconditionally
(`Allowed Input Sets`), hashes the cursor position (`Hash Properties`), fails at the center `(2,2)`,
rewards reaching column `4`, and wins at `(4,4)` — chaining the column-4 reward via `Satisfies`. The
next chapter walks through how those rules actually steer the search.
