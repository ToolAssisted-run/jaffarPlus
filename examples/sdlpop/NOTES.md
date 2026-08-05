# Prince of Persia (SDLPoP) — search notes

Notes for running JaffarPlus on the SDLPoP port. The level folders (`0100`, `0200`, …)
are named `LLSS` = level·segment. These are practical hints — verify against current code.

## Approach: read the game, don't only brute-force
SDLPoP is open source, so the highest-leverage move is to **read the game logic for
oversights and shortcuts** rather than searching blindly. Examples that paid off:
- A pickup object-type read that leaks state.
- Shadow-figure collision being disabled on certain levels.
Finding the mechanic first, then searching to exploit it, is far cheaper than hoping a
blind search stumbles onto it.

## Configuration
- **Disable Non-Gameplay RNG.** The torch-flicker animation churns the RNG every frame;
  with combat present that explodes the combat-RNG hash space. Set the option that
  disables non-gameplay RNG, and pair it with the combat-RNG hash guard on segments that
  actually contain guards.
- Reconstructed room maps and a `LEVELS.DAT` decoder live under `analysis/`; a published
  full-game TAS reconstruction lives under `oldScripts/`.

## Movement tricks discovered (level noted where confirmed)
- **Guard edge-wrap**: herd a guard to a room corner and nudge so its X byte over/
  underflows to the opposite side of the *same* room; crossing then carries into the next
  room. Confirmed on level 4.
- **Wall-clip**: a guard bump into the left wall can clip the character *down* through the
  floor. Key on level 8; note that down-clips elsewhere tend to be dead-ends.
- **Sword lock-and-drag** ("moonwalk"): sheathe/redraw the sword to lock onto a guard at
  zero horizontal distance, then retreat to drag it quickly one direction. Confirmed on
  level 7.

## Segment re-search method
- Organize by `LLSS` segment folders with mid-climb checkpoints. Use an interpolated
  vertical-position climb reward so partial climbs are rewarded smoothly.
- Levels 1 and 2 came out optimal; combat-heavy segments explode the state DB, so hash
  combat state carefully and **always verify a solution visually** — Pos-Y-bucket win
  conditions and inert placeholder tiles have both produced false positives.

## Custom levels & visualization
- You can forge a chamber by injecting into a `LEVELS.DAT` slot (no checksum); copy
  another level's tiles, set the levels-file path, and cold-boot. Guards spawn on room
  transition.
- Headless screenshots (`player --screenshotDir/--screenshotSteps`) plus a video script
  render any solution or single frame for review.

## PAUSED investigation: L4 dead-guard-on-opener door skip (state as of 2026-06-20)

Working dirs `0400/` and `0400b/`. The goal — **confirmed end-to-end viable**: if a dead
guard's body lands on the opener tile at room 4 (col 7, row 1), `check_press` routes it to
`died_on_button` → the tile becomes debris → the **level-exit door (room 24, tilepos 12)
opens permanently**. `Level Door Open == 1` is the real win signal (a forced col-nudge test
proved the full trigger chain).

- **How to get there**: edge-wrap the second guard from room 7 into the mirror room (room 4),
  strike it down there. Files: `0400b/state_room7`, `full_move.seq`, `state_guard_room4`,
  `state_standing` (byte-edited standing Player: pose block at save offsets 2504/2556; set
  frame=15, y=118, action=0, sword=0, curr_seq=0x19A0 — curr_seq=0 plays garbage).
- **Search needs a staged guard-counter reward** (`Guard Current HP <= 3/2/1` → big rewards
  + checkpoints); the reward is flat until first contact otherwise. Contact = SHIFT press,
  ~19 frames per exchange.
- **THE BLOCKER**: the body always lands col 8 (torch tile), one column right of the opener.
  Landing col is NOT a function of death position (col-0 and col-1 kills both fling to
  x≈184/col 8) — it's the death-anim `char_dx_forward` displacement, negated by the guard's
  **facing direction**. Dying facing LEFT sweeps cols 8→9→8, never 7. Strong inference:
  col 7 needs the guard to die **facing RIGHT**, i.e. the Player on the guard's right — which
  conflicts with keeping the Player left of the col-4 split wall (the constraint that stops
  the Player from just pressing the opener personally via the row-0 upper floor, which is
  the known "cheat" solution `0400/door.sol`, 482 steps).
- Constrained search `0400/door2.jaffar` (Player locked to cols 0–3 via fail rule; col-8
  dead-body prune; 100 GB DB) found nothing (stopped, not exhausted). Untried angles:
  forbid only the opener tile itself (not the whole right half) so the guard can die facing
  right; or hunt a death variant (frame/sub-pixel/knockback) that flings col 7 facing left.
- Room-4 geometry: row1 tiles `[19,1,19,1,20,1,3,15,19,1]` — wall col 4, pillar col 6,
  opener col 7, torch col 8; upper floor row 0 cols 4–9.

## Engine naming note

`games/sdlpop/princeOfPersia.hpp` property names use **`Player …`** (22 properties + 3
magnet actions renamed from `Kid …`; configs under `0400b/` use the new names). A deeper
blanket rename inside miniPoP internals compiles but segfaults at runtime — don't redo it
without finding the break. Rebuild: `ninja -C build-sdlpop`.
