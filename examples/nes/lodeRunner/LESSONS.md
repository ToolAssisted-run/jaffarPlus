# NES Lode Runner — Lessons Learned

Campaign closed 2026-08-11. Final stage50 result: `stage50.new.sol` (518 inputs from the
stage50 anchor, full-movie frame 48777) vs the original choreography's 526 — an 8-frame
improvement, both-core verified (QuickerNES + NesHawk). Machine-found best was
`stage50.win519.sol` (519); the last frame came from manual polish. Full working notes in
`NOTES.md`; this file is the distilled, transferable lessons.

## The clock-parity law (the big one)

Player run speed AND enemy stepping are phase-keyed to the global cyclic clock
(`$53`, mod 28, advancing +2/+3 alternately — full phase period 56 frames). Consequences:

- A partial solution that ends N frames ahead of a reference (N ≢ 0 mod 56) can NEVER
  replicate the reference's subsequent micro-choreography, no matter how exactly its
  player state matches at the boundary. Proven directly: with `$1D/$23/$24` (and tile,
  offsets) byte-identical at a section boundary, trajectories still diverge on the first
  advance, because the physics reads the clock.
- Therefore "hand-off phase gates" (win conditions demanding the reference's boundary
  micro-state) are structurally unsatisfiable for any lead not ≡ 0 (mod 56). We proved
  this the expensive way: a full-window search tied the reference's reward floor for 147
  straight depths and never matched the phase bytes.
- Per-section cost of re-executing a fixed route is a function of clock parity: at lead 4,
  section 3 (two digs + enemy-carried gold steal) cost +7 frames over the reference —
  eating the whole lead. If sectioning is retried: MEASURE section cost per lead
  (lead-tuning probes) before banking any section win.

## Sectioned-search methodology (what worked)

- Route odometer: waypoint file of "x y goldGate" tiles (consecutive-duplicate-collapsed
  from the reference RAM trace), reward = latched progress × fixed reward + a high-water
  sub-tile proximity term. Naturally monotone along the reference — verify with
  `JAFFAR_DUMP_REF_TRACE` (zero decreasing steps) before EVERY launch; it caught real
  bugs (ramp dips) repeatedly.
- Pre-advance waypoint consumption is mandatory: consume waypoints matching the
  pre-advance tile before the post-advance pass, or the section's start tile falsely
  brands the follower off-route.
- Reference floor across different lineages: the driver's floor now takes its own
  `Initial State File Path` + `Initial Sequence File Path` (replayed at the EMULATOR
  level, so game-lineage variables stay at root defaults). Floor depth k = the
  reference's reward k steps after ITS boundary vs ours k steps after OURS.
- Grace = banked lead (+ user-approved slack), and cap Max Steps so only globally-ahead
  wins can fire. Pad the floor solution with trailing nulls to (cap) length, or the
  "reached the reference's frame count" cancel fires inside the banked-lead window.
- Reward events that lead their latch: ramps paying during a dig must be HIGH-WATER
  latches (serialized + hashed). The dig animation ends 1–2 frames before the registry
  hole appears; a live-valued ramp dips right before the completion latch and breaks
  floor monotonicity.
- Ordered multi-dig shaping: the Dig Chain (cells dug in sequence, gold-gated, ramp +
  latch per cell) worked exactly as designed — the reference floor showed both payments
  on the reference's own dig completions.

## Hashing

- Never hash a global timer ($9E spawn counter) — but $53 IS legitimate hash state here:
  it is the movement-phase driver (genuine luck/physics state, not a mere counter).
- Full enemy state matters: tiles, timers (sign bit + value), pixel offsets, facing,
  PLUS `$C5` (kill count / buried-guard respawn lineage) — added when section 3 trapped
  a carrier; lines that kill vs merely trap an enemy must not dedup together.
- Dig state: `$A0` progress, dig direction (`$702` bit 6), all 8 hole-registry slots
  including refill counters (`$6A0/$6C0/$6E0`), and both tile-map layers
  (`$0200`/`$0400`). All were needed at one point or another.
- Engine Hash Lookahead (1) resolves RAM-identical phase twins by future divergence —
  the fix for over-dedup after the cycle-phase digest was retired (serialized-state
  hashing carries instance-volatile bytes and the global frame counter).

## Resync & verification

- Movies do not transplant across leads: replaying reference inputs from a shifted seed
  desyncs (rope-grab y-phase → x-speed alternation flips → ladder mounts whiff by
  pixels). Null inserts fix temporal shifts only; positional lags need held-input run
  extend/shrink edits (`sec2resync.py`: greedy hill-climb scored by route-odometer depth
  with a timeliness tiebreak, edit window reaching 45 frames upstream of the stall).
- The accounting authority is always a full replay (`--dumpRam` + offline scoring);
  line counts and dump frame counts are off-by-one traps.
- Fast NesHawk rendering: keep a NesHawk savestate anchor at the segment start; render
  seconds instead of 6-minute full-movie replays. Screenshot dir must exist; screenshot
  steps must not exceed movie length.

## Infrastructure

- `setsid` does NOT survive SSH session-scope teardown: systemd SIGKILLs the whole scope
  cgroup silently (no kernel log, wrapper shells die too — looks exactly like a random
  mid-run death). Launch long runs as transient units:
  `systemd-run --user --collect --unit=... bash -c '<cmd> > log 2>&1; echo EXITCODE=$? >> log'`
  and verify the cgroup shows `app.slice`, not `session-*.scope`.
- The "reached the reference's frame count" driver cancel is not banked-lead aware —
  account for it with floor padding (see above).
- Rebuilding binaries while a replay is running segfaults the running player
  ("(deleted)" in the kernel log) — harmless-looking but a red herring generator.
