# NES Lode Runner (U) — GameFAQs Guide Digest

Source: GameFAQs FAQ 40192 by Andrew Schultz (v1.0.0, 2005), fetched via Wayback
Machine snapshot (2019-03-13) of
https://gamefaqs.gamespot.com/nes/587420-lode-runner/faqs/40192
Full extracted text kept only in session scratchpad; this file is the distilled digest.

## Objective / win condition

- Collect ALL gold chests on the screen, then reach the TOP of the level to advance.
- When the last chest is collected, an exit ladder (or ladders) APPEARS at a
  level-specific location (e.g. UR on level 3, inside the V on level 5, center+UR
  on level 10, center on level 38). If you think all gold is gone and no ladder
  appeared, a guard is still holding a chest somewhere.
- Level 50 loops back to level 1 ("weak last level, weak ending" — no real ending).

## Tile types (10 pieces)

| Tile | Behavior |
|---|---|
| Empty | walk/fall through |
| Brick | walkable; diggable; hole lasts ~5–6 seconds then refills |
| Cement | walkable; NOT diggable |
| Ladder | climbable; can drop off sideways; CANNOT dig the brick under a ladder |
| Rope | traverse hand-over-hand; press down to drop; CANNOT dig brick under a rope |
| False brick | looks like brick; you fall through unless a rope/ladder is directly above (then falling in is optional). Guards fall through too |
| Gold chest | collectible; CANNOT dig the brick under a chest |
| Guard ("monk") | enemy; can be trapped in dug holes and used as a bridge |
| Player | you |

## Dig mechanics

- A = dig right, B = dig left (dig is always diagonal-down into the adjacent brick).
- Hole regeneration: an impression "lasts six seconds or so" (elsewhere the guide
  says a start-of-level hole buys "five seconds"). Treat regen ≈ 5–6 s.
- A refilling brick KILLS anything inside it — you die; a guard dies and
  regenerates at various places along the TOP of the screen (respawn columns are
  level-dependent; e.g. level 9 guards respawn in the castle towers, and notably
  NOT in the right tower — some spawn slots are per-level).
- You free-fall through a hole (and through holes chained below); guards that
  fall in are stuck until the hole refills (or until dug out from the side).
- Multi-layer digs are done wide-to-narrow: a "4-3-2-1 dig" = dig 4 holes, drop
  in, dig 3, drop, dig 2, dig 1. Wide staircase digs are required for buried gold.
- You can dig while moving: hold left/right + dig button to step-and-dig. To dig
  from a ladder you must hold up/down then RELEASE the vertical direction to dig.
- Digging while walking makes you move at guard speed (see AI below).

## Guard AI

- Default bias: guards go LEFT when they have no better plan.
- Speed: guards move at HALF player speed, and also FALL at half speed.
  Player digging-while-moving = guard speed (equal).
- Targeting: guards try to get on the same ROW as you — they home when they could
  walk to you on that row if no blocks intervened. A guard in a pit between you
  and another guard won't path at you, but a bottom wall between you makes them
  charge. Guards flip-flop direction as you climb a ladder depending on your
  height — exploitable to steer them.
- Trapped guards: fall into a dug hole → stuck; you can walk across their head.
  On NES it is harder than on the Apple to run over a lone guard below you, and
  running over TWO trapped guards requires them in directly adjacent holes.
- If you start a level falling onto a guard, tap down-right-down-right to slide off.

## Gold and guards

- Guards ALWAYS pick up gold they run over; they drop it at (pseudo)random while
  running. Long runs make them drop it sooner.
- Deterministic drop spot: a guard circling a ladder eventually leaves its chest
  at the TOP of the ladder — guards cannot pick gold up on the turn from
  ladder-top to brick. Lazy way to force a cough-up.
- Sunk gold: if a chest is dropped into a brick-hole that has another brick-hole
  directly above it, the chest never appears BUT is counted as collected
  (0 points for it). Sinking gold is a legit way to "collect" it.
- Bug: a guard releasing a chest while falling off a rope (left of a ladder-top)
  can park gold in an unreachable spot; other guards can re-grab it while circling.
- Killing a gold-carrying guard (hole refill) makes it respawn; the chest is
  dropped/handled per the above — dig traps to make carriers give chests up.

## Scoring / lives

- Points are awarded only ON LEVEL COMPLETION: 100 per guard trapped in a hole,
  100 per gold chest (0 if sunk). No end-of-level time bonus. Death before
  solving = 0 for the level.
- Lives cap at 9 (maximum nine lives).
- Score-display rollover bug: with score ≥ 64000, gaining 16000 on a level shows
  7*000 where * = char('0'+10) — same class of bug as SMB lives counter. Fixes
  itself when the tens-of-thousands digit flips again without a 10000+ gain.
- One-off oddity: author saw a 1600-point "cherry tomato" bonus item appear at
  the start position on level 11 (unexplained spawn).

## Buttons / meta features (TAS-relevant)

- SELECT: level select — change to a different level at will. Also RESTARTS the
  current level WITHOUT losing a life (press select then start right after).
- START: pause (used to survey the level, since the NES viewport shows only part
  of the screen and scrolls AGAINST your facing — it hides what's ahead in your
  direction of travel; no free scroll, unlike Championship Lode Runner).
- SELECT+START: skips the level-start music — "saves a lot of time". The level
  panorama pan and pre-level pause cannot be skipped.

## NES vs Apple/Broderbund original

- NES has 50 levels; Apple original had 150 levels plus a level editor and high
  score list. NES guide covers levels 1–50, comparable to the Apple's first 50.
- NES CUTS OFF THE TOP TWO ROWS of each level (levels are simpler than Apple
  counterparts; maps in the guide are the NES 28-wide x ~13-tall layouts).
- NES scoring differs from Apple. No view scrolling. Easier level-start interface.
- Known bad/pirated ROM dumps exist; a good copy shows walls on both sides at
  level start.

## Per-level strategy highlights (tricky levels)

- L2: false brick immediately LEFT of the ladder to the top — approach the exit
  ladder over the cement or you can be dropped into guards. First buried-gold
  dig: 2-then-1 from the left, drop, dig left (X21/XX3 pattern).
- L4: dig at the rope edge to release guards from the top compartment — they
  release their chest even without hole traps.
- L6: heavy buried gold; standard trick is baiting all 3 guards onto the right
  ladder where they stay stuck, then doing the 1-2-3/4-5/6-7 layered digs.
- L9: guards respawn in the castle towers (never the RIGHT tower) — circle the
  UL to cycle spawns without digging; two prescribed dig sequences for the
  center vault and right tower.
- L13: a false brick sits ABOVE and BELOW every gold chest — never walk into a
  chest, walk above it and fall on it.
- L14 (rope forest): grabbing gold with no brick beneath it FAILS — you must
  drop onto gold from a rope or ladder side-fall.
- L16: dig above the 2-ladder and note the hole gives extra escape time; you can
  buy time by digging right while on a ladder.
- L25: start by digging a hole and waiting ~5 s; one guard falls through a false
  brick and exits right. False brick at the bottom of the right-of-ladder drop
  and left of the UL top rope (exit route).
- L30, L42: bait guards into pits where gold was — pits double as guard prisons.
- L35: checkerboard of false bricks; pre-dig holes ahead of runs.
- L43: jiggle up/down a ladder to flip-flop guard directions until one wedges.
- L44 ("LODE RUNNER" letters): hide on 2nd-top rung between O and D; guards
  crowd the ladder left of you; most gold must be fallen onto.
- L50: after the DR pit guard is released, dig-fest the bottom-left; exit via UL
  ladders → wraps to level 1.

## Misc tactics from the AI section

- Hiding/waiting beats plowing through guards (they're half speed anyway).
- With 3 guards and you in the middle, run toward the side with fewer guards;
  leave yourself 2 tiles of lead to have time to dig a hole.
- Standing level-with-bricks at certain heights makes guards turn away (used in
  L12); ladder-rung height manipulation steers guard direction globally.
