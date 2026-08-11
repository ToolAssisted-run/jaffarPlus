#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>
#include <queue>

namespace jaffarPlus
{

namespace games
{

namespace nes
{

// NES Lode Runner (Mapper 0). RAM semantics documented in examples/nes/lodeRunner/docs/.
// Positions are tile coords (X 0-27, Y 1-13, Y=1 top) plus 1/8-tile sub-offsets; the mutable
// 28x13 tile map lives at $0200-$0567 and must be hashed (digging changes traversability).
// There is no dedicated RNG: enemy respawn columns and gold drops sample the free-running
// counters $0053 (spawn timer) and $009E (global timer).
class LodeRunner final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "NES / Lode Runner"; }

  LodeRunner(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    // Parsing configuration
    _lastInputStepReward = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Last Input Step Reward");

    // The spawn/global timers are the game's only "randomness" source. Hashing them preserves
    // luck-manipulation correctness but multiplies state count; allow relaxing per-search.
    _hashTimers = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Timers");
    // Quantization mask for all timer-class hash bytes ($53, phase work, enemy timers): 3 =
    // mod-4 residues (frontier-compact), 255 = full value (luck-exact; needed where enemy-phase
    // aliasing merges future-divergent twins, e.g. narrow corridor funnels at tiny populations).
    _timerHashMask        = _gameConfigRemaining.contains("Timer Hash Mask") ? jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Timer Hash Mask") : 3;
    _pointMagnetTileBasis = _gameConfigRemaining.contains("Point Magnet Tile Basis") ? jaffarCommon::json::popBoolean(_gameConfigRemaining, "Point Magnet Tile Basis") : false;

    // Route-waypoint odometer: the reference trajectory as an ORDERED chain of tile waypoints
    // ("x y" per line, consecutive duplicates collapsed at generation). Reward = (waypoints
    // passed, in order, latched) x "Route Waypoint Reward" -- strictly monotone along the
    // reference by construction; being further along the route IS the reward. "Route Next
    // Pull" adds a small tile-Manhattan pull toward the next waypoint to break plateau ties
    // (keep small: guidance, not exploration).
    _routeWpReward = _gameConfigRemaining.contains("Route Waypoint Reward") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Route Waypoint Reward") : 0.0f;
    _routeNextPull = _gameConfigRemaining.contains("Route Next Pull") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Route Next Pull") : 0.0f;
    if (_gameConfigRemaining.contains("Route Waypoints File"))
    {
      const auto p = jaffarCommon::json::popString(_gameConfigRemaining, "Route Waypoints File");
      if (p.empty() == false)
      {
        // Format: "x y g" per line -- g is the REQUIRED banked-gold count ($C4) to credit the
        // waypoint (the reference's own count when it passed that tile). Sections the route:
        // waypoints beyond a pickup cannot be credited by a lineage that shortcuts the gold.
        std::ifstream f(p);
        if (f.good() == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not open 'Route Waypoints File': '%s'\n", p.c_str());
        int x, y, g;
        while (f >> x >> y >> g)
        {
          _routeWps.push_back({(uint8_t)x, (uint8_t)y});
          _routeWpGold.push_back((uint8_t)g);
        }
      }
    }

    // Camera X only matters during the level-start scroll-in (it gates the select scroll-skip);
    // during normal play it follows the player and only adds state noise.
    _hashCamera = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Camera");

    // Enemy 1/8-tile sub-offsets multiply the state space enormously (64^2 per guard). Tile
    // position + timer + facing usually give enough search diversity; disable for big levels.
    _hashEnemyOffsets = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Enemy Offsets");

    // Player sub-offsets: merging them (false) trades movement-timing precision for a ~64x
    // smaller state space -- useful to just complete a big level, not for frame optimization.
    _hashPlayerOffsets = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Player Offsets");

    // Distance semantics for the nearest-chest magnet: "Manhattan" (straight tile distance) or
    // "Path" (directed search over the live tile map following movement rules: falls are one-way,
    // climbing needs ladders; chests with no walk path fall back to Manhattan so the gradient
    // never vanishes -- excavation shaping remains the dig-progress magnet's job).
    const auto distanceMode = jaffarCommon::json::popString(_gameConfigRemaining, "Nearest Chest Distance Mode");
    if (distanceMode == "Path")
      _nearestChestPathMode = true;
    else if (distanceMode == "Manhattan")
      _nearestChestPathMode = false;
    else
      JAFFAR_THROW_LOGIC("Unrecognized 'Nearest Chest Distance Mode': '%s' (expected 'Manhattan' or 'Path')", distanceMode.c_str());

    // Extra path cost for stepping through an enemy-occupied tile in Path mode (0 = ignore
    // enemies; they chase the player, so occupancy is transient -- use only a mild bias)
    _enemyPathCost = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Enemy Path Cost");

    // The game has no strict frame loop: it runs as much logic as fits each frame, so audio
    // playback state (music tempo counters, active sound effects) changes the per-frame cycle
    // budget and thereby FUTURE lag patterns. Two states equal everywhere but audio genuinely
    // diverge later -- hash the RAM audio variables ($B7-$B9, $D0-$DF) to keep them distinct.
    _hashSoundState = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Sound State");

    // Because of that same loose frame loop, the between-frame CPU execution point and IRQ/APU
    // schedule (emulator-internal, RAM-invisible) are genuine game state: RAM-identical states
    // captured at different main-loop points diverge later. Hash the emulator's "Cycle Phase"
    // digest to keep them distinct (costs state diversity; required for frame-exact lineages).
    _hashCyclePhase = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Cycle Phase");

    // Work-RAM phase bytes mined from twin divergence experiments: hash-equal states that later
    // diverge differ in these unhashed bytes ($26/$28/$3B player-adjacent work, $55-$5B spawn
    // timer work area, $1F6 sound work). Hashing them separates causal phase classes that the
    // main hash merges -- the tolerance-0 floor's cancel depth measures whether the set suffices.
    _hashPhaseWorkRam = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Phase Work RAM");

    // PROVISIONAL diagnostic: hash the ENTIRE 2KB work RAM -- the ultimate RAM-discriminant test
    // (states merge only when byte-identical in all of RAM). Pair with a large state DB; if a
    // window still fails tol-0 replication under this, RAM is exhausted as the divergence carrier.
    _hashFullLram = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Full LRAM");

    // Transient-input hashing (controller latch bits + facing): required WITHOUT engine-level
    // Hash Lookahead (twins merge otherwise), redundant WITH it (the same information appears
    // in RAM after the lookahead advance). Default on; disable in lookahead configs.
    _hashInputLatch = _gameConfigRemaining.contains("Hash Input Latch") ? jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Input Latch") : true;

    // Bisection instrument: hash arbitrary extra LRAM ranges ([start, end) pairs) on top of the
    // standard set -- for locating a missing causal byte by halving (stage50 K=150 protocol).
    for (const auto& e : jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Hash LRAM Ranges"))
    {
      if (e.size() != 2) JAFFAR_THROW_LOGIC("'Hash LRAM Ranges' entries must be [start, end) pairs");
      _hashLramRanges.push_back({e[0].get<uint16_t>(), e[1].get<uint16_t>()});
    }

    // One-off latched bonus for digging a specific cell (e.g. the brick whose removal frees a
    // gold-carrying enemy: reward-inert for ~60 frames otherwise, so beam retention drops the
    // lineage). Latch sets when the dig registry shows a hole at the cell; serialized + hashed.
    _digBonusReward = _gameConfigRemaining.contains("Dig Bonus Reward") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Dig Bonus Reward") : 0.0f;
    // Companion latch: a gold-carrying enemy standing IN the bonus cell's hole (the burial that
    // precedes the surrender). Funds the re-trap action itself, which is otherwise reward-inert.
    _buryBonusReward = _gameConfigRemaining.contains("Bury Bonus Reward") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Bury Bonus Reward") : 0.0f;
    // Ordered dig chain: cells that must be dug in SEQUENCE (each latches when the dig registry
    // shows an active hole there), gated on a minimum banked-gold count. Each advance pays
    // "Dig Chain Reward" permanently; progress is serialized + hashed lineage state.
    _digChainReward   = _gameConfigRemaining.contains("Dig Chain Reward") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Dig Chain Reward") : 0.0f;
    _digChainGoldGate = _gameConfigRemaining.contains("Dig Chain Gold Gate") ? jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Dig Chain Gold Gate") : 0;
    if (_gameConfigRemaining.contains("Dig Chain Cells"))
      for (const auto& e : jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Dig Chain Cells"))
      {
        if (e.size() != 2) JAFFAR_THROW_LOGIC("'Dig Chain Cells' entries must be [x, y] pairs");
        _digChainCells.push_back({e[0].get<uint8_t>(), e[1].get<uint8_t>()});
      }
    // One-off reward paid (latched) once a grounded gold has appeared at the watch tile.
    _goldDepositReward = _gameConfigRemaining.contains("Gold Deposit Reward") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Gold Deposit Reward") : 0.0f;

    // Watch tile for an enemy carrying-to-dropped transition ("Enemy Drop Watch": [x, y]).
    if (_gameConfigRemaining.contains("Enemy Drop Watch"))
    {
      const auto w = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Enemy Drop Watch");
      if (w.size() != 2) JAFFAR_THROW_LOGIC("'Enemy Drop Watch' must be an [x, y] pair");
      _enemyDropWatchX = w[0].get<uint8_t>();
      _enemyDropWatchY = w[1].get<uint8_t>();
    }
    if (_gameConfigRemaining.contains("Dig Bonus Cell"))
    {
      const auto c = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Dig Bonus Cell");
      if (c.size() != 2) JAFFAR_THROW_LOGIC("'Dig Bonus Cell' must be an [x, y] pair");
      _digBonusCellX = c[0].get<uint8_t>();
      _digBonusCellY = c[1].get<uint8_t>();
    }

    // Tour lookahead for the nearest-gold pull: weights[0] applies to player -> nearest gold,
    // weights[k>=1] to the k-th greedy nearest-neighbor hop over the REMAINING golds (g1 -> g2,
    // g2 -> g3, ...). The tail terms rank states by the compactness of the tour they leave
    // behind, so collection orders that strand a far gold for last score worse. [1.0] restores
    // plain nearest-gold behavior. Chain hops use path distances in Path mode (cached with the
    // field); Manhattan mode applies weights[0] only.
    _lookaheadWeights = jaffarCommon::json::popArray<float>(_gameConfigRemaining, "Gold Lookahead Weights");
    if (_lookaheadWeights.empty()) JAFFAR_THROW_LOGIC("'Gold Lookahead Weights' must contain at least one weight");

    // Strategy mix: w_ref blends the REFERENCE-following family (in-order prefix credit,
    // leg magnets set by rules) against the EXPLORATORY family (any-order banked, any-gold
    // chain). Start levels near 1.0 (follow the reference), relax toward 0 after wins.
    _referenceStrategyWeight = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Reference Strategy Weight");

    // Trace magnet: reward closeness to the reference's recorded (x,y) at the SAME search step
    // (step-indexed trace file, "x y" per line). The definitive replicate-first guide when the
    // route geometry defeats field-based gradients (stage50: the reference detours around
    // enemies invisible to the path field, so its own leg score wobbles and the frontier chases
    // a doomed shortcut instead).
    _traceMagnetIntensity = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Trace Magnet Intensity");
    const auto tracePath  = jaffarCommon::json::popString(_gameConfigRemaining, "Trace File");
    if (tracePath.empty() == false)
    {
      std::string traceData;
      if (jaffarCommon::file::loadStringFromFile(traceData, tracePath) == false) JAFFAR_THROW_LOGIC("Could not read 'Trace File': %s", tracePath.c_str());
      for (const auto& line : jaffarCommon::string::split(traceData, '\n'))
      {
        if (line.empty()) continue;
        const auto parts = jaffarCommon::string::split(line, ' ');
        if (parts.size() >= 2) _tracePoints.push_back({(float)std::stoi(parts[0]), (float)std::stoi(parts[1])});
      }
    }

    // Path-aware choreography leg: module-side magnet toward the CURRENT expected pickup using
    // true walkable-path distance (a euclidean point magnet parks the frontier against walls --
    // measured: best at 1.4 straight-line / 15.5 path from the target at a stage50 floor cancel).
    _choreoLegIntensity = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Choreo Leg Intensity");

    // Path-aware exit pull, active once all gold is banked (the nearest-gold family is zero
    // then by construction -- no sources): graph distance to "Exit Position".
    _exitMagnetIntensity = _gameConfigRemaining.contains("Exit Magnet Intensity") ? jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Exit Magnet Intensity") : 0.0f;
    if (_gameConfigRemaining.contains("Exit Position"))
    {
      const auto e = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Exit Position");
      if (e.size() != 2) JAFFAR_THROW_LOGIC("'Exit Position' must be an [x, y] pair");
      _exitX = e[0].get<uint8_t>();
      _exitY = e[1].get<uint8_t>();
    }

    // Full capture choreography: the ordered PICKUP positions of the reference ($C4 events --
    // note these include drop-site pickups of enemy-carried gold, NOT the grab/tile order).
    // A lineage's pickups must match this sequence position-by-position; the first mismatch
    // latches the violated flag and freezes further reference gold credit.
    for (const auto& e : jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Reference Pickup Order"))
    {
      if (e.size() != 2) JAFFAR_THROW_LOGIC("'Reference Pickup Order' entries must be [x, y] pairs");
      _refChestOrder.push_back({e[0].get<uint8_t>(), e[1].get<uint8_t>()});
    }

    // Small repulsion from ACTIVE enemies (normal or carrying): penalty per enemy inside the
    // radius, linear in closeness. Trapped (timer 1..125) and respawning (>=126) enemies are
    // exempt -- they are harmless and often useful (head-riding, interception choreography).
    _enemyRepulsionIntensity = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Enemy Repulsion Intensity");
    _enemyRepulsionRadius    = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Enemy Repulsion Radius");

    // Carried gold attracts at a fraction of a grounded chest's pull (effective distance is
    // divided-down by this weight). A courier parked in an unkillable posture is otherwise a
    // trap target: nearest by raw distance, yet uncapturable (hit on stage02).
    _carriedGoldWeight = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Carried Gold Weight");

    // Named tiles of the $0400 layout layer exposed as properties (e.g. a specific chest: value
    // 7 while present, 0 once collected) so rules can gate on individual chests / dug bricks.
    for (const auto& tileJs : jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Watch Tiles"))
    {
      watchTile_t t;
      t.name = jaffarCommon::json::getString(tileJs, "Name");
      t.x    = jaffarCommon::json::getNumber<size_t>(tileJs, "X");
      t.y    = jaffarCommon::json::getNumber<size_t>(tileJs, "Y");
      _watchTiles.push_back(t);
    }
  }

private:
  __INLINE__ void registerGameProperties() override
  {
    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("LRAM").pointer;

    // Cycle-phase digest (activates the emulator-side refresh only when hashed)
    if (_hashCyclePhase == true)
    {
      const auto p    = _emulator->getProperty("Cycle Phase");
      _cyclePhase     = p.pointer;
      _cyclePhaseSize = p.size;
    }

    // Global / game state
    registerGameProperty("Current Level", &_lowMem[0x00A6], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Play Mode", &_lowMem[0x00DB], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Graphics Mode", &_lowMem[0x0003], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Game Speed", &_lowMem[0x00E5], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Camera X", &_lowMem[0x0004], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Spawn Timer", &_lowMem[0x0053], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Global Timer", &_lowMem[0x009E], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Lives", &_lowMem[0x0098], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Player
    registerGameProperty("Player Tile X", &_lowMem[0x0020], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Tile Y", &_lowMem[0x0021], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Offset X", &_lowMem[0x0022], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Offset Y", &_lowMem[0x0023], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Movement-phase micro-state ($1D screen-y phase, $24 sub-phase): together with Offset Y these
    // pin the player's fall/run phase -- section hand-off win conditions gate on them so a section
    // boundary carries the reference's exact micro-state forward (phase leaks cost real frames).
    registerGameProperty("Player Phase 1D", &_lowMem[0x001D], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Phase 24", &_lowMem[0x0024], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Alive", &_lowMem[0x009A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Falling", &_lowMem[0x009B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Dig State", &_lowMem[0x00A0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player X", &_playerX, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Y", &_playerY, Property::datatype_t::dt_float32, Property::endianness_t::little);

    // Gold
    registerGameProperty("Gold Remaining", &_lowMem[0x0093], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Gold Collected", &_lowMem[0x00C4], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Dig Bonus Started", &_digBonusStarted, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Route Violated", &_routeViolated, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Route Progress", &_routeProgress, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Enemy Drop Done", &_enemyDropDone, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Gold At Watch Tile", &_goldAtWatch, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Guards buried by a refilling hole ($C5). A killed guard respawns at the top row at column
    // ($53+digCount)%32 -- the reference's endgame engineers a kill so the courier respawns next
    // to the last chest and fetches it; rules can reward that outcome via this property.
    registerGameProperty("Dig Chain Progress", &_digChainProgress, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Kill Count", &_lowMem[0x00C5], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Enemies (3 max). Timer is signed: negative = carrying gold countdown, 1..125 = pit-trapped,
    // 126/127 = respawning, 0 = normal.
    registerGameProperty("Enemy 1 Tile X", &_lowMem[0x0661], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 2 Tile X", &_lowMem[0x0662], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 3 Tile X", &_lowMem[0x0663], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 1 Tile Y", &_lowMem[0x0669], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 2 Tile Y", &_lowMem[0x066A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 3 Tile Y", &_lowMem[0x066B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy 1 Timer", &_lowMem[0x0671], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Enemy 2 Timer", &_lowMem[0x0672], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Enemy 3 Timer", &_lowMem[0x0673], Property::datatype_t::dt_int8, Property::endianness_t::little);

    // Config-declared watch tiles ($0400 layout layer values)
    for (const auto& t : _watchTiles) registerGameProperty(t.name, &_lowMem[0x0400 + t.y * 28 + t.x], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Irrecoverable single-hole trap: player inside a dug brick (layout brick, live empty),
    // solid floor beneath, both sides at player level solid (map edge = solid). No exit exists:
    // no climb (dug brick has no ladder), no walk (sides solid), no dig (needs an open side
    // cell), no enemy rescue (contact kills). The refill kills ~170 frames later -- rules can
    // fail these lineages at entry instead of letting them crowd the frontier while doomed.
    registerGameProperty("Player Trapped In Hole", &_playerTrappedInHole, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Choreo Matched", &_choreoMatched, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Choreo Violated", &_choreoViolated, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Search bookkeeping
    registerGameProperty("Last Input Step", &_lastInputStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Current Step", &_currentStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);

    // Whether a vertical input (U/D or a diagonal) can have any movement effect in this state:
    // the player overlaps or is adjacent to a ladder, or stands on/over one, or is on a rope.
    // Where this is 0, held verticals are movement no-ops that only perturb the cycle budget --
    // they create hash-identical twin states that diverge later (PROVEN by collision mining: all
    // causal hash collisions were held-direction twins). Input sets gate on this to never
    // generate the no-op inputs at all.
    registerGameProperty("Vertical Input Useful", &_verticalInputUseful, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Code-exact dig availability, replicated from the disassembled handlers ($CE90 dig-left,
    // $CF21 dig-right): the dig acts iff the player row is above the floor row, the target
    // column is in bounds, the diagonal-below tile is BRICK (1) on the live map, and the side
    // tile is EMPTY (0) on the live map. Everywhere else A/B are pure latch noise -- gating them
    // out is lossless by the game's own dispatcher.
    registerGameProperty("Can Dig Left", &_canDigLeft, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Can Dig Right", &_canDigRight, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Getting some properties' pointers now for quick access later
    _playerTileX   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Tile X")]->getPointer();
    _playerTileY   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Tile Y")]->getPointer();
    _playerOffsetX = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Offset X")]->getPointer();
    _playerOffsetY = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Offset Y")]->getPointer();
    _goldRemaining = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Gold Remaining")]->getPointer();

    // Getting index for a non input
    _nullInputIdx = _emulator->registerInput("|..|........|");
  }

  __INLINE__ InputSet::inputIndex_t getNullInputIndex() const override { return _nullInputIdx; }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Increasing counter if input is null
    if (input != _nullInputIdx) _lastInputStep = _currentStep;

    const uint8_t preGold = _lowMem[0x00C4];
    const uint8_t preTX = *_playerTileX, preTY = *_playerTileY;
    const int8_t  preET[3] = {(int8_t)_lowMem[0x0671], (int8_t)_lowMem[0x0672], (int8_t)_lowMem[0x0673]};

    // Running emulator
    _emulator->advanceState(input);

    // Choreography matching: on each pickup ($C4 increment), the player's tile must equal the
    // next expected reference pickup position; otherwise the violation latch sets and credit
    // freezes. Serialized + hashed lineage state (see serializeStateImpl / additional hashing).
    if (!_refChestOrder.empty() && _lowMem[0x00C4] > preGold && _choreoViolated == 0)
    {
      const uint8_t px = *_playerTileX, py = *_playerTileY;
      if (_choreoMatched < _refChestOrder.size() && px == _refChestOrder[_choreoMatched].first && py == _refChestOrder[_choreoMatched].second)
        _choreoMatched++;
      else
        _choreoViolated = 1;
    }

    // Route-waypoint odometer advance: pass waypoints strictly in order, gated by the section's
    // required banked-gold count -- no credit beyond a pickup boundary without the pickup.
    // A frame may cross several collapsed waypoints (falls). Lineage state -- serialized, hashed.
    if (_routeWps.empty() == false)
    {
      // Consume waypoints matching the PRE-advance tile first: without this, a lineage that
      // leaves its current waypoint's tile before the odometer consumed it (the start tile is
      // the guaranteed case; late gold gates are another) is falsely branded off-route.
      while (_routeProgress < _routeWps.size() && preTX == _routeWps[_routeProgress].first && preTY == _routeWps[_routeProgress].second && preGold >= _routeWpGold[_routeProgress])
        _routeProgress++;
      const auto prevProgress = _routeProgress;
      while (_routeProgress < _routeWps.size() && *_playerTileX == _routeWps[_routeProgress].first && *_playerTileY == _routeWps[_routeProgress].second &&
             _lowMem[0x00C4] >= _routeWpGold[_routeProgress])
        _routeProgress++;
      // High-water sub-tile proximity toward the CURRENT next waypoint (eighth-tiles, 0-7),
      // latched per waypoint so the smooth term is monotone along every lineage. Reset on
      // waypoint crossings (the +wpReward staircase step strictly exceeds the 7/8 ceiling).
      // Strict route conformance: any tile CHANGE that did not advance the odometer is
      // off-route (wrong tile, backward move, or order-skipping) -- latch a violation for the
      // fail rule. The search space collapses to timing variations along the exact tile path.
      if (_routeViolated == 0 && (*_playerTileX != preTX || *_playerTileY != preTY) && _routeProgress == prevProgress) _routeViolated = 1;
      if (_routeProgress != prevProgress) _routeSubBest = 0;
      if (_routeProgress < _routeWps.size() && _lowMem[0x00C4] >= _routeWpGold[_routeProgress])
      {
        const float px = (float)*_playerTileX + (float)_lowMem[0x0022] / 8.0f;
        const float py = (float)*_playerTileY + (float)_lowMem[0x0023] / 8.0f;
        const float d  = std::abs((float)_routeWps[_routeProgress].first - px) + std::abs((float)_routeWps[_routeProgress].second - py);
        if (d < 1.0f)
        {
          const uint8_t sub = (uint8_t)((1.0f - d) * 8.0f + 0.01f);
          if (sub > _routeSubBest) _routeSubBest = sub > 7 ? 7 : sub;
        }
      }
    }

    // Dig-bonus latch: the dig registry ($6A0/$6C0 slot arrays) showing a hole at the bonus
    // cell means the target brick has been dug; the latch survives the hole refilling.
    // Slot coords persist after refill (refill counter 0), so an ACTIVE counter is required.
    if (_digBonusReward != 0.0f && _digBonusDone == 0)
      for (size_t i = 0; i < 8; i++)
        if (_lowMem[0x06A0 + i] == _digBonusCellX && _lowMem[0x06C0 + i] == _digBonusCellY && _lowMem[0x06E0 + i] != 0)
        {
          _digBonusDone = 1;
          break;
        }

    // Dig-chain latch: advance while the NEXT chain cell has an active registry hole and the
    // gold gate is met. Ordered by construction: cell k+1 is only examined after cell k latched.
    // The in-progress ramp is a HIGH-WATER latch on the dig animation counter: the animation ends
    // a frame or two before the registry hole appears, so a live-value ramp would dip right before
    // the completion latch pays (non-monotone along the reference). Reset when the cell completes.
    if (_digChainReward != 0.0f && _digChainProgress < _digChainCells.size() && _lowMem[0x00C4] >= _digChainGoldGate)
    {
      const uint8_t digProg = _lowMem[0x00A0];
      if (digProg >= 1 && digProg <= 11)
      {
        const uint8_t tx = ((_lowMem[0x0702] & 0x40) != 0) ? (uint8_t)(*_playerTileX + 1) : (uint8_t)(*_playerTileX - 1);
        if (tx == _digChainCells[_digChainProgress].first && (uint8_t)(*_playerTileY + 1) == _digChainCells[_digChainProgress].second && digProg > _digChainRampProg)
          _digChainRampProg = digProg;
      }
      for (size_t s = 0; s < 8; s++)
        if (_lowMem[0x06A0 + s] == _digChainCells[_digChainProgress].first && _lowMem[0x06C0 + s] == _digChainCells[_digChainProgress].second && _lowMem[0x06E0 + s] != 0)
        {
          _digChainProgress++;
          _digChainRampProg = 0;
          break;
        }
    }

    // Gold-deposit latch: once a grounded gold has appeared at the watch tile, the deposit has
    // happened -- pay "Gold Deposit Reward" permanently (survives the gold's later pickup).
    if (_enemyDropWatchX != 255 && _goldDeposited == 0 && _lowMem[0x0400 + (size_t)_enemyDropWatchY * 28 + _enemyDropWatchX] == 0x07) _goldDeposited = 1;

    // Enemy-drop watch: latch when any enemy transitions from carrying (negative timer) to not,
    // standing at the watched tile -- "picked up and relinquished a gold at exactly this tile".
    if (_enemyDropWatchX != 255 && _enemyDropDone == 0)
      for (size_t i = 0; i < 3; i++)
        if (preET[i] < 0 && (int8_t)_lowMem[0x0671 + i] >= 0 && _lowMem[0x0661 + i] == _enemyDropWatchX && _lowMem[0x0669 + i] == _enemyDropWatchY)
        {
          _enemyDropDone = 1;
          break;
        }

    // Bury latch: a carrying enemy (negative timer) standing in ANY active dig-registry hole =
    // buried with its load (cell-agnostic: the surrender mechanics don't care where).
    if (_buryBonusReward != 0.0f && _buryBonusDone == 0)
      for (size_t i = 0; i < 3 && _buryBonusDone == 0; i++)
      {
        if ((int8_t)_lowMem[0x0671 + i] >= 0) continue;
        for (size_t s = 0; s < 8; s++)
          if (_lowMem[0x06A0 + s] == _lowMem[0x0661 + i] && _lowMem[0x06C0 + s] == _lowMem[0x0669 + i] && _lowMem[0x06E0 + s] != 0)
          {
            _buryBonusDone = 1;
            break;
          }
      }

    // Advancing current step
    _currentStep++;
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    // Input latch causal bits of $06: A/B (bit7/bit6 -- digs are press-edge-triggered), U/D
    // (bit3/bit2 -- buffered vertical press at ladder approaches), and L/R (bit1/bit0 --
    // momentum: a held direction vs coasting share RAM for a frame but diverge after; the
    // R-vs-null twin at the dig-initiation window was the depth-66 extinction). All collision-
    // mined as causal. With the gated 11/5/1 alphabet the added collision surface is small --
    // measured, not assumed (the old full-byte explosion was under the un-gated 15-input set).
    const uint8_t inputLatch = _lowMem[0x0006] & 0xCF;
    // Player facing ($0702 bit 6): dig direction + post-coast semantics -- unhashed until the
    // stage50 K=150 cross-parent collision (follower tied the floor exactly, then vanished into
    // a hash-twin whose future diverged at a fall-commit).
    hashEngine.Update((uint8_t)(_lowMem[0x0702] & 0x40));
    // Extra LRAM ranges enter at FULL value: the current use is the $A0-$A7 dig-work window --
    // bounded local dig timers/coordinates, only nonzero during digs. Quantizing them mod-4
    // merged same-step lineages whose digs started ~8 frames apart (different completion
    // schedules -> different enemy-freeing futures), which killed the T-300 freeing dig.
    for (const auto& r : _hashLramRanges)
      for (uint16_t a = r.first; a < r.second; a++) hashEngine.Update(_lowMem[a]);
    if (!_refChestOrder.empty())
    {
      hashEngine.Update(_choreoMatched);
      hashEngine.Update(_choreoViolated);
    }
    if (_digBonusReward != 0.0f) hashEngine.Update(_digBonusDone);
    if (_digChainReward != 0.0f)
    {
      hashEngine.Update(_digChainProgress);
      hashEngine.Update(_digChainRampProg);
    }
    if (_buryBonusReward != 0.0f) hashEngine.Update(_buryBonusDone);
    if (_routeWps.empty() == false)
    {
      hashEngine.Update(_routeProgress);
      hashEngine.Update(_routeSubBest);
      hashEngine.Update(_routeViolated);
    }
    if (_enemyDropWatchX != 255)
    {
      hashEngine.Update(_enemyDropDone);
      hashEngine.Update(_goldDeposited);
    }
    if (_hashInputLatch == true) hashEngine.Update(inputLatch);

    // Player: tile pos (+ optional sub-offsets), alive/falling ($9A-$9B), dig state, gold, facing
    hashEngine.Update(&_lowMem[0x0020], 2);
    if (_hashPlayerOffsets == true) hashEngine.Update(&_lowMem[0x0022], 2);
    hashEngine.Update(&_lowMem[0x009A], 2);
    hashEngine.Update(_lowMem[0x00A0]);
    hashEngine.Update(_lowMem[0x0093]);
    hashEngine.Update(_lowMem[0x0702]);

    // Game state: level, play mode, graphics mode (scroll-in/appearing phases), game speed
    hashEngine.Update(_lowMem[0x00A6]);
    hashEngine.Update(_lowMem[0x00DB]);
    hashEngine.Update(_lowMem[0x0003]);
    hashEngine.Update(_lowMem[0x00E5]);
    if (_hashCamera == true) hashEngine.Update(_lowMem[0x0004]);
    // $9E (global frame counter) is deliberately NOT hashed: any frame-count divergence between
    // same-step states would give every lineage a distinct digest and kill dedup. $53 (spawn
    // timer, the enemy-AI luck source) enters mod-4 quantized (re-quantized 2026-08-09 after
    // the full-value experiment; cycle-phase hashing now carries the twin-separation load).
    if (_hashTimers == true) hashEngine.Update((uint8_t)(_lowMem[0x0053] & _timerHashMask));
    if (_hashSoundState == true)
    {
      hashEngine.Update(&_lowMem[0x00B7], 3);
      hashEngine.Update(&_lowMem[0x00D0], 16);
      hashEngine.Update(&_lowMem[0x01EA], 22); // sound/timing work area (twin-divergence shadow)
    }
    // Full-state digest hashing = perfect state identity: dedup merges only true byte-identical
    // duplicates (all emulator-internal phase included). Expensive in state diversity -- use for
    // funnel-bound segments/diagnostics, not full-level searches.
    if (_hashCyclePhase == true) hashEngine.Update(_cyclePhase, _cyclePhaseSize);
    if (_hashFullLram == true) hashEngine.Update(&_lowMem[0x0000], 0x0800);
    if (_hashPhaseWorkRam == true)
    {
      // Mod-4 quantized phase-work hashing (re-quantized 2026-08-09; cycle-phase hashing now
      // carries the twin-separation load and full values just multiply the frontier).
      hashEngine.Update((uint8_t)(_lowMem[0x0026] & _timerHashMask));
      hashEngine.Update((uint8_t)(_lowMem[0x0028] & _timerHashMask));
      hashEngine.Update((uint8_t)(_lowMem[0x003B] & _timerHashMask));
      for (size_t i = 0; i < 7; i++) hashEngine.Update((uint8_t)(_lowMem[0x0055 + i] & _timerHashMask)); // $55-$5B spawn-timer work area
      hashEngine.Update((uint8_t)(_lowMem[0x01F6] & _timerHashMask));
    }

    // Enemies: tile X/Y, timers as sign+mod-4 class bytes (re-quantized 2026-08-09), facing.
    hashEngine.Update(&_lowMem[0x0661], 3);
    hashEngine.Update(&_lowMem[0x0669], 3);
    for (size_t i = 0; i < 3; i++) hashEngine.Update((uint8_t)(_lowMem[0x0671 + i] & (0x80 | _timerHashMask)));
    hashEngine.Update(_lowMem[0x00C5]); // kill count: buried-guard respawn lineage
    if (_hashEnemyOffsets == true)
    {
      hashEngine.Update(&_lowMem[0x0679], 3);
      hashEngine.Update(&_lowMem[0x0681], 3);
    }
    hashEngine.Update(_lowMem[0x0722]);
    hashEngine.Update(_lowMem[0x0732]);
    hashEngine.Update(_lowMem[0x0742]);

    // Dig holes: 8 slots (X, Y, refill counter)
    hashEngine.Update(&_lowMem[0x06A0], 8);
    hashEngine.Update(&_lowMem[0x06C0], 8);
    hashEngine.Update(&_lowMem[0x06E0], 8);

    // Mutable tile maps (dug bricks, refills, exit ladder appearance): two 28-wide layers indexed
    // base + Y*28 + X for Y in [1,13] -- $0200 live/display layer, $0400 static layout layer
    // (gold chest = 0x07 there). Both end at $0587.
    hashEngine.Update(&_lowMem[0x0200], 0x0388);
  }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    // Position = tile + subOffset/8 (1/8-tile granularity), in tile units
    _playerX = (float)*_playerTileX + (float)*_playerOffsetX / 8.0f;
    _playerY = (float)*_playerTileY + (float)*_playerOffsetY / 8.0f;

    // Live presence of a grounded gold at the watch tile (layout layer 0x07) -- rule-visible.
    // Covers "the enemy has voluntarily deposited its gold at exactly this tile" win conditions.
    _goldAtWatch = (_enemyDropWatchX != 255 && _lowMem[0x0400 + (size_t)_enemyDropWatchY * 28 + _enemyDropWatchX] == 0x07) ? 1 : 0;

    // Rule-visible dig-bonus progress: 1 once the bonus-cell dig has STARTED (mid-dig at the
    // cell, or the completion latch) -- win conditions can gate on "dig underway at (x,y)".
    _digBonusStarted = _digBonusDone;
    if (_digBonusStarted == 0 && _digBonusReward != 0.0f)
    {
      const uint8_t prog = _lowMem[0x00A0];
      if (prog >= 1 && prog <= 11)
      {
        const uint8_t tx = ((_lowMem[0x0702] & 0x40) != 0) ? (uint8_t)(*_playerTileX + 1) : (uint8_t)(*_playerTileX - 1);
        if (tx == _digBonusCellX && (uint8_t)(*_playerTileY + 1) == _digBonusCellY) _digBonusStarted = 1;
      }
    }

    // Vertical-input usefulness (see registerGameProperties): on a ladder/rope, over a ladder
    // top, or approaching an adjacent ladder on the FACED side (sprite attr $0702 bit6 set =
    // facing right) -- the buffered-climb press. Side-adjacency WITHOUT facing re-opens the
    // no-op twin hole (collision-mined at depths 158-169); facing-gated adjacency validates
    // against every reference vertical press with zero violations.
    const int     px     = std::min((int)*_playerTileX, _mapW - 1);
    const int     py     = std::max(1, std::min((int)*_playerTileY, 13));
    bool          useful = false;
    const uint8_t t      = terrainAt(px, py);
    if (t == 3 || t == 4) useful = true;
    if (useful == false && py < 13 && terrainAt(px, py + 1) == 3) useful = true;
    const bool facingRight = (_lowMem[0x0702] & 0x40) != 0;
    if (useful == false && facingRight == false && px > 0 && terrainAt(px - 1, py) == 3) useful = true;
    if (useful == false && facingRight == true && px < _mapW - 1 && terrainAt(px + 1, py) == 3) useful = true;
    // Exit phase: the hidden exit ladder STREAMS into the map after the last bank, and the
    // optimal climb buffers its press a frame before the tile materializes -- tile reads are
    // unreliable in this window, so verticals are unconditionally allowed.
    if (*_goldRemaining == 0 || *_goldRemaining >= 250) useful = true;
    _verticalInputUseful = useful ? 1 : 0;

    // Dig availability (code-exact, live-map reads like the handlers: entity markers matter --
    // an enemy on the side tile blocks the dig exactly as in the game code)
    const int dpy = (int)*_playerTileY;
    _canDigLeft   = 0;
    _canDigRight  = 0;
    if (dpy < 13 && dpy >= 1)
    {
      const int dpx = (int)*_playerTileX;
      if (dpx > 0 && _lowMem[0x0200 + (dpy + 1) * 28 + (dpx - 1)] == 1 && _lowMem[0x0200 + dpy * 28 + (dpx - 1)] == 0) _canDigLeft = 1;
      if (dpx < 27 && _lowMem[0x0200 + (dpy + 1) * 28 + (dpx + 1)] == 1 && _lowMem[0x0200 + dpy * 28 + (dpx + 1)] == 0) _canDigRight = 1;
    }
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    // Resetting magnets ahead of rule re-evaluation
    _pointMagnet.intensity = 0.0;
    _pointMagnet.x         = 0.0;
    _pointMagnet.y         = 0.0;
    _goldMagnet            = 0.0;
    _nearestChestMagnet    = 0.0;
    _digProgressMagnet     = 0.0;
    _digProgressRadius     = 0.0;
    _stopProcessingReward  = false;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    // Updating distance to user-defined point
    if (_pointMagnetTileBasis == true)
    {
      // Tile-basis Manhattan: immune to sub-pixel wobble (fall arcs, ladder snaps), which leaks
      // into shaped rewards as non-monotone noise at tolerance-0 floors. Plateaus between tile
      // crossings are benign (dedup collapses same-tile dithering).
      _playerDistanceToPointX = std::abs((float)_pointMagnet.x - (float)*_playerTileX);
      _playerDistanceToPointY = std::abs((float)_pointMagnet.y - (float)*_playerTileY);
      _playerDistanceToPoint  = _playerDistanceToPointX + _playerDistanceToPointY;
    }
    else
    {
      _playerDistanceToPointX = std::abs((float)_pointMagnet.x - _playerX);
      _playerDistanceToPointY = std::abs((float)_pointMagnet.y - _playerY);
      _playerDistanceToPoint  = sqrtf(_playerDistanceToPointX * _playerDistanceToPointX + _playerDistanceToPointY * _playerDistanceToPointY);
    }

    // Weighted distance to the remaining gold -- ALL GOLD IS EQUAL (grounded chests and carried
    // gold are one source set). weights[0] x (player -> nearest gold), then the tour-lookahead
    // chain: weights[k] x (k-th greedy nearest-neighbor hop over the remaining golds).
    _nearestChestDistance = 0.0f;
    _goldChainTotal       = 0.0f;
    _chainHopCount        = 0;
    if (_nearestChestMagnet != 0.0f)
    {
      float best    = -1.0f;
      int   originG = -1;
      if (_nearestChestPathMode == true)
      {
        computeChestPathField();
        const int ptx = std::min((int)*_playerTileX, _mapW - 1);
        const int pty = std::max(1, std::min((int)*_playerTileY, 13));
        // Sub-tile smoothing: distance through own cell or any legally enterable neighbor,
        // each measured to that cell's center (field values are center-to-center tile steps).
        // Evaluated on both class fields; the carried field competes at _carriedGoldWeight x
        // its raw distance (weaker pull, same units).
        auto smoothed = [&](const float* field) -> std::pair<float, int>
        {
          float     best_    = field[pty * _mapW + ptx] + std::abs(_playerX - ((float)ptx + 0.5f)) + std::abs(_playerY - ((float)pty + 0.5f));
          int       cell_    = pty * _mapW + ptx;
          const int nb[4][2] = {{ptx - 1, pty}, {ptx + 1, pty}, {ptx, pty - 1}, {ptx, pty + 1}};
          for (size_t i = 0; i < 4; i++)
          {
            const int nx = nb[i][0], ny = nb[i][1];
            if (nx < 0 || nx >= _mapW || ny < 1 || ny > 13) continue;
            if (isSolidTile(terrainAt(nx, ny)) == true) continue;
            if (canMove(ptx, pty, nx, ny) == false) continue;
            const float cand = field[ny * _mapW + nx] + std::abs(_playerX - ((float)nx + 0.5f)) + std::abs(_playerY - ((float)ny + 0.5f));
            if (cand < best_)
            {
              best_ = cand;
              cell_ = ny * _mapW + nx;
            }
          }
          return {best_, cell_};
        };
        const auto [dG, cellG] = smoothed(_chestField);
        const auto [dC, cellC] = smoothed(_chestFieldC);
        const float effG       = dG;
        const float effC       = dC * _carriedGoldWeight;
        if (effG < _unreachable && (dC >= _unreachable || effG <= effC))
        {
          best    = effG;
          originG = (int)_chestFieldOrigin[cellG];
        }
        else if (dC < _unreachable)
        {
          best    = effC;
          originG = (int)_groundedSourceCount + (int)_chestFieldOriginC[cellC];
        }
      }
      if (best < 0.0f)
      {
        for (size_t y = 1; y <= 13; y++)
          for (size_t x = 0; x < 28; x++)
            if (_lowMem[0x0400 + y * 28 + x] == 0x07)
            {
              const float d = std::abs((float)x - _playerX) + std::abs((float)y - _playerY);
              if (best < 0.0f || d < best) best = d;
            }
        if (*_goldRemaining != 1)
          for (size_t i = 0; i < 3; i++)
            if ((int8_t)_lowMem[0x0671 + i] < 0)
            {
              const float d = _carriedGoldWeight * (std::abs((float)_lowMem[0x0661 + i] - _playerX) + std::abs((float)_lowMem[0x0669 + i] - _playerY));
              if (best < 0.0f || d < best) best = d;
            }
        if (best < 0.0f)
          for (size_t i = 0; i < 3; i++)
          {
            const int8_t t = (int8_t)_lowMem[0x0671 + i];
            if (t >= 1 && t <= 125)
            {
              const float d = _carriedGoldWeight * (std::abs((float)_lowMem[0x0661 + i] - _playerX) + std::abs((float)_lowMem[0x0669 + i] - _playerY));
              if (best < 0.0f || d < best) best = d;
            }
          }
      }
      if (best > 0.0f) _nearestChestDistance = best;
      _goldChainTotal = _lookaheadWeights[0] * _nearestChestDistance;

      // Greedy nearest-neighbor chain over the remaining golds, one weighted hop per extra
      // weight. Unreachable hops fall back to Manhattan so entombed gold keeps a gradient.
      if (_nearestChestPathMode == true && originG >= 0 && _lookaheadWeights.size() > 1)
      {
        bool used[_maxGold] = {};
        int  cur            = originG;
        used[cur]           = true;
        for (size_t k = 1; k < _lookaheadWeights.size() && _chainHopCount < _maxGold; k++)
        {
          int   nxt = -1;
          float bd  = 0.0f;
          for (size_t j = 0; j < _goldSourceCount; j++)
          {
            if (used[j] == true) continue;
            float d = _goldPairDist[cur][j];
            if (d >= _unreachable) d = std::abs((float)_goldSourceX[cur] - (float)_goldSourceX[j]) + std::abs((float)_goldSourceY[cur] - (float)_goldSourceY[j]);
            if (_goldSourceCarried[j] == true) d *= _carriedGoldWeight;
            if (nxt < 0 || d < bd)
            {
              nxt = (int)j;
              bd  = d;
            }
          }
          if (nxt < 0) break;
          _chainHopDist[_chainHopCount]   = bd;
          _chainHopWeight[_chainHopCount] = _lookaheadWeights[k];
          _chainHopCount++;
          _goldChainTotal += _lookaheadWeights[k] * bd;
          used[nxt] = true;
          cur       = nxt;
        }
      }
    }

    // Assemble the per-source reward contributions ONCE per state evaluation; both the reward
    // sum (calculateGameSpecificReward) and the log breakdown (printInfoImpl) read these members,
    // so the formulas exist in exactly one place.
    _bankedChests   = countBankedChests();
    _digProgression = (_digProgressMagnet != 0.0f) ? digProgressionSum() : 0.0f;
    _digKillBonus   = (_digProgressMagnet != 0.0f) ? digKillBonus() : 0.0f;

    // REFERENCE family: choreography-matched pickups (position-by-position against the
    // reference's $C4 event sequence, tracked in advanceStateImpl), paid at w_ref strength.
    // A violated lineage keeps what it earned but earns no more.
    _rewardGoldBankedRef = _referenceStrategyWeight * _goldMagnet * (float)_choreoMatched;

    // Surrendered chests: gold that left 'remaining' with NO player bank ($92) -- a trapped
    // carrier giving up its load. Free level progress (no delivery run, no kill needed); the
    // reference exploits it, so it pays at reference-family strength. With the exploratory
    // rem-progress term this totals a full chest credit; out-of-order player grabs still
    // earn only the exploratory fraction.
    const uint32_t playerBanked = _lowMem[0x00C4];
    _surrenderedChests          = (_bankedChests > playerBanked) ? (_bankedChests - playerBanked) : 0;
    _rewardGoldSurrender        = _referenceStrategyWeight * _goldMagnet * (float)_surrenderedChests;

    // One-off dig bonus: full value latches at completion (the dig registry only records
    // COMPLETED holes). While the bonus-cell dig is IN PROGRESS, a ramp pays half at dig start
    // growing toward full -- the incentive must lead the 33-frame dig, not lag it, or beam
    // retention drops the digging lineage before the latch ever pays. Target cell derived from
    // player tile + facing ($0702 bit6: set = right), matching the dispatcher's dig semantics.
    _rewardDigBonus = 0.0f;
    if (_digBonusReward != 0.0f)
    {
      if (_digBonusDone == 1)
        _rewardDigBonus = _digBonusReward;
      else
      {
        const uint8_t prog = _lowMem[0x00A0];
        if (prog >= 1 && prog <= 11)
        {
          const uint8_t px = *_playerTileX, py = *_playerTileY;
          const uint8_t tx = ((_lowMem[0x0702] & 0x40) != 0) ? (uint8_t)(px + 1) : (uint8_t)(px - 1);
          if (tx == _digBonusCellX && (uint8_t)(py + 1) == _digBonusCellY) _rewardDigBonus = 0.5f * _digBonusReward + (0.5f * _digBonusReward * (float)prog) / 12.0f;
        }
      }
    }
    _rewardBuryBonus = (_buryBonusDone == 1) ? _buryBonusReward : 0.0f;

    // Dig chain: each latched cell holds its full value permanently; the NEXT chain cell pays the
    // same in-progress ramp as the one-off dig bonus (half at dig start growing toward full) so the
    // incentive leads the 33-frame dig. Ramp only while the gold gate is met and the current dig's
    // target (player tile + facing) is exactly the next chain cell.
    _rewardDigChain = _digChainReward * (float)_digChainProgress;
    if (_digChainRampProg >= 1) _rewardDigChain += 0.5f * _digChainReward + (0.5f * _digChainReward * (float)_digChainRampProg) / 12.0f;

    // Route odometer reward: latched progress staircase + small next-waypoint pull (tile basis).
    _rewardRoute = 0.0f;
    if (_routeWps.empty() == false)
    {
      // Staircase + high-water sub-tile proximity (0..7 eighths toward the next waypoint,
      // latched in advanceStateImpl): smooth, section-gated, monotone along every lineage.
      _rewardRoute = _routeWpReward * ((float)_routeProgress + (float)_routeSubBest / 8.0f);
    }
    _rewardGoldDeposit = (_goldDeposited == 1) ? _goldDepositReward : 0.0f;
    {
    }

    // EXPLORATORY family at (1 - w_ref) strength
    _rewardGoldBanked  = (1.0f - _referenceStrategyWeight) * _goldMagnet * (float)_bankedChests;
    _rewardPointMagnet = -1.0f * _pointMagnet.intensity * _playerDistanceToPoint;
    _rewardTraceMagnet = 0.0f;
    if (_traceMagnetIntensity != 0.0f && !_tracePoints.empty())
    {
      const size_t idx   = std::min((size_t)_currentStep, _tracePoints.size() - 1);
      const float  d     = std::abs(_tracePoints[idx].first - _playerX) + std::abs(_tracePoints[idx].second - _playerY);
      _rewardTraceMagnet = -1.0f * _traceMagnetIntensity * d;
    }
    _rewardChoreoLeg = 0.0f;
    if (_choreoLegIntensity != 0.0f && !_refChestOrder.empty() && _choreoViolated == 0 && _choreoMatched < _refChestOrder.size())
    {
      const auto& tgt = _refChestOrder[_choreoMatched];
      if (memcmp(_choreoLegKey, &_lowMem[0x0200], 0x0388) != 0 || _choreoLegTarget != ((size_t)tgt.first | ((size_t)tgt.second << 8)))
      {
        uint8_t tx = tgt.first, ty = tgt.second;
        runReverseDijkstra(&tx, &ty, 1, _choreoLegField, nullptr, false);
        memcpy(_choreoLegKey, &_lowMem[0x0200], 0x0388);
        _choreoLegTarget = (size_t)tgt.first | ((size_t)tgt.second << 8);
      }
      const int ptx = std::min((int)*_playerTileX, _mapW - 1);
      const int pty = std::max(1, std::min((int)*_playerTileY, 13));
      float     d   = _choreoLegField[pty * _mapW + ptx];
      if (d >= _unreachable)
        d = std::abs((float)tgt.first - _playerX) + std::abs((float)tgt.second - _playerY);
      else
        d += std::abs(_playerX - ((float)ptx + 0.5f)) + std::abs(_playerY - ((float)pty + 0.5f));
      _rewardChoreoLeg = -1.0f * _choreoLegIntensity * d;
    }
    // Exit magnet: sole endgame distance gradient once all gold is banked ($93 == 0 in the
    // pre-underflow gap, >= 250 after the exit-phase underflow). Path-aware with Manhattan
    // fallback, cached per map like the choreo leg.
    _rewardExitMagnet = 0.0f;
    if (_exitMagnetIntensity != 0.0f && (_lowMem[0x0093] == 0 || _lowMem[0x0093] >= 250))
    {
      if (memcmp(_exitFieldKey, &_lowMem[0x0200], 0x0388) != 0)
      {
        uint8_t tx = _exitX, ty = _exitY;
        runReverseDijkstra(&tx, &ty, 1, _exitField, nullptr, false);
        memcpy(_exitFieldKey, &_lowMem[0x0200], 0x0388);
      }
      const int ptx = std::min((int)*_playerTileX, _mapW - 1);
      const int pty = std::max(1, std::min((int)*_playerTileY, 13));
      float     d   = _exitField[pty * _mapW + ptx];
      if (d >= _unreachable)
        d = std::abs((float)_exitX - _playerX) + std::abs((float)_exitY - _playerY);
      else
        d += std::abs(_playerX - ((float)ptx + 0.5f)) + std::abs(_playerY - ((float)pty + 0.5f));
      _rewardExitMagnet = -1.0f * _exitMagnetIntensity * d;
    }

    _rewardEnemyRepulsion = 0.0f;
    if (_enemyRepulsionIntensity != 0.0f)
      for (size_t i = 0; i < 3; i++)
      {
        const int8_t t = (int8_t)_lowMem[0x0671 + i];
        if (t >= 1) continue;
        const float d = std::abs((float)_lowMem[0x0661 + i] - _playerX) + std::abs((float)_lowMem[0x0669 + i] - _playerY);
        if (d < _enemyRepulsionRadius) _rewardEnemyRepulsion -= _enemyRepulsionIntensity * (_enemyRepulsionRadius - d);
      }
    _rewardNearestChest = -1.0f * (1.0f - _referenceStrategyWeight) * _nearestChestMagnet * _goldChainTotal;
    _rewardDigProgress  = _digProgressMagnet * (_digProgression + _digKillBonus);
    _rewardLastInput    = _lastInputStepReward * (float)_lastInputStep;
    {
      const int px = (int)*_playerTileX, py = (int)*_playerTileY;
      auto      live       = [&](const int x, const int y) -> uint8_t { return (x >= 0 && x < _mapW && y >= 1 && y <= 13) ? _lowMem[0x0200 + y * 28 + x] : 1; };
      auto      isSol      = [&](const uint8_t v) -> bool { return v == 1 || v == 2; };
      _playerTrappedInHole = 0;
      // PREDICTIVE doom check: no air control exists, so a fall's landing cell is determined
      // the moment the support is lost. Resolve the landing cell down the player's column
      // (through open holes -- staircase descents keep falling), and flag when it is an
      // ISOLATED dug hole (active refill slot, solid floor beneath, solid on both sides):
      // no climb, no walk, no dig, refill kills. Holes are read from the refill-slot registry
      // ($06A0/$06C0/$06E0) -- the live map shows entity markers (8/9), never the dug state.
      // Guard: an enemy in the fall column (or in the landing hole) is a legitimate out
      // (head-riding), so those states are not flagged.
      if (px >= 0 && px < _mapW && py >= 1 && py <= 13)
      {
        auto isHole = [&](const int x, const int y) -> bool
        {
          for (size_t j = 0; j < 8; j++)
            if (_lowMem[0x06E0 + j] != 0 && (int)_lowMem[0x06A0 + j] == x && (int)_lowMem[0x06C0 + j] == y) return true;
          return false;
        };
        auto effTile = [&](const int x, const int y) -> uint8_t
        {
          if (x < 0 || x >= _mapW || y < 1 || y > 13) return 1;
          if (isHole(x, y) == true) return 0;
          const uint8_t v = _lowMem[0x0200 + y * 28 + x];
          return (v == 8 || v == 9) ? _lowMem[0x0400 + y * 28 + x] : v;
        };
        auto supportedAt = [&](const int x, const int y) -> bool
        {
          const uint8_t t = effTile(x, y);
          if (t == 3 || t == 4) return true;
          if (y >= 13) return true;
          const uint8_t below = effTile(x, y + 1);
          return isSol(below) || below == 3;
        };
        int ly = py;
        while (ly < 13 && supportedAt(px, ly) == false) ly++;
        if (isHole(px, ly) == true && (ly >= 13 || isSol(effTile(px, ly + 1))) && isSol(effTile(px - 1, ly)) && isSol(effTile(px + 1, ly)))
        {
          bool enemyOut = false;
          for (size_t i = 0; i < 3; i++)
          {
            const int ex = (int)_lowMem[0x0661 + i], ey = (int)_lowMem[0x0669 + i];
            if (ex == px && ey >= py && ey <= ly) enemyOut = true;
          }
          if (enemyOut == false) _playerTrappedInHole = 1;
        }
      }
    }
    _groundedGold = 0;
    _carriedGold  = 0;
    for (size_t y = 1; y <= 13; y++)
      for (size_t x = 0; x < 28; x++)
        if (_lowMem[0x0400 + y * 28 + x] == 0x07) _groundedGold++;
    for (size_t i = 0; i < 3; i++)
      if ((int8_t)_lowMem[0x0671 + i] < 0) _carriedGold++;

    // Debug instrument for the trace monotonicity study: JAFFAR_DUMP_COMPONENTS=<file> appends
    // one component line per state evaluation (aligned with the floor-trace steps during the
    // init-time reference replay). Inert when the env var is unset.
    static FILE* compOut = []()
    {
      const char* p = getenv("JAFFAR_DUMP_COMPONENTS");
      return (p != nullptr) ? fopen(p, "w") : (FILE*)nullptr;
    }();
    if (compOut != nullptr)
    {
      fprintf(compOut, "%u\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%u\t%u\n", _bankedChests, _rewardGoldBanked, _nearestChestDistance, _goldChainTotal, _rewardNearestChest,
              _rewardPointMagnet, _digProgression, _digKillBonus, _groundedGold, _carriedGold);
      fflush(compOut);
    }
  }

  // Tile of the live $0200 layer, falling back to the $0400 layout layer where the player/enemy
  // display markers (9/8) hide the terrain beneath them
  __INLINE__ uint8_t terrainAt(const int x, const int y) const
  {
    const uint8_t v = _lowMem[0x0200 + y * 28 + x];
    return (v == 8 || v == 9) ? _lowMem[0x0400 + y * 28 + x] : v;
  }

  // Solid = brick (1) or stone (2). Trapdoors (5) read as non-solid: stepping on one falls
  // through, which is exactly their path semantics.
  static __INLINE__ bool isSolidTile(const uint8_t v) { return v == 1 || v == 2; }

  // Whether a cell supports the player: standing on/in a ladder or rope, or floored by
  // brick/stone/ladder-top (row 13 is the always-bricked bottom)
  __INLINE__ bool canStandAt(const int x, const int y) const
  {
    const uint8_t t = terrainAt(x, y);
    if (t == 3 || t == 4) return true;
    if (y >= 13) return true;
    const uint8_t below = terrainAt(x, y + 1);
    return isSolidTile(below) || below == 3;
  }

  // Whether movement rules allow a single-tile move (ux,uy) -> (vx,vy): unsupported cells only
  // fall; supported cells walk sideways, drop/climb down, and climb up when on a ladder
  __INLINE__ bool canMove(const int ux, const int uy, const int vx, const int vy) const
  {
    const int dx = vx - ux, dy = vy - uy;
    if (canStandAt(ux, uy) == false) return dx == 0 && dy == 1;
    if (dy == 0 && (dx == 1 || dx == -1)) return true;
    if (dx != 0) return false;
    if (dy == 1) return true;
    if (dy == -1) return terrainAt(ux, uy) == 3;
    return false;
  }

  // Layout-layer movement predicates: identical rules to canStandAt/canMove but reading the
  // static $0400 layout, so temporary live-map changes (open dug holes) do not perturb the
  // long-horizon tour estimates (the holes refill before those legs execute).
  __INLINE__ bool canStandAtLayout(const int x, const int y) const
  {
    const uint8_t t = _lowMem[0x0400 + y * 28 + x];
    if (t == 3 || t == 4) return true;
    if (y >= 13) return true;
    const uint8_t below = _lowMem[0x0400 + (y + 1) * 28 + x];
    return isSolidTile(below) || below == 3;
  }
  __INLINE__ bool canMoveLayout(const int ux, const int uy, const int vx, const int vy) const
  {
    const int dx = vx - ux, dy = vy - uy;
    if (canStandAtLayout(ux, uy) == false) return dx == 0 && dy == 1;
    if (dy == 0 && (dx == 1 || dx == -1)) return true;
    if (dx != 0) return false;
    if (dy == 1) return true;
    if (dy == -1) return _lowMem[0x0400 + uy * 28 + ux] == 3;
    return false;
  }

  __INLINE__ bool enemyAtTile(const int x, const int y) const
  {
    for (size_t i = 0; i < 3; i++)
      if ((int)_lowMem[0x0661 + i] == x && (int)_lowMem[0x0669 + i] == y) return true;
    return false;
  }

  // Whether any guard currently carries gold (negative timer)
  __INLINE__ bool anyCarrier() const
  {
    for (size_t i = 0; i < 3; i++)
      if ((int8_t)_lowMem[0x0671 + i] < 0) return true;
    return false;
  }

  // Reverse Dijkstra over the movement graph from the given source cells: fieldOut[cell] = true
  // traversal cost cell -> nearest source (respecting one-way falls). originOut (optional) gets
  // the winning source's index per cell.
  void runReverseDijkstra(const uint8_t* srcX, const uint8_t* srcY, const size_t nSrc, float* fieldOut, uint8_t* originOut, const bool useLayout = false)
  {
    for (int i = 0; i < _mapW * _mapH; i++) fieldOut[i] = _unreachable;
    if (originOut != nullptr)
      for (int i = 0; i < _mapW * _mapH; i++) originOut[i] = 255;
    auto& pq = _chestFieldQueue; // empty by invariant (fully drained below)
    for (size_t s = 0; s < nSrc; s++)
    {
      const int idx = (int)srcY[s] * _mapW + (int)srcX[s];
      if (fieldOut[idx] > 0.0f)
      {
        fieldOut[idx] = 0.0f;
        if (originOut != nullptr) originOut[idx] = (uint8_t)s;
        pq.push({0.0f, idx});
      }
    }
    while (pq.empty() == false)
    {
      const auto [d, vIdx] = pq.top();
      pq.pop();
      if (d > fieldOut[vIdx]) continue;
      const int vx = vIdx % _mapW, vy = vIdx / _mapW;
      // Cost of the forward step INTO v (edge u->v), optionally penalizing enemy-held tiles
      const float stepCost = 1.0f + ((_enemyPathCost != 0.0f && enemyAtTile(vx, vy) == true) ? _enemyPathCost : 0.0f);
      const int   nb[4][2] = {{vx - 1, vy}, {vx + 1, vy}, {vx, vy - 1}, {vx, vy + 1}};
      for (size_t i = 0; i < 4; i++)
      {
        const int ux = nb[i][0], uy = nb[i][1];
        if (ux < 0 || ux >= _mapW || uy < 1 || uy > 13) continue;
        if (isSolidTile(useLayout ? _lowMem[0x0400 + uy * 28 + ux] : terrainAt(ux, uy)) == true) continue;
        if (useLayout ? canMoveLayout(ux, uy, vx, vy) : canMove(ux, uy, vx, vy)) {}
        else
          continue;
        const float nd   = d + stepCost;
        const int   uIdx = uy * _mapW + ux;
        if (nd < fieldOut[uIdx])
        {
          fieldOut[uIdx] = nd;
          if (originOut != nullptr) originOut[uIdx] = originOut[vIdx];
          pq.push({nd, uIdx});
        }
      }
    }
  }

  // Gold field bundle -- ALL GOLD IS EQUAL: sources are grounded chests (0x07 in the $0400
  // layer) AND gold-carrying guards (a courier's tile is a gold source like any other). Computes
  // the multi-source nearest-gold field (with per-cell winning source), plus the source-to-source
  // path-distance matrix for the tour-lookahead chain. With no carrier everything depends only
  // on the tile map, so the one-entry cache stays ~always hot; while a carrier exists its
  // position joins the cache key (more misses, still cheap).
  void computeChestPathField()
  {
    uint8_t      key[16];
    MetroHash128 h;
    h.Update(&_lowMem[0x0200], 0x0388);
    const bool carriers = anyCarrier();
    if (carriers == true || _enemyPathCost != 0.0f)
    {
      h.Update(&_lowMem[0x0661], 3);
      h.Update(&_lowMem[0x0669], 3);
      h.Update(&_lowMem[0x0671], 3);
    }
    h.Finalize(key);
    if (_chestFieldValid == true && memcmp(key, _chestFieldKey, 16) == 0) return;

    // Gather the gold sources: grounded first, then carriers
    _goldSourceCount = 0;
    for (int y = 1; y <= 13 && _goldSourceCount < _maxGold; y++)
      for (int x = 0; x < _mapW && _goldSourceCount < _maxGold; x++)
        if (_lowMem[0x0400 + y * 28 + x] == 0x07)
        {
          _goldSourceX[_goldSourceCount] = (uint8_t)x;
          _goldSourceY[_goldSourceCount] = (uint8_t)y;
          _goldSourceCount++;
        }
    _groundedSourceCount = _goldSourceCount;

    // DELIVERY-PHASE EXCEPTION: with one chest left and a courier inbound, courier progress is
    // NOT player progress -- rewarding proximity to the moving carrier floods the frontier with
    // schedule-advanced lure lineages whose courier overshoots an unready reception (measured:
    // the +175 band gap that evicted the reference). The carrier leaves the source set; ranking
    // falls to the reception point-magnet and banked count. Grounded gold stays (dropped-chest
    // fallback).
    if (carriers == true && *_goldRemaining != 1)
      for (size_t i = 0; i < 3 && _goldSourceCount < _maxGold; i++)
        if ((int8_t)_lowMem[0x0671 + i] < 0)
        {
          _goldSourceX[_goldSourceCount] = (uint8_t)std::min((int)_lowMem[0x0661 + i], _mapW - 1);
          _goldSourceY[_goldSourceCount] = (uint8_t)std::max(1, std::min((int)_lowMem[0x0669 + i], 13));
          _goldSourceCount++;
        }

    // Release continuity: during the pit-fall/release transition a chest is briefly neither
    // carried nor grounded (a measured -143 trace cliff as the chain re-targets far gold).
    // Detect gold IN TRANSIT by counting: if grounded+carried sources fall short of the gold
    // remaining, pit-trapped enemies (timer 1..125) stand in as sources for the missing gold.
    size_t accounted = _goldSourceCount;
    for (size_t i = 0; i < 3; i++)
      if ((int8_t)_lowMem[0x0671 + i] < 0) accounted++;
    const uint8_t remNow = (*_goldRemaining > _initialGold) ? 0 : *_goldRemaining;
    if (accounted < (size_t)remNow)
      for (size_t i = 0; i < 3 && _goldSourceCount < _maxGold; i++)
      {
        const int8_t t = (int8_t)_lowMem[0x0671 + i];
        if (t >= 1 && t <= 125)
        {
          _goldSourceX[_goldSourceCount] = (uint8_t)std::min((int)_lowMem[0x0661 + i], _mapW - 1);
          _goldSourceY[_goldSourceCount] = (uint8_t)std::max(1, std::min((int)_lowMem[0x0669 + i], 13));
          _goldSourceCount++;
        }
      }

    // Class flags: sources gathered grounded-first, carried-class (couriers + transit
    // stand-ins) after; two separate fields so the carried pull can be weighted down.
    for (size_t i = 0; i < _goldSourceCount; i++) _goldSourceCarried[i] = (i >= _groundedSourceCount);
    runReverseDijkstra(_goldSourceX, _goldSourceY, _groundedSourceCount, _chestField, _chestFieldOrigin, true);
    runReverseDijkstra(&_goldSourceX[_groundedSourceCount], &_goldSourceY[_groundedSourceCount], _goldSourceCount - _groundedSourceCount, _chestFieldC, _chestFieldOriginC, true);

    // Source-to-source distance matrix for the chain hops: a single-source field from gold j
    // evaluated at gold i gives the walking cost i -> j (the graph is directed; falls one-way)
    if (_lookaheadWeights.size() > 1)
      for (size_t j = 0; j < _goldSourceCount; j++)
      {
        runReverseDijkstra(&_goldSourceX[j], &_goldSourceY[j], 1, _goldTmpField, nullptr, true);
        for (size_t i = 0; i < _goldSourceCount; i++) _goldPairDist[i][j] = _goldTmpField[(int)_goldSourceY[i] * _mapW + (int)_goldSourceX[i]];
      }

    memcpy(_chestFieldKey, key, 16);
    _chestFieldValid = true;
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.pushContiguous(&_lastInputStep, sizeof(_lastInputStep));
    serializer.pushContiguous(&_currentStep, sizeof(_currentStep));
    serializer.pushContiguous(&_choreoMatched, sizeof(_choreoMatched));
    serializer.pushContiguous(&_choreoViolated, sizeof(_choreoViolated));
    serializer.pushContiguous(&_digBonusDone, sizeof(_digBonusDone));
    serializer.pushContiguous(&_buryBonusDone, sizeof(_buryBonusDone));
    serializer.pushContiguous(&_routeProgress, sizeof(_routeProgress));
    serializer.pushContiguous(&_routeSubBest, sizeof(_routeSubBest));
    serializer.pushContiguous(&_routeViolated, sizeof(_routeViolated));
    serializer.pushContiguous(&_enemyDropDone, sizeof(_enemyDropDone));
    serializer.pushContiguous(&_goldDeposited, sizeof(_goldDeposited));
    serializer.pushContiguous(&_digChainProgress, sizeof(_digChainProgress));
    serializer.pushContiguous(&_digChainRampProg, sizeof(_digChainRampProg));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.popContiguous(&_lastInputStep, sizeof(_lastInputStep));
    deserializer.popContiguous(&_currentStep, sizeof(_currentStep));
    deserializer.popContiguous(&_choreoMatched, sizeof(_choreoMatched));
    deserializer.popContiguous(&_choreoViolated, sizeof(_choreoViolated));
    deserializer.popContiguous(&_digBonusDone, sizeof(_digBonusDone));
    deserializer.popContiguous(&_buryBonusDone, sizeof(_buryBonusDone));
    deserializer.popContiguous(&_routeProgress, sizeof(_routeProgress));
    deserializer.popContiguous(&_routeSubBest, sizeof(_routeSubBest));
    deserializer.popContiguous(&_routeViolated, sizeof(_routeViolated));
    deserializer.popContiguous(&_enemyDropDone, sizeof(_enemyDropDone));
    deserializer.popContiguous(&_goldDeposited, sizeof(_goldDeposited));
    deserializer.popContiguous(&_digChainProgress, sizeof(_digChainProgress));
    deserializer.popContiguous(&_digChainRampProg, sizeof(_digChainRampProg));
  }

  // Open dug holes earning the dig-progress credit: near a remaining on-map chest OR near a
  // gold-carrying guard (the mobile chest being trapped). Shared by the reward and the log.
  // An IN-PROGRESS dig ($A0 1-11) credits its target tile (facing-side diagonal-below -- the
  // dig sets facing to the dig side) the same way: without this, the 24 frozen dig frames are
  // a reward trough and dig lineages are the first eviction victims under DB pressure.
  __INLINE__ float digProgressionSum() const
  {
    // CONTINUOUS dig credit: each hole contributes its lifecycle progression -- from the first
    // dig action frame (animation stages ramp $A0 1..11) through the open-hole phase (refill
    // counter INIT~177 counting down to 0 at closure) -- summed over holes within the radius of
    // any remaining gold (grounded chest or carrying guard). Ranks trap MATURITY continuously:
    // an aging reception hole out-ranks a fresh or absent one at every step of the wait.
    // 182 = measured slot-spawn counter (171) + the animation-end credit (11): the anim->refill
    // handoff is seamless, so a hole's lifecycle credit is monotone from first dig frame to closure.
    constexpr float REFILL_INIT = 182.0f;
    auto            nearGold    = [&](const float hx, const float hy) -> bool
    {
      for (size_t y = 1; y <= 13; y++)
        for (size_t x = 0; x < 28; x++)
          if (_lowMem[0x0400 + y * 28 + x] == 0x07 && std::abs((float)x - hx) + std::abs((float)y - hy) <= _digProgressRadius) return true;
      for (size_t i = 0; i < 3; i++)
        if ((int8_t)_lowMem[0x0671 + i] < 0 && std::abs((float)_lowMem[0x0661 + i] - hx) + std::abs((float)_lowMem[0x0669 + i] - hy) <= _digProgressRadius) return true;
      return false;
    };
    // SUM aggregation (the empirically winning form -- the suspected hole-farming eviction was
    // in fact the dig-gated alphabet blocking the reference's own nudge presses). Each guard
    // kill ($C5) still pays a PERMANENT bonus of REFILL_INIT + 1 progression units so that
    // completing the trap always beats nursing an almost-closed hole.
    float sum = 0.0f;
    if (_lowMem[0x00A0] >= 1 && _lowMem[0x00A0] <= 11)
    {
      const int tx = (int)*_playerTileX + (((_lowMem[0x0702] & 0x40) != 0) ? 1 : -1);
      const int ty = (int)*_playerTileY + 1;
      if (tx >= 0 && tx < _mapW && ty >= 1 && ty <= 13 && nearGold((float)tx, (float)ty)) sum += (float)_lowMem[0x00A0];
    }
    else if (_lowMem[0x00A0] == 12)
    {
      // Registration gap: $A0=12 latches at animation end but the refill slot spawns 1-2 frames
      // later; without this the fresh hole is uncredited for those frames (measured -11 dent).
      // The gap is identified exactly: target tile dug in the live map (0 over a solid layout
      // tile) with no refill slot yet. Once the slot exists the open-hole term takes over.
      const int tx = (int)*_playerTileX + (((_lowMem[0x0702] & 0x40) != 0) ? 1 : -1);
      const int ty = (int)*_playerTileY + 1;
      if (tx >= 0 && tx < _mapW && ty >= 1 && ty <= 13 && _lowMem[0x0200 + ty * 28 + tx] == 0 && isSolidTile(_lowMem[0x0400 + ty * 28 + tx]) == true)
      {
        bool hasSlot = false;
        for (size_t j = 0; j < 8; j++)
          if (_lowMem[0x06E0 + j] != 0 && _lowMem[0x06A0 + j] == (uint8_t)tx && _lowMem[0x06C0 + j] == (uint8_t)ty) hasSlot = true;
        if (hasSlot == false && nearGold((float)tx, (float)ty)) sum += 11.0f;
      }
    }
    for (size_t j = 0; j < 8; j++)
      if (_lowMem[0x06E0 + j] != 0 && nearGold((float)_lowMem[0x06A0 + j], (float)_lowMem[0x06C0 + j])) sum += REFILL_INIT - (float)_lowMem[0x06E0 + j];
    return sum;
  }

  // Permanent per-kill credit (REFILL_INIT + 1 progression units each): completing the trap
  // always beats nursing an almost-closed hole. Itemized separately in the reward breakdown.
  __INLINE__ float digKillBonus() const { return 183.0f * (float)_lowMem[0x00C5]; }

  // Chests already banked, clamped over the exit-climb $93 underflow (see the gold magnet)
  __INLINE__ uint8_t countBankedChests() const { return *_goldRemaining > _initialGold ? _initialGold : (uint8_t)(_initialGold - *_goldRemaining); }

  // Sums the per-component contributions assembled in ruleUpdatePostHook. Component semantics:
  // gold banked (clamped over the exit-climb $93 underflow -- unclamped, winning lineages score
  // -24.9M and are evicted at the finish line), point magnet, nearest-gold pull (ALL gold is
  // equal: grounded chests and carried gold are one source set, measured by path distance), and
  // dig-progress credit (holes near any remaining gold -- the trap that converts a carry).
  __INLINE__ float calculateGameSpecificReward() const
  {
    // Reward for having made an input recently (for early termination)
    float reward = _rewardLastInput;

    // If this is a win state, then evaluate only w.r.t. how long since the last input
    if (_stopProcessingReward) return reward;

    reward += _rewardGoldBankedRef;
    reward += _rewardChoreoLeg;
    reward += _rewardTraceMagnet;
    reward += _rewardGoldSurrender;
    reward += _rewardDigBonus;
    reward += _rewardDigChain;
    reward += _rewardBuryBonus;
    reward += _rewardExitMagnet;
    reward += _rewardRoute;
    reward += _rewardGoldDeposit;
    reward += _rewardGoldBanked;
    reward += _rewardPointMagnet;
    reward += _rewardNearestChest;
    reward += _rewardDigProgress;
    reward += _rewardEnemyRepulsion;
    return reward;
  }

  void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Player Pos:                             (%5.3f, %5.3f) Alive: %u, Falling: %u, Dig: %u\n", _playerX, _playerY, _lowMem[0x009A],
                              _lowMem[0x009B], _lowMem[0x00A0]);
    jaffarCommon::logger::log("[J+]  + Gold Remaining / Collected:             %u / %u\n", _lowMem[0x0093], _lowMem[0x00C4]);
    jaffarCommon::logger::log("[J+]  + Spawn / Global Timer:                   %u / %u\n", _lowMem[0x0053], _lowMem[0x009E]);
    for (size_t i = 0; i < 3; i++)
      jaffarCommon::logger::log("[J+]  + Enemy %lu:                                (%2u, %2u) Timer: %d\n", i + 1, _lowMem[0x0661 + i], _lowMem[0x0669 + i],
                                (int8_t)_lowMem[0x0671 + i]);

    // Per-source reward breakdown: prints the SAME member values calculateGameSpecificReward sums
    jaffarCommon::logger::log("[J+]  + Reward Breakdown (w_ref %.2f / w_exp %.2f):\n", _referenceStrategyWeight, 1.0f - _referenceStrategyWeight);
    if (!_refChestOrder.empty() || std::abs(_pointMagnet.intensity) > 0.0f) jaffarCommon::logger::log("[J+]    + REFERENCE family:\n");
    if (!_refChestOrder.empty())
      jaffarCommon::logger::log("[J+]      + Choreo pickups:   %+.1f (%u/%lu matched in ref order%s [player picked %u] x %.0f x %.2f)\n", _rewardGoldBankedRef, _choreoMatched,
                                _refChestOrder.size(), _choreoViolated ? " VIOLATED" : "", _lowMem[0x00C4], _goldMagnet, _referenceStrategyWeight);
    if (_digChainReward != 0.0f)
      jaffarCommon::logger::log("[J+]      + Dig chain:        %+.1f (%u / %lu cells dug x %.0f, gold gate %u%s)\n", _rewardDigChain, _digChainProgress, _digChainCells.size(),
                                _digChainReward, _digChainGoldGate, (_rewardDigChain > _digChainReward * (float)_digChainProgress) ? ", digging..." : "");
    if (!_refChestOrder.empty() || _surrenderedChests > 0)
      if (_digBonusReward != 0.0f)
        jaffarCommon::logger::log("[J+]      + Dig bonus:        %+.1f (cell (%u,%u) %s)\n", _rewardDigBonus, _digBonusCellX, _digBonusCellY,
                                  _digBonusDone ? "DUG" : (_rewardDigBonus > 0.0f ? "digging..." : "not dug"));
    if (_buryBonusReward != 0.0f)
      jaffarCommon::logger::log("[J+]      + Bury bonus:       %+.1f (carrier in (%u,%u): %s)\n", _rewardBuryBonus, _digBonusCellX, _digBonusCellY,
                                _buryBonusDone ? "BURIED" : "not yet");
    jaffarCommon::logger::log("[J+]      + Surrendered gold: %+.1f (%u chests surrendered by trapped carriers x %.0f x %.2f)\n", _rewardGoldSurrender, _surrenderedChests,
                              _goldMagnet, _referenceStrategyWeight);
    if (std::abs(_pointMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]      + Leg magnet:       %+.1f (target (%.1f,%.1f), dist %.3f x %.0f)\n", _rewardPointMagnet, _pointMagnet.x, _pointMagnet.y,
                                _playerDistanceToPoint, _pointMagnet.intensity);
    if (_choreoLegIntensity != 0.0f && !_refChestOrder.empty())
      jaffarCommon::logger::log("[J+]      + Choreo path-leg:  %+.1f (to pickup %u, path-aware, x %.0f)\n", _rewardChoreoLeg, _choreoMatched + 1, _choreoLegIntensity);
    if (_traceMagnetIntensity != 0.0f && !_tracePoints.empty())
      jaffarCommon::logger::log("[J+]      + Trace magnet:     %+.1f (step-indexed reference position, x %.0f)\n", _rewardTraceMagnet, _traceMagnetIntensity);
    jaffarCommon::logger::log("[J+]    + EXPLORATORY family:\n");
    if (_routeWps.empty() == false)
      jaffarCommon::logger::log("[J+]      + Route odometer:   %+.1f (waypoint %u / %lu passed x %.0f, next-pull x %.0f)\n", _rewardRoute, _routeProgress, _routeWps.size(),
                                _routeWpReward, _routeNextPull);
    if (_goldDepositReward != 0.0f)
      jaffarCommon::logger::log("[J+]      + Gold deposit:     %+.1f (enemy deposit at (%u,%u): %s)\n", _rewardGoldDeposit, _enemyDropWatchX, _enemyDropWatchY,
                                _goldDeposited ? "DEPOSITED" : "not yet");
    if (_exitMagnetIntensity != 0.0f)
      jaffarCommon::logger::log("[J+]      + Exit magnet:      %+.1f (to (%u,%u), path-aware, x %.0f, UNWEIGHTED; %s)\n", _rewardExitMagnet, _exitX, _exitY, _exitMagnetIntensity,
                                _rewardExitMagnet != 0.0f ? "ACTIVE" : "inactive until all gold banked");
    jaffarCommon::logger::log("[J+]      + Gold banked:      %+.1f (%u banked x %.0f x %.2f)\n", _rewardGoldBanked, _bankedChests, _goldMagnet, 1.0f - _referenceStrategyWeight);
    if (std::abs(_nearestChestMagnet) > 0.0f)
    {
      char chain[96];
      int  p = 0;
      for (size_t c = 0; c < _chainHopCount && p < (int)sizeof(chain) - 20; c++) p += snprintf(chain + p, sizeof(chain) - p, " + %.2fx%.1f", _chainHopWeight[c], _chainHopDist[c]);
      jaffarCommon::logger::log("[J+]      + Nearest gold:     %+.1f (%s: %.2fx%.3f%s, x %.0f x %.2f; sources: %lu grounded + %lu carried)\n", _rewardNearestChest,
                                _nearestChestPathMode ? "path" : "manhattan", _lookaheadWeights[0], _nearestChestDistance, _chainHopCount > 0 ? chain : "", _nearestChestMagnet,
                                1.0f - _referenceStrategyWeight, _groundedGold, _carriedGold);
    }
    jaffarCommon::logger::log("[J+]    + NEUTRAL:\n");
    if (_enemyRepulsionIntensity != 0.0f)
      jaffarCommon::logger::log("[J+]      + Enemy repulsion:  %+.1f (active enemies within r=%.1f, x %.1f)\n", _rewardEnemyRepulsion, _enemyRepulsionRadius,
                                _enemyRepulsionIntensity);
    if (_digProgressMagnet != 0.0f)
      jaffarCommon::logger::log("[J+]    + Dig progress:      %+.1f (lifecycle sum %.1f + kill bonus %.0f [%u kill(s)], x %.2f, radius %.1f)\n", _rewardDigProgress,
                                _digProgression, _digKillBonus, _lowMem[0x00C5], _digProgressMagnet, _digProgressRadius);
    if (_lastInputStepReward != 0.0f) jaffarCommon::logger::log("[J+]    + Last-input term:   %+.1f (step %u x %.2f)\n", _rewardLastInput, _lastInputStep, _lastInputStepReward);
    if (_stopProcessingReward) jaffarCommon::logger::log("[J+]    + (reward processing stopped: terminal state, only last-input term applies)\n");
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;

    if (actionType == "Set Point Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto x         = jaffarCommon::json::getNumber<float>(actionJs, "X");
      auto y         = jaffarCommon::json::getNumber<float>(actionJs, "Y");
      rule.addAction([=, this]() { this->_pointMagnet = pointMagnet_t{.intensity = intensity, .x = x, .y = y}; });
      recognizedActionType = true;
    }

    if (actionType == "Set Gold Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_goldMagnet = intensity; });
      recognizedActionType = true;
    }

    if (actionType == "Set Nearest Chest Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_nearestChestMagnet = intensity; });
      recognizedActionType = true;
    }

    if (actionType == "Set Dig Progress Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto radius    = jaffarCommon::json::getNumber<float>(actionJs, "Radius");
      rule.addAction(
          [=, this]()
          {
            this->_digProgressMagnet = intensity;
            this->_digProgressRadius = radius;
          });
      recognizedActionType = true;
    }

    if (actionType == "Set Initial Gold")
    {
      auto value = jaffarCommon::json::getNumber<uint8_t>(actionJs, "Value");
      rule.addAction([=, this]() { this->_initialGold = value; });
      recognizedActionType = true;
    }

    if (actionType == "Stop Processing Reward")
    {
      rule.addAction([this]() { _stopProcessingReward = true; });
      recognizedActionType = true;
    }

    return recognizedActionType;
  }

  __INLINE__ jaffarCommon::hash::hash_t getStateInputHash() override
  {
    // There is no discriminating state element, so simply return a zero hash
    return jaffarCommon::hash::hash_t();
  }

  // Datatype to describe a point magnet
  struct pointMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    float x         = 0.0; // What is the x point of attraction
    float y         = 0.0; // What is the y point of attraction
  };

  // Magnets (used to determine state reward and have Jaffar favor a direction or action)
  pointMagnet_t _pointMagnet;
  float         _goldMagnet         = 0.0;
  float         _nearestChestMagnet = 0.0;
  float         _digProgressMagnet  = 0.0;
  float         _digProgressRadius  = 0.0;

  // Quick-access property pointers
  uint8_t* _playerTileX;
  uint8_t* _playerTileY;
  uint8_t* _playerOffsetX;
  uint8_t* _playerOffsetY;
  uint8_t* _goldRemaining;

  // Derived sub-tile player position (tile units)
  float _playerX = 0.0;
  float _playerY = 0.0;

  // Baseline gold count for the gold magnet (set per-level via the Set Initial Gold action)
  uint8_t _initialGold = 0;

  // Additions to make the last input as soon as possible
  uint16_t _lastInputStep = 0;
  uint16_t _currentStep   = 0;

  // Derived: whether vertical inputs can matter in this state (see stateUpdatePostHook)
  uint8_t _verticalInputUseful = 0;

  // Derived: code-exact dig availability (see stateUpdatePostHook)
  uint8_t _canDigLeft  = 0;
  uint8_t _canDigRight = 0;

  // Per-source reward contributions, assembled once per state evaluation in ruleUpdatePostHook
  // and read by both calculateGameSpecificReward (sum) and printInfoImpl (breakdown log)
  float                                    _rewardGoldBanked = 0.0f;
  float                                    _digBonusReward   = 0.0f;   ///< One-off reward for digging the "Dig Bonus Cell" (0 = disabled)
  uint8_t                                  _digBonusCellX    = 255;    ///< Target cell X of the one-off dig bonus
  uint8_t                                  _digBonusCellY    = 255;    ///< Target cell Y of the one-off dig bonus
  uint8_t                                  _digBonusDone     = 0;      ///< Latch: bonus cell has been dug (serialized + hashed)
  std::vector<std::pair<uint8_t, uint8_t>> _digChainCells;             ///< Ordered cells for the dig chain
  float                                    _digChainReward     = 0.0f; ///< Reward per chain cell dug in order (0 = disabled)
  uint8_t                                  _digChainGoldGate   = 0;    ///< Minimum banked gold before chain cells can latch
  uint8_t                                  _digChainProgress   = 0;    ///< Chain cells dug in order so far (serialized + hashed)
  float                                    _rewardDigChain     = 0.0f; ///< Current dig-chain reward contribution (latched + in-progress ramp)
  uint8_t                                  _digChainRampProg   = 0;    ///< High-water dig-animation counter for the current chain cell (serialized + hashed)
  uint8_t                                  _digBonusStarted    = 0;    ///< Derived (postHook): bonus-cell dig underway or done; rule-visible property
  float                                    _buryBonusReward    = 0.0f; ///< One-off reward for a carrying enemy buried in the bonus cell (0 = disabled)
  uint8_t                                  _buryBonusDone      = 0;    ///< Latch: carrier stood in the bonus hole (serialized + hashed)
  float                                    _rewardBuryBonus    = 0.0f; ///< Current bury-bonus reward contribution
  float                                    _rewardDigBonus     = 0.0f; ///< Current dig-bonus reward contribution
  float                                    _rewardPointMagnet  = 0.0f;
  float                                    _rewardNearestChest = 0.0f;
  float                                    _rewardDigProgress  = 0.0f;
  float                                    _rewardLastInput    = 0.0f;
  uint8_t                                  _bankedChests       = 0;
  float                                    _digProgression     = 0.0f;
  float                                    _digKillBonus       = 0.0f;
  size_t                                   _groundedGold       = 0;
  size_t                                   _carriedGold        = 0;

  // Game-Specific values
  float _nearestChestDistance = 0.0;
  float _playerDistanceToPointX;
  float _playerDistanceToPointY;
  float _playerDistanceToPoint;

  // Pointer to emulator's low memory storage
  uint8_t* _lowMem;

  // Reward for the last time an input was made (for early termination)
  float _lastInputStepReward;

  // Whether to hash the free-running spawn/global timers (the game's only randomness source)
  bool _hashTimers;

  // Whether to hash the camera X position (matters only during the level-start scroll-in)
  bool _hashCamera;

  // Whether to hash the RAM audio variables (audio phase alters the frame budget -- see ctor)
  bool _hashSoundState;

  // Whether to hash the emulator's between-frame execution context digest (see ctor)
  bool _hashCyclePhase;

  // Whether to hash the mined work-RAM phase bytes (see ctor)
  bool _hashPhaseWorkRam;

  // PROVISIONAL: whether to hash the entire 2KB LRAM (see ctor)
  bool _hashFullLram;
  bool _hashInputLatch = true;

  // Pointer/size of the emulator's "Cycle Phase" digest (valid when _hashCyclePhase)
  uint8_t* _cyclePhase     = nullptr;
  size_t   _cyclePhaseSize = 0;

  // Whether to hash the enemies' 1/8-tile sub-offsets (large state-count multiplier)
  bool _hashEnemyOffsets;

  // Whether to hash the player's 1/8-tile sub-offsets (movement-timing precision vs state count)
  bool _hashPlayerOffsets;

  // Nearest-chest distance semantics: false = Manhattan, true = movement-rule path search
  bool _nearestChestPathMode = false;

  // Extra path cost for entering an enemy-occupied tile in Path mode (0 = ignore enemies)
  float _enemyPathCost = 0.0f;

  // Path-mode distance field scratch (derived cache -- never serialized nor hashed)
  static constexpr int                       _mapW                             = 28;
  static constexpr int                       _mapH                             = 14; // rows 0..13; row 0 unused
  static constexpr float                     _unreachable                      = 1.0e9f;
  static constexpr size_t                    _maxGold                          = 8;
  float                                      _chestField[_mapW * _mapH]        = {};
  float                                      _chestFieldC[_mapW * _mapH]       = {};
  uint8_t                                    _chestFieldOriginC[_mapW * _mapH] = {};
  bool                                       _goldSourceCarried[_maxGold]      = {};
  size_t                                     _groundedSourceCount              = 0;
  float                                      _carriedGoldWeight                = 0.5f;
  float                                      _referenceStrategyWeight          = 0.0f;
  std::vector<std::pair<uint8_t, uint8_t>>   _refChestOrder;
  uint32_t                                   _orderedPrefix           = 0;
  uint8_t                                    _choreoMatched           = 0;
  uint8_t                                    _choreoViolated          = 0;
  uint8_t                                    _playerTrappedInHole     = 0;
  float                                      _rewardGoldBankedRef     = 0.0f;
  uint32_t                                   _surrenderedChests       = 0;
  float                                      _rewardGoldSurrender     = 0.0f;
  float                                      _enemyRepulsionIntensity = 0.0f;
  float                                      _enemyRepulsionRadius    = 0.0f;
  float                                      _rewardEnemyRepulsion    = 0.0f;
  std::vector<std::pair<uint16_t, uint16_t>> _hashLramRanges;
  float                                      _choreoLegIntensity   = 0.0f;
  float                                      _traceMagnetIntensity = 0.0f;
  std::vector<std::pair<float, float>>       _tracePoints;
  float                                      _rewardTraceMagnet             = 0.0f;
  float                                      _rewardChoreoLeg               = 0.0f;
  float                                      _choreoLegField[_mapW * _mapH] = {};
  uint8_t                                    _timerHashMask                 = 3;       ///< "Timer Hash Mask": quantization mask for timer-class hash bytes
  bool                                       _pointMagnetTileBasis          = false;   ///< "Point Magnet Tile Basis": tile-Manhattan point magnets (sub-pixel-wobble-free)
  std::vector<std::pair<uint8_t, uint8_t>>   _routeWps;                                ///< Ordered route waypoints ("Route Waypoints File")
  std::vector<uint8_t>                       _routeWpGold;                             ///< Required banked-gold count per waypoint (section gates)
  float                                      _routeWpReward                    = 0.0f; ///< Reward per waypoint passed ("Route Waypoint Reward")
  float                                      _routeNextPull                    = 0.0f; ///< Small tile pull toward the next waypoint ("Route Next Pull")
  uint16_t                                   _routeProgress                    = 0;    ///< Waypoints passed in order (lineage state: serialized + hashed)
  uint8_t                                    _routeSubBest                     = 0;    ///< High-water eighth-tile proximity toward the next waypoint (lineage state)
  uint8_t                                    _routeViolated                    = 0;    ///< Latch: player changed tile off the exact route order (lineage state; rule-visible)
  uint8_t                                    _enemyDropWatchX                  = 255;  ///< "Enemy Drop Watch" tile X (255 = off)
  uint8_t                                    _enemyDropWatchY                  = 255;  ///< "Enemy Drop Watch" tile Y
  uint8_t                                    _enemyDropDone                    = 0;    ///< Latch: an enemy relinquished its gold at the watched tile (lineage state)
  uint8_t                                    _goldAtWatch                      = 0;    ///< Derived (postHook): grounded gold currently present at the watch tile
  float                                      _goldDepositReward                = 0.0f; ///< "Gold Deposit Reward": one-off for the deposit having happened (0 = off)
  uint8_t                                    _goldDeposited                    = 0;    ///< Latch: a gold has appeared at the watch tile (lineage state)
  float                                      _rewardGoldDeposit                = 0.0f; ///< Current deposit reward contribution
  float                                      _rewardRoute                      = 0.0f; ///< Current route-odometer reward contribution
  float                                      _exitMagnetIntensity              = 0.0f; ///< Exit pull strength ("Exit Magnet Intensity", 0 = off)
  uint8_t                                    _exitX                            = 0;    ///< Exit cell X ("Exit Position")
  uint8_t                                    _exitY                            = 1;    ///< Exit cell Y ("Exit Position")
  float                                      _rewardExitMagnet                 = 0.0f; ///< Current exit-magnet contribution
  uint8_t                                    _exitFieldKey[0x0388]             = {};   ///< Map snapshot keying the cached exit field
  float                                      _exitField[_mapW * _mapH]         = {};   ///< Cached exit path field
  uint8_t                                    _choreoLegKey[0x0388]             = {};
  size_t                                     _choreoLegTarget                  = (size_t)-1;
  uint8_t                                    _chestFieldOrigin[_mapW * _mapH]  = {};
  float                                      _goldTmpField[_mapW * _mapH]      = {};
  bool                                       _chestFieldValid                  = false;
  uint8_t                                    _chestFieldKey[16]                = {};
  uint8_t                                    _goldSourceX[_maxGold]            = {};
  uint8_t                                    _goldSourceY[_maxGold]            = {};
  size_t                                     _goldSourceCount                  = 0;
  float                                      _goldPairDist[_maxGold][_maxGold] = {};
  std::vector<float>                         _lookaheadWeights;
  float                                      _goldChainTotal           = 0.0f;
  size_t                                     _chainHopCount            = 0;
  float                                      _chainHopDist[_maxGold]   = {};
  float                                      _chainHopWeight[_maxGold] = {};
  std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> _chestFieldQueue;

  // Datatype for a config-declared watched tile of the $0400 layout layer
  struct watchTile_t
  {
    std::string name;
    size_t      x;
    size_t      y;
  };

  // Config-declared watched tiles
  std::vector<watchTile_t> _watchTiles;

  // Specifies whether the reward should continue to be processed (for early termination)
  bool _stopProcessingReward;

  // Null input index to remember the last valid input
  InputSet::inputIndex_t _nullInputIdx;
};

} // namespace nes

} // namespace games

} // namespace jaffarPlus
