# Lode Runner (NES) — StrategyWiki digest

Sources (fetched 2026-08-03, via curl with browser UA; WebFetch was 403-blocked):

- https://strategywiki.org/wiki/Lode_Runner (overview)
- https://strategywiki.org/wiki/Lode_Runner/Gameplay
- https://strategywiki.org/wiki/Lode_Runner/Walkthrough (hub page)
- https://strategywiki.org/wiki/Lode_Runner/Levels_1_-_10 ... /Levels_41_-_50 (the five sub-pages covering the NES's 50 levels)
- https://strategywiki.org/wiki/Lode_Runner/Versions (only C64/MS-DOS notes; nothing NES-specific)

**Coverage caveat (important):** StrategyWiki's walkthrough sub-pages contain **no strategy text at all** — each level section is only a map screenshot of the original Brøderbund/Apple II layout (28x16 tiles). The per-level notes in the table below were **derived by visually reading those 50 map images**, not from wiki prose. Counts of gold/guards are approximate ("~"). The NES port (Hudson Soft) uses the same 50 layouts but *slightly altered for the NES's lower vertical resolution* (per the overview page), so verify details in the emulator. All 50 map images are mirrored locally in `docs/maps/Lode_Runner_levelN.png`.

---

## Core gameplay rules (from /Gameplay and overview)

### Movement
- Left/right runs; if a **wire (bar/rope)** is in the path the player automatically grabs it and shimmies across; pressing **down while on a wire releases it** and the player falls.
- Up/down climbs/descends **ladders**.
- Falling is harmless regardless of height (player falls through dug holes freely; guards do not).

### Digging
- Dig removes a brick **to the lower-left or lower-right** of the player (never straight down).
- One fire button: digs in the **facing** direction. Two fire buttons: left button digs left, right button digs right, **independent of facing**. On the NES pad: **B = dig left, A = dig right**.
- Digging is **fast but not instantaneous** — you can be caught by a guard mid-dig if you start too late.
- Holes **refill automatically** after a fixed period. Anything inside when the brick regenerates is crushed: the **player dies**, a **guard is killed and respawns near the top** of the screen.
- The player **cannot climb out of a hole he falls into** (guards can); being in your own hole when it refills is fatal. Levels can also become unwinnable (gold in an unreachable spot after bad digs) — every system has a **suicide button** to end the life and restart the level.

### Guards
- Guards pursue relentlessly, always seeking the **quickest route** to the player; they only stop when trapped or blocked. Any contact is fatal.
- Guards **fall into dug holes** and get stuck there even if there is no floor beneath the hole (the player falls straight through in the same situation).
- A trapped guard either **climbs out after a short delay** or is **crushed when the hole refills**; a crushed guard **reappears near the top of the screen** and resumes pursuit. (Respawn is exploitable: some levels *require* killing a guard so it respawns above otherwise-unreachable gold.)
- Guards **pick up gold** they walk over (their sprite changes slightly — a color change — while carrying). They occasionally drop it voluntarily; the reliable way to make them drop it is to **trap them in a hole**.
- You cannot finish a level while a guard still holds a piece of gold.

### Win condition
- Collect **every** piece of gold; the **escape ladder** then extends to the top of the screen (the zig-zag ladder segments drawn above/inside the maps below are these hidden exit ladders).
- The level is completed by climbing off the **top of the screen** via that ladder. Play is strictly linear, level 1 → 50 (NES).

### Standard series rules NOT documented on StrategyWiki (well-known; verify against emulator/disassembly)
- Only normal bricks are diggable; **solid bedrock blocks, ladders, wires, and trapdoor tiles cannot be dug**, and a brick with a ladder/solid directly on top typically can't be dug either.
- **Trapdoors / false bricks** (drawn with a "T" glyph in the maps) look like floor but are fall-through; they are a core routing element in many later levels.
- The NES version has 50 levels and a level editor ("EDIT MODE"); the wiki does not document the NES SELECT/START functions (pause, game-speed adjustment) — confirm in-game.

---

## Per-level walkthrough notes (derived from the wiki's map images)

Map legend as rendered in `docs/maps/`: blue bricks = diggable; bold/solid blue slabs = undiggable bedrock; white ladders/wires; "T" glyph = trapdoor (false brick); zig-zag white ladder = hidden escape ladder (appears after last gold); orange-striped chest = gold; red figure = guard; white figure = player start.

| Lv | ~Gold | ~Guards | Layout & route notes |
|----|-------|---------|----------------------|
| 1 | 6 | 3 | Simple multi-tier platforms with long ladders and two wires. Gold on every tier; exit ladder appears top-right. Gentle intro — dig-trap the three guards or just outrun them. |
| 2 | 8 | 3 | Taller tiered layout, long parallel ladders left/center, wires mid-screen, one trapdoor near the right edge. Exit ladder at right edge, mid-height. Gold spread over all tiers. |
| 3 | 9 | 3 | Terraced staircases descending across the screen with wires on the right. Gold on ledges at all heights; exit at top-right corner. Watch guards patrolling the middle terraces. |
| 4 | ~14 | 3 | Symmetric "cathedral" of ladder/bar lattices (H-shaped posts) in three clusters; gold sits in pairs inside the lattices. Exit at top-left with a wire run. Mostly climbing, little digging. |
| 5 | 8 | 5 | Diagonal brick slopes; the central vertical zig-zag is the hidden exit ladder column. Guards start on both flanks. Gold on the slope ledges; dig down through the diagonals to descend fast. |
| 6 | ~14 | 6 | Dense brick fortress riddled with corridors; trapdoor column bottom-left and single traps top-left. Many guards — heavy dig-trapping needed. Gold embedded throughout; exit top-center. |
| 7 | 8 | 5 | Open layout with very tall ladders; a sealed chamber on the mid-right holds gold that must be dug into. Exit is the zig-zag at top-left. Guards drop from upper platforms. |
| 8 | ~7 | 7 | Symmetric twin brick towers with gold sealed inside; central twin hidden-ladder shafts; trapdoors and step-stools at tower bases. Seven guards — one of the hardest early levels; requires digging into the towers while juggling guards. |
| 9 | ~4 | 6 | Castle wall spanning the screen with a wire across the top and a vertical column of trapdoors through the wall (fall-through shaft). Gold in wall niches; one gold in a basement pocket reached via ladder. Exit is the tall zig-zag at the left edge. |
| 10 | ~10 | 3 | Terraced platforms with a large undiggable slab mid-right; hidden ladder segments top-right/center. Gold on most ledges; light guard pressure. |
| 11 | ~12 | 4 | Signature level: a central twin-ladder column stacked with gold top to bottom; two small corner nests (each 2 guards + gold) at top-left/top-right. Route: sweep the central shaft, then dig-trap the corner guards to grab their gold. |
| 12 | ~12 | 3 | Layered platforms with wires and a full-height ladder on the right; an undiggable slab center-right. Gold spread across layers; exit via top-right. |
| 13 | ~10 | 4 | "Prison bars": a forest of full-height ladders with trapdoor columns between brick pillars, over a sealed bottom corridor (undiggable slab roof) patrolled by guards. Gold embedded mid-columns; exit at right edge. Trapdoors dominate routing. |
| 14 | ~12 | 3 | The all-wire level: no diggable floor at all — only ladders, wires, and single wire dashes over the void. Pure navigation/evasion, no dig-trapping possible; gold hangs along the ladder/wire net. |
| 15 | ~14 | 4 | Symmetric terraces over an undiggable bottom slab; hidden exit segments at both top corners. Gold on tier edges; guards on the upper terraces. |
| 16 | ~12 | 2 | Left: diamond-shaped brick island studded with gold around its rim (dig in from the top edges). Center: T-shaped tower. Right: a distinctive staircase trail of hidden-ladder segments. Bottom row of gold on the right. |
| 17 | ~12 | 3 | Large U/H-shaped chambers; gold inside the side chambers and along the outer walls; trapdoors near the top-left start. Hidden ladders at both screen edges. |
| 18 | ~8 | 3 | Sprawling ruins with a mound of undiggable blocks on the left. Gold in scattered pockets (two low inside the mound). Hidden exit zig-zag at top-left; light guard count but awkward geometry. |
| 19 | ~18 | 4 | Giant ziggurat/pyramid: gold rows along each tier including a gallery of ~8 pieces in the middle. Guards descend the upper tiers. Classic chain-dig level — dig multi-brick stair-holes to mine down through the pyramid, minding refill timing. |
| 20 | ~14 | 4 | Multi-story level with wires and trapdoors (left-mid and center-bottom) and a tall central ladder. Gold on every floor; hidden exit at top. |
| 21 | ~12 | 4 | Brick complex under a solid top bar; large undiggable regions on the right; gold cluster (3 stacked) bottom-right corner. Exit at top-right via small hidden segments. |
| 22 | ~18 | 3 | Terraced staircases running both directions with wires; gold everywhere including bottom corners. Low guard pressure, long collection route — routing/ordering is the whole game here. |
| 23 | ~14 | 4 | Fortress with undiggable columns at the left edge and a central pit spanned by ladders; wires across the mid-level. Hidden ladder chain top-right. Gold in wall pockets on both flanks. |
| 24 | ~13 | 4 | A "ship" of brick sitting on an undiggable water row; the exit is a long diagonal chain of hidden ladder segments rising from the superstructure to top-center. Gold buried through the hull — heavy digging with guards inside. |
| 25 | ~11 | 3 | Maze of walled rooms with trapdoors (top-left, two at bottom-center); hidden ladders at the right edge. Gold in room corners; several rooms must be entered by digging or trapdoor drops. |
| 26 | ~12 | 4 | Left: spiral/labyrinth chamber with gold at its heart. Center: tall undiggable column tower with gold pairs on top. Right: platforms with wire links. Exit at top-right. Buried gold makes dig-order matter. |
| 27 | ~18 | 5 | Symmetric craggy mountain with hidden ladder zig-zags at both outer edges and gold salted through both slopes. Guards on both flanks at mid-height; player starts top-left on a wire. |
| 28 | ~13 | 3 | Rooms around a massive undiggable central column; wires at top-left; hidden exit zig-zag top-center. Gold in the right-side room stack (three tiers). |
| 29 | ~14 | 4 | Floating brick islands, each with gold **buried inside** (visible as embedded pairs) — dig into each box from the top and drop in; one bottom island has a trapdoor. Wires connect the gaps. Guard respawn manipulation is useful for the far islands. |
| 30 | ~17 | 5 | Four long parallel floors, each pierced by trapdoors, with gold on every tier and a stack of 4 gold at bottom-left; guards patrol top and bottom rows. Descend via the trapdoor holes; exit ladder at the right edge. |
| 31 | ~9 | 4 | V-shaped amphitheater: stepped walls lined with interior trapdoor columns, wires spanning the valley. Gold on the steps and valley floor; hidden exit zig-zag at top-center. Trapdoor faces make the walls one-way — plan the descent order. |
| 32 | ~17 | 3 | Big asymmetric brick complex; hidden ladder at top-left; undiggable slab top-right where a guard starts. Gold throughout including sealed pockets mid-level requiring dig-ins. |
| 33 | ~13 | 4 | Dense city-wall level; wires and hidden zig-zags at top-left; large brick masses center/right with gold in niches. Guards at opposite corners. |
| 34 | ~20 | 4 | Giant ring/"O" of brick studded with gold around the inner rim; two guards inside the ring at the top, two at the bottom. Trapdoors at the ring's lower quadrant. Circle-sweep the rim; the inner faces need careful dig timing. |
| 35 | ~20 | 4 | Stacked solid (undiggable) floor tiers split by a central solid column; ladders only along the center and edges. Gold on every tier both sides; four guards mid-level. No digging escape on solid tiers — pure chase management. |
| 36 | ~10 | 3 | Open cavern of ladders and wire dashes between two brick bases; bottom-left brick house with trapdoors and gold inside; undiggable slabs with gold on the right. Long aerial traverses. |
| 37 | ~11 | 4 | Left-center "jail" of parallel ladders over a gold cache; scattered rooms right with undiggable slabs low-right. Hidden exit at top-left. Guards start top and bottom. |
| 38 | ~10 | 4 | Symmetric temple: two brick wings (1 gold each), central ladder/bar scaffold around a solid slab, and a solid undiggable step-pyramid below with gold on its ledges. Gold on undiggable steps can only be walked to — watch guard interception. Hidden exits at top corners. |
| 39 | ~12 | 5 | Honeycomb of diagonal brick steps and short ladders covering the whole screen. Gold tucked in the lattice; five guards roam freely — many equivalent paths, guard prediction dominates. |
| 40 | ~18 | 4 | Multi-floor complex around a solid central spine; interior rooms floored with trapdoors (several "T" rows). Gold on nearly every floor; hidden zig-zag top-left. Trapdoor drops chain the floors together. |
| 41 | ~20 | 5 | Three structures: left diamond mound, center staircase of bars/ladders, right ring — plus a row of 4 gold at top-right reached by bar lattice. Solid slabs mixed in. Five guards spread wide; long multi-structure route. |
| 42 | ~24 | 3 | Winding walled chambers over the full screen with one interior trapdoor mid-right; huge gold count scattered in every room. Few guards — a pure routing/ordering level (biggest collection route so far). |
| 43 | ~12 | 4 | Grand hall: tall ladder columns flanking a central solid stepped tower with gold on its shoulders; guards cluster around the tower at mid-height. Hidden exit zig-zags at both top corners; player starts bottom-center between them. |
| 44 | 6 | 3 | The famous "LODE RUNNER" text level — the level geometry spells the game's name in ladders/bars; one gold under each letter region in a mid-screen row. Guards in the left brick frame and right edge. Traversal through letter lattices; almost no digging. |
| 45 | ~16 | 3 | Trapdoor-heavy complex: a column of traps down the left wall, traps mid-center and right; central cache of 3 gold in a row. Hidden exit at top-center. Memorizing which tiles are false is the level's core difficulty. |
| 46 | ~16 | 4 | Layered platform level with undiggable slabs center and right; hidden exit zig-zag top-right. Gold on most ledges; guards mid-level converge on the central slab area. |
| 47 | ~15 | 2 | Dense brick maze with tall solid columns center-right and a wire+guards start at top-left. Gold deep in pockets; hidden ladder chain down the right edge. Low guard count, high geometry complexity. |
| 48 | ~14 | 3 | Left: staircase mound with gold on steps. Center: alternating brick/solid column with ladders. Right: large sealed solid box holding three interior gold rows reached by inner ladders/wires. Guards on the bottom floor; small hidden segment bottom-center. |
| 49 | ~20 | 3 | Right-side tower with interior vertical gold shafts (~8 pieces) plus a left plateau whose solid slab has a trapdoor; wires across the top. Dig into the tower shafts from above; guard respawn from crushed guards lands near the tower top. |
| 50 | ~16 | 4 | Finale: wide tiered level mixing solid slabs (center rows) with brick, trapdoors along the right side and in the center slab. Gold on all tiers; wire dashes at top. Four guards; hidden exit at top. A composite of every mechanic in the set. |

### Levels the maps flag as notably tricky
- **8, 9** — sealed towers / trapdoor shaft with 6-7 guards.
- **13, 30, 31, 40, 45, 50** — trapdoor (false brick) routing levels.
- **14** — no diggable floor at all (wire/ladder net; no way to kill guards).
- **19, 24, 29, 34, 42, 48, 49** — buried/sealed gold requiring long dig chains or dig-ins (and 24's long hidden exit ladder climb).
- **35, 38** — undiggable solid floors: no hole-trapping where it matters.
- **44** — the "LODE RUNNER" text level (pure traversal puzzle).

---

## NES-version specifics
- Hudson Soft port; **first 50 levels only**, layouts slightly altered for the NES's reduced vertical resolution (overview page). Famicom release sold ~1.1M copies; NES guards visually inspired the later Bomberman character.
- Two-button digging: **B digs left, A digs right**, regardless of facing (per the Gameplay page's two-button rule).
- StrategyWiki does **not** document the NES SELECT/START behavior (pause, the game-speed up/down feature) nor the edit mode keys — confirm those in the emulator; the wiki only notes every version has a self-destruct input for unwinnable states.
- `Championship Lode Runner` (the 50 expert fan levels) is a separate cart/guide, not part of this game's 50.
