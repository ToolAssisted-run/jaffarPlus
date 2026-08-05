# 7. Understanding the Output

When `jaffar` runs, it prints a full status report **every step** (and a final exit line when it
stops). The report looks intimidating, but it is just four things stacked together: what the best
state found so far looks like, how much progress the search has made, how full memory is, and how
fast it is going. This chapter walks through every field so you can read a run at a glance and
diagnose one that is slow, runs out of memory, or "runs out of states".

- [The exit line](#the-exit-line)
- [The per-step report, top to bottom](#the-per-step-report-top-to-bottom)
- [Best-state report](#best-state-report)
- [Search progress](#search-progress)
- [Memory: state database](#memory-state-database)
- [Memory: hash database (deduplication)](#memory-hash-database-deduplication)
- [Performance and engine internals](#performance-and-engine-internals)
- [Reading a run at a glance](#reading-a-run-at-a-glance)

Every line is prefixed with `[J+]`. Examples below omit it for readability.

## The exit line

The last line the engine prints is the one you usually care about most:

```
Step 154 - Exit Reason: Solution found.
```

`Exit Reason` is one of these values, and it is also the process exit code:

| Exit reason | Code | Meaning |
|---|---|---|
| `Solution found.` | 0 | A state satisfied a `Trigger Win` rule (and the `Stop Frames After First Win` window elapsed). The winning solution was written to the configured output path. |
| `Engine ran out of states.` | 1 | The reachable, non-duplicate state space was **exhausted** without a win. The search is complete — no solution exists under the current rules/inputs/hashing. |
| `Maximum step count reached.` | 2 | The search hit `Max Steps` before finding a win or exhausting the space. It was cut off, **not** completed — raise `Max Steps` (or improve guidance) to go further. |
| `Win collection complete (Max Files reached).` | 7 | `Win State Collection` was enabled and collected its `Max Files` cap of distinct win solutions. |

> The shell `$?` after a backgrounded pipeline reflects the *last* command, not `jaffar`. Read the
> `Exit Reason` line itself, not the exit code of a wrapping command.

## The per-step report, top to bottom

Each step the engine prints a block in this order:

1. **Best-state report** — what the single highest-reward state found so far looks like (runner, game, emulator).
2. A separator with **job/script identity** and the headline counters.
3. **Engine Information** — states processed, classified, and (optionally) timed.
4. **State Database Information** — how many states are stored and how full memory is.
5. **Database Popping / Free-Slot Cache / Get Free State Rates** — NUMA bookkeeping for the state pool.
6. **Hash Database Information** — the two-tier deduplication statistics.
7. **Manually Saved Solution / Candidate Inputs** — auxiliary status.

## Best-state report

This describes the *current best* state (highest cumulative reward), which is also what gets written
as the solution if it wins.

**Runner Information (Best State)** — the input side:

| Field | Meaning |
|---|---|
| `Input History Type` | How the path of inputs to each state is stored — `None`, `Raw`, or `Trie` (`Store Input History` in the config), with its per-state cold/full sizes. |
| `Possible Input Count` | Size of the game's full input alphabet (encoded in N bits). |
| `Current Input Count` | How many steps deep this best state is. |
| `Hash` | The state's content hash (used for deduplication). |
| `Hash Step Tolerance Stage` | Where this state sits in the `Hash Step Tolerance` window (see [Search Concepts](04-search-concepts.md)). |
| `Allowed Inputs` | The inputs the game permits from this exact state (after the `Allowed Input Sets` filter). A short list here means the state is constrained; only `\|.\|.....\|` (no-op) means the search cannot branch and will stall. |

**Game Information (Best State)** — the game side:

| Field | Meaning |
|---|---|
| `Game State Type` | `Normal`, `Win`, or `Fail` — how the rules classified this state. |
| `Game State Reward` | The cumulative reward of this state. This is what the search maximizes; watch it climb. |
| `Rule Status` | A bit-per-rule string showing which rules have fired for this state (left-to-right = rule order). |
| `Game Properties` | The values of every property listed in `Print Properties`. This is your custom window into the state. |
| Game-specific dump | Everything below (e.g. for Prince of Persia: `Global Step Counter`, `Current/Next Level`, `Timer`, the `[Player]`/`[Guard]` character lines, `RNG`, `Moving Objects`, `Transformable Objects`, and the active magnets and their centers). The exact fields are defined by the game integration. |

**Emulator Information (Best State)** — emulator-level state info (usually minimal; defined per core).

## Search progress

After the job/script header (`Job Id`, `Script File`, `Emulator Name`, `Game Name`, `Current Step #`
of `Max`, and `Current Reward (Best / Worst)` of the frontier), **Engine Information** reports the work
done this step and in total:

| Field | Meaning |
|---|---|
| `Base States Processed` | States *popped* from the database this step to be expanded (in millions). |
| `New States Processed` | Successor states *produced* by expanding them (in millions). |
| `Base/New States Performance` | Throughput in Mstates/s — your headline speed number. |
| `Normal States` | New states kept as ordinary frontier states. |
| `Win States` | New states that satisfied a win rule. **`> 0` means a solution exists**, even if the run is configured to continue past wins (`Stop Frames After First Win` absent or > 0). |
| `Failed States` | New states killed by a `Trigger Fail` rule (pruned). |
| `Repeated States` | New states discarded as **duplicates** (already-seen hash). A high percentage (e.g. 90%+) means the search is re-deriving the same positions — normal for combat/cycles, but if it dominates and the count is small, the reachable space is tiny. |
| `Dropped States (No Storage Available)` | States discarded because the **state database is full** (see below). Non-zero means you are memory-bound — raise the budget or shrink the state. |
| `Dropped States (Failed Serialization)` / `(Checkpoint)` | States dropped because they could not be serialized, or pruned by checkpoint tolerance. |

`Checkpoint (Level / Tolerance / Cutoff)` shows the current checkpoint depth reached, its tolerance,
and the step cutoff — relevant when you use `Trigger Checkpoint` (see [Rules & Rewards](03-rules-and-rewards.md)).

## Memory: state database

**State Database Information** is where you watch for the search exhausting RAM:

| Field | Meaning |
|---|---|
| `Current State Count … / … Max / …% Full` | Stored states vs. the configured capacity, and the fill percentage. Approaching `100% Full` is when `Dropped States` start appearing. |
| `Current State Size … / … Max` | Bytes used vs. the byte budget. |
| `State Size Raw` / `State Size in DB` | Size of one serialized state, and its padded in-database footprint. Smaller states = more of them fit (use `Disabled State Properties` to trim unused memory blocks). |
| `NUMA Domain N States` | Per-domain state counts under the NUMA-aware two-tier database. |

## Memory: hash database (deduplication)

**Hash Database Information** reports the two-tier deduplicator that prevents re-exploring states (a
NUMA-local **L1** in front of a shared global **L2**):

| Field | Meaning |
|---|---|
| `L1 (local): Checks / Local Hits / Entries / Size` | Lookups served by the per-domain cache, how many hit (were duplicates), entries stored, and size vs. its budget. |
| `L2 (global): Checks (L1 misses) / Cross-Domain Hits / Entries / Size` | Lookups that fell through to the shared table, cross-domain duplicate hits, and its size. |
| `Served locally` / `Total dup rate` | Fraction handled without touching L2, and the overall duplicate rate. |
| `L2 Store … Collision Count (Rate …)` | Global table generation, entries, and hash-collision rate. |

A rising `Size … / … Mb` toward the budget here (rather than the state DB) means hashing is the
memory bottleneck — tune the L1/L2 sizes in the config.

## Performance and engine internals

- `Thread Count / NUMA Domains` — the parallelism the engine is using.
- `Elapsed Time (Step/Total)` — wall-clock for this step and cumulatively. By default only the coarse
  step time plus the serially-measured `Advance Hash Db` / `Advance State Db` stages are shown; build
  with `-DdetailedProfiling=true` for the full per-operation breakdown (state advance/load/save, hash
  calculation/checking, rule checking, reward, input generation, etc.).
- `Database Popping State Rates` / `Free-Slot Cache` / `Get Free State Rates` — NUMA locality
  bookkeeping for the state pool (hit/steal/locality rates, average NUMA distance). These matter when
  diagnosing scaling on a multi-socket host; on a single node they can be ignored.
- `Manually Saved Solution` — the path/reward of any solution written out so far.

## Reading a run at a glance

For day-to-day use, four fields tell you almost everything:

1. **`Exit Reason`** — did it win, exhaust the space, or get cut off?
2. **`Win States`** — `> 0` confirms a solution was found.
3. **`Game State Reward`** (best state) — is the search making progress toward your goal?
4. **`…% Full` / `Dropped States`** — are you memory-bound?

If the reward is stuck and `Repeated States` is ~100% with a small state count, the search has
exhausted what it can reach — revisit guidance (magnets, rewards), `Allowed Input Sets`, or the
hashing. See [Search Concepts & Tuning](04-search-concepts.md).
