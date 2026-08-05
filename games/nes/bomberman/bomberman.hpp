#pragma once
#include <array>
#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>
namespace jaffarPlus
{
namespace games
{
namespace nes
{
class Bomberman final : public jaffarPlus::Game
{
  // Tile map geometry (verified by RE, see examples/nes/bomberman/MECHANICS.md):
  // 13 rows x 31 cols at $0200, 32-byte row stride (last byte padding).
  static constexpr uint16_t _mapBase   = 0x0200;
  static constexpr uint8_t  _mapRows   = 13;
  static constexpr uint8_t  _mapCols   = 31;
  static constexpr uint8_t  _mapStride = 32;
  // Tile codes (verified): 0 empty, 1 concrete, 2 brick, 3 bomb, 4 brick hiding exit,
  // 5 brick hiding power-up, 6 revealed power-up, 8 revealed exit, 0x11-0x14 explosion.
  static constexpr uint8_t _tileExitBrick    = 4;
  static constexpr uint8_t _tilePowerupBrick = 5;
  static constexpr uint8_t _tilePowerup      = 6;
  static constexpr uint8_t _tileExit         = 8;

public:
  static __INLINE__ std::string getName() { return "NES / Bomberman"; }
  Bomberman(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    // Risky-optimization switch: when true, the allowed-input set FORCES a bomb drop (A alone)
    // on the first input frame where a bomb slot is free -- placement timing becomes a fixed
    // cadence and the search only steers where the player stands at each drop.
    _forceImmediateBomb = false;
    if (_gameConfigRemaining.contains("Force Immediate Bomb Placement"))
      _forceImmediateBomb = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Force Immediate Bomb Placement");
    // Companion switch: when true (default), a ticking bomb (outside its cross) FORCES detonation --
    // the allowed set is cleared to move+B composites + plain B only. When false, B and the move+B
    // composites are offered as ADDITIONS alongside movement / A / null, so the search is free to
    // detonate now, later, or not at all (more exploration; can trim lag frames from ill-timed pops).
    _forceImmediateDetonation = true;
    if (_gameConfigRemaining.contains("Force Immediate Detonation")) _forceImmediateDetonation = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Force Immediate Detonation");
    // Per-brick reward for the any-brick ladder (every plain brick destroyed, independent of chains).
    // Default 10 (as tuned for stages 1-49); lower it (e.g. 0.1) on stages where brick-breaking should
    // barely register so the search focuses on kills/exit instead of chasing brick destruction.
    _anyBrickLadderReward = 10.0f;
    if (_gameConfigRemaining.contains("Any-Brick Ladder Reward")) _anyBrickLadderReward = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Any-Brick Ladder Reward");
    // Everything-goes mode: when true, every input frame offers the FULL unrestricted joypad set
    // (all directions, A, B, and every move/A/B composite) with NO parity, hazard, or detonation
    // gating. Massive branching -- intended only for short localized searches (e.g. an endgame tail).
    _disableInputRestrictions = false;
    if (_gameConfigRemaining.contains("Disable Input Restrictions")) _disableInputRestrictions = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Disable Input Restrictions");
    // Composite-direction alphabet (v2, 2026-08-04): diagonals (UL/UR/DL/DR) are NOT redundant --
    // within the +-3px cornering-assist window of a junction they stack the parallel handler's
    // pixel on top of the assist (3px/frame while misaligned; BFS proof 71->69 steps, see
    // MECHANICS.md). Opposing pairs: U+D (or L+R against a perpendicular push) moves 1px then
    // freezes the player -- a 1px-off-center hover null cannot hold; pure L+R cancels. Opt-in
    // because configs must also list these input strings in some (even never-satisfiable) input
    // set, or the runner's string map lacks them and printInfo/solution stringification crashes;
    // enabling it by default would break v1-era configs.
    _allowCompositeDirections = false;
    if (_gameConfigRemaining.contains("Allow Composite Directions")) _allowCompositeDirections = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Allow Composite Directions");
    // Covered-enemies-handled semantics (v2, 2026-08-04, user): an enemy inside a ticking bomb's
    // blast cross counts as HANDLED -- the covered-enemy credit (+1500) pays in fuse mode too
    // (not just Detonator mode), and the primary closest-enemy player magnet retargets to the
    // nearest UNcovered enemy so the player moves on to the next objective while the bomb does
    // its work. Opt-in: default false preserves every archived config's reward function.
    _coveredEnemiesHandled = false;
    if (_gameConfigRemaining.contains("Covered Enemies Handled")) _coveredEnemiesHandled = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Covered Enemies Handled");
    // Grab-time exit-chain replan (v2, 2026-08-04, user): the one-shot pu->exit plan (computed
    // on the VIRGIN board with the powerup tile as a route proxy) leaves stray chain cells once
    // the real solve diverges -- and at v2's 30k Brick Reward those strays become farmable
    // detours. With this knob, the exit chain is recomputed ONCE at powerup grab, sourced from
    // the player's ACTUAL position on the CURRENT board. That makes the plan lineage state:
    // it is then serialized and hashed (knob-gated so archived configs keep their exact state
    // and hash layouts). Between plan points the chain stays frozen (ladder stability).
    _replanChainsOnGrab = false;
    if (_gameConfigRemaining.contains("Replan Chains On Grab")) _replanChainsOnGrab = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Replan Chains On Grab");
    // Per-stage power-up type: the stage's single power-up is stage-fixed (1=Fire, 2=Bombs, ...).
    // "Powerup Stat": "Flame" (default, stage 1), "Bombs" (stage 2), "Detonator" (stage 3) or
    // "Speed" (stage 4; ROM $CF29: pickup does LDA #1 / STA $75) selects the grab signal.
    // NOTE: the detonator MACHINERY (B ladder, elapsed-hash gating, pending zeroing) keys on
    // _detonatorActive = ($77 != 0) computed per frame -- NOT on this knob -- because a deathless
    // run carries $77 into every later stage (only death strips it, ROM-verified stage-4 entry).
    _powerupStatBombs     = false;
    _powerupStatDetonator = false;
    _powerupStatSpeed     = false;
    _powerupStatBombpass  = false;
    _powerupStatWallpass  = false;
    _powerupStatFlamepass = false;
    if (_gameConfigRemaining.contains("Powerup Stat"))
    {
      const auto ps         = jaffarCommon::json::popString(_gameConfigRemaining, "Powerup Stat");
      _powerupStatBombs     = (ps == "Bombs");
      _powerupStatDetonator = (ps == "Detonator");
      _powerupStatSpeed     = (ps == "Speed");
      _powerupStatBombpass  = (ps == "Bombpass");
      _powerupStatWallpass  = (ps == "Wallpass");
      _powerupStatFlamepass = (ps == "Flamepass");
    }
    // Bombs is the only repeatable stage power-up ($74 accumulates, ROM cap 9): the grab signal
    // must clear the CARRIED count, e.g. stage 5 enters with $74=1 from stage 2 -> threshold 2.
    _powerupBombsThreshold = 1;
    if (_gameConfigRemaining.contains("Powerup Bombs Threshold")) _powerupBombsThreshold = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Powerup Bombs Threshold");
    // Flames is also repeatable ($73 += 0x10 per pickup, cap 0x50): threshold-style grab signal
    // like Bombs -- stage 7 enters with flame 2 (stage-1 pickup), so its grab target is 3.
    _powerupFlameThreshold = 2;
    if (_gameConfigRemaining.contains("Powerup Flame Threshold")) _powerupFlameThreshold = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Powerup Flame Threshold");
    // Bonus-stage mode: invincible kill-frenzy window; free A/B input policy + kill-count reward
    _bonusMode = false;
    if (_gameConfigRemaining.contains("Bonus Mode")) _bonusMode = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Bonus Mode");
    // Enemy record window: the object table (stride-10 arrays at $580/$58A/...) allocates enemy
    // records as a CONTIGUOUS block ENDING at index 9: start = 10 - rosterSize. Stages with <=7
    // enemies use 3..9 (default base 3); stages 11/12 spawn 8 -> base 2. Slots BELOW the block
    // keep STALE residue from a previous stage that used more slots (observed: two ghost bonus
    // Oneals at 0-1 on stage-11 entry, alive-looking st<32/x!=0) -- the window must exclude them.
    _enemyRecBase = 3;
    if (_gameConfigRemaining.contains("Enemy Record Base")) _enemyRecBase = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Enemy Record Base");
    _erN  = (uint8_t)(10 - _enemyRecBase);
    _erX  = (uint16_t)(0x0580 + _enemyRecBase);
    _erPX = (uint16_t)(0x058A + _enemyRecBase);
    _erY  = (uint16_t)(0x0594 + _enemyRecBase);
    _erPY = (uint16_t)(0x059E + _enemyRecBase);
    _erST = (uint16_t)(0x05A8 + _enemyRecBase);
    _erAI = (uint16_t)(0x05D0 + _enemyRecBase);
    // Skip-test support: compute/serve the exit chain regardless of the enemy counter
    _exitChainAlwaysActive = false;
    if (_gameConfigRemaining.contains("Exit Chain Always Active")) _exitChainAlwaysActive = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Exit Chain Always Active");
  }

private:
  __INLINE__ void registerGameProperties() override
  {
    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("LRAM").pointer;
    // Player position (verified by controlled-input RE): tile coords + pixel-in-tile (0-15,
    // tile center is 8). These bytes double as the engine's map-access scratch on frames where
    // bombs are placed/explode, so they can alias for a frame; searches should corroborate with
    // the hashed tile map when in doubt.
    _playerTileX  = (uint8_t*)registerGameProperty("Player Tile X", &_lowMem[0x0028], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _playerTileY  = (uint8_t*)registerGameProperty("Player Tile Y", &_lowMem[0x002A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _playerPixelX = (uint8_t*)registerGameProperty("Player Pixel X", &_lowMem[0x0029], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _playerPixelY = (uint8_t*)registerGameProperty("Player Pixel Y", &_lowMem[0x002B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // World-pixel position (derived: tile*16 + pixel), the natural magnet axes.
    registerGameProperty("Player Pos X", &_playerPosX, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Player Pos Y", &_playerPosY, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    _level = (uint8_t*)registerGameProperty("Level", &_lowMem[0x0058], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Gameplay Active", &_lowMem[0x000B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Kill Counter", &_lowMem[0x009E], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Stage Timer", &_lowMem[0x0093], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _lives        = (uint8_t*)registerGameProperty("Lives", &_lowMem[0x0068], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _bombRadius   = (uint8_t*)registerGameProperty("Bomb Radius", &_lowMem[0x0073], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _bombMax      = (uint8_t*)registerGameProperty("Bomb Max", &_lowMem[0x0074], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _hasDetonator = (uint8_t*)registerGameProperty("Has Detonator", &_lowMem[0x0077], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _timeLeft     = (uint8_t*)registerGameProperty("Time Left", &_lowMem[0x0093], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Enemies remaining in the stage (verified: decrements per kill, 0 = stage clear pending exit)
    _enemiesLeft = (uint8_t*)registerGameProperty("Enemies Left", &_lowMem[0x009C], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Table-derived alive count (robust cross-check against the game's $9C objective counter)
    registerGameProperty("Enemies Alive", &_enemiesAlive, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Stage-clear / death signals (verified on the QuickerNES casual playthrough + scripted death):
    // $5E (mirrored at $9F) flips 0->1 the moment the stage is cleared; $5C flips 0->1 the moment
    // the dying sequence starts (125+ frames before gameplay shuts down).
    _gameEndStatus = (uint8_t*)registerGameProperty("Game End Status", &_lowMem[0x005E], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _dyingFlag     = (uint8_t*)registerGameProperty("Dying", &_lowMem[0x005C], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Power-up stats (Flame Count = $73 high nybble; grabbing the stage's power-up bumps a stat
    // the same frame the map cell clears -- the game's own latched pickup ladder)
    registerGameProperty("Flame Count", &_flameCount, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Powerup Present", &_powerupPresent, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Powerup Progress", &_powerupProgress, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Enemy table (10-slot structure-of-arrays, decoded; see MECHANICS.md): tile X $584+i,
    // pixel X $58E+i, tile Y $598+i, pixel Y $5A2+i, state $5AC+i (<32 alive, 32 dying, 44 dead).
    // Derived aggregates for reward shaping:
    registerGameProperty("Closest Enemy Distance", &_closestEnemyDist, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Sum Enemy Distance", &_sumEnemyDist, Property::datatype_t::dt_float32, Property::endianness_t::little);
    // Brick-aware path distances (Dijkstra over the tile map; bricks cost +8 tiles): breaking a
    // brick that BLOCKS the route to a target drops these sharply, rewarding exactly the
    // consequential bricks. Units: world pixels.
    registerGameProperty("Path Distance To Enemy", &_pathDistEnemy, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Path Distance To Powerup", &_pathDistPowerup, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Path Distance To Exit", &_pathDistExit, Property::datatype_t::dt_float32, Property::endianness_t::little);
    // Global frame counter, +1 per frame during play. Without the Speed stat, inputs are IGNORED
    // entirely on frames where ($33 % 4) == 0 (movement, bomb drops, everything). The Speed power-up
    // ($75 != 0, stage 4, carried for the rest of a deathless run) BYPASSES that gate (ROM $CCA4:
    // LDA $75 / BNE process-input): input then processes EVERY frame -- that is what the skates'
    // speedup actually is. "Input Frame" tells whether the NEXT advance will process input --
    // configs use it to restrict non-input frames to the null input.
    _frameCounter = (uint8_t*)registerGameProperty("Frame Counter", &_lowMem[0x0033], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Input Frame", &_nextFrameAcceptsInput, Property::datatype_t::dt_bool, Property::endianness_t::little);
    // True while the stage-load screen-off window is active ($0C PPU_CTRL shadow == 0x10, the
    // board draw). Derived properties (powerup/exit/enemy scans) read a half-built board there;
    // configs whose seeds are cut at the stage arm must gate their Trigger Fail rules on
    // "Loading" == false or every step-1 child dies to garbage-property rules.
    registerGameProperty("Loading", &_isLoading, Property::datatype_t::dt_bool, Property::endianness_t::little);
    // RNG state RandomCtrl1-4 (layout/enemy manipulation lever; see MECHANICS.md)
    _rngState1 = (uint8_t*)registerGameProperty("RNG State 1", &_lowMem[0x0054], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _rngState2 = (uint8_t*)registerGameProperty("RNG State 2", &_lowMem[0x0055], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _rngState3 = (uint8_t*)registerGameProperty("RNG State 3", &_lowMem[0x0056], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _rngState4 = (uint8_t*)registerGameProperty("RNG State 4", &_lowMem[0x0057], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Map-derived (scanned each frame in stateUpdatePostHook; 255 = not present/unknown)
    registerGameProperty("Exit Tile X", &_exitTileX, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Exit Tile Y", &_exitTileY, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Exit Revealed", &_exitRevealed, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Caught In Blast", &_caughtInBlast, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("All Enemies Covered", &_allEnemiesCovered, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Blast Trapped", &_blastTrapped, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Powerup Obtained", &_powerupObtained, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Powerup Tile X", &_powerupTileX, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Powerup Tile Y", &_powerupTileY, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Bricks Left", &_bricksLeft, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Anti-exploit: true when a FRESHLY placed bomb (elapsed <= 2) sits on a live flame cell
    // ($42C/$47C/$4CC flame records). Only reachable via the flame-survival manipulation
    // (legit players die before their tile ever equals a flame cell); a config fail rule bans
    // it until Flamepass. Positive-controlled against the run7 exploit lineage.
    registerGameProperty("Bomb Placed In Flames", &_bombPlacedInFlames, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Current Step", &_currentStep, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    _nullInputIdx = _emulator->registerInput("|..|........|");
    _inputU       = _emulator->registerInput("|..|U.......|");
    _inputD       = _emulator->registerInput("|..|.D......|");
    _inputL       = _emulator->registerInput("|..|..L.....|");
    _inputR       = _emulator->registerInput("|..|...R....|");
    _inputA       = _emulator->registerInput("|..|.......A|");
    _inputB       = _emulator->registerInput("|..|......B.|");
    _inputUB      = _emulator->registerInput("|..|U.....B.|");
    _inputDB      = _emulator->registerInput("|..|.D....B.|");
    _inputLB      = _emulator->registerInput("|..|..L...B.|");
    _inputRB      = _emulator->registerInput("|..|...R..B.|");
    // Extra composites, used only by the "Disable Input Restrictions" (everything-goes) mode.
    _inputUA  = _emulator->registerInput("|..|U......A|");
    _inputDA  = _emulator->registerInput("|..|.D.....A|");
    _inputLA  = _emulator->registerInput("|..|..L....A|");
    _inputRA  = _emulator->registerInput("|..|...R...A|");
    _inputAB  = _emulator->registerInput("|..|......BA|");
    _inputUAB = _emulator->registerInput("|..|U.....BA|");
    _inputDAB = _emulator->registerInput("|..|.D....BA|");
    _inputLAB = _emulator->registerInput("|..|..L...BA|");
    _inputRAB = _emulator->registerInput("|..|...R..BA|");
    // Composite directions ("Allow Composite Directions", v2): diagonals for junction turning
    // (assist stacking), opposing pairs for the 1px-nudge-then-freeze hover. See MECHANICS.md.
    _inputUL = _emulator->registerInput("|..|U.L.....|");
    _inputUR = _emulator->registerInput("|..|U..R....|");
    _inputDL = _emulator->registerInput("|..|.DL.....|");
    _inputDR = _emulator->registerInput("|..|.D.R....|");
    _inputLR = _emulator->registerInput("|..|..LR....|");
    _inputUD = _emulator->registerInput("|..|UD......|");
    // Tracked player position: $20/$28/$29/$2A double as the engine's map-access scratch and can
    // alias to other entities' lookups (~0.5% of frames, e.g. enemy deaths). Continuity-filtered
    // shadow; feeds magnets, path smoothing and the corridor-parity input pruning. Hashed and
    // serialized (it is path-dependent state that affects the allowed-input set).
    _trackedTileX    = *_playerTileX;
    _trackedTileY    = *_playerTileY;
    _trackedPixX     = *_playerPixelX;
    _trackedPixY     = *_playerPixelY;
    _trackedMismatch = 0;
    registerGameProperty("Tracked Tile X", &_trackedTileX, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Tracked Tile Y", &_trackedTileY, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    stateUpdatePostHook();
  }
  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Running emulator
    _emulator->advanceState(input);
    // Advancing current step
    _currentStep++;
    _lastInput = input;
  }
  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    // Bottom-up causal allowlist: start EMPTY, add only known-consequential state. Excluded by
    // design: all global timers,
    // enemy pixel positions and AI countdowns, the score-popup object ($56C block). $25 = the
    // explosion/flame phase counter (period-quantized lethality clock) is hashed judiciously.
    // Deliberately excluded: global timers ($33 full frame counter, $93 stage timer -- only the
    // input-gate phase of $33 is causal), pad mirrors ($10-$17; A press-edge semantics unproven,
    // see MECHANICS.md open questions), $9F (mirror of $5E), $5E8 wave countdown (suspected anim
    // sync), sound/PPU queues, score, stack. Bomb 'elapsed' IS hashed: explosion timing is
    // gameplay, and it is what keeps bomb-wait lineages dedup-distinct.
    // Latched flags & stats: gameplay-active, level, dying, stage-clear, lives, flame/bomb/
    // detonator stats, enemies left
    // RNG $54-$57 IS hashed (user call, 2026-07-28): it advances only on enemy-AI decisions
    // (~1.4%/frame), never with raw time or player inputs, so it adds no churn-blowup -- and
    // merging RNG-variants silently swaps enemy futures (it broke reference-lineage
    // reproduction during the squeeze runs). Honest dedup at negligible cost.
    static constexpr uint16_t singles[] = {0x0B, 0x25, 0x54, 0x55, 0x56, 0x57, 0x58, 0x5C, 0x5E, 0x68, 0x73, 0x74, 0x75, 0x76, 0x77, 0x79, 0x7B, 0x9C, 0x9E};
    for (const auto a : singles) hashEngine.Update(_lowMem[a]);
    // Input-gate phase: the only causal content of the frame counter (movement/input on phase != 0)
    hashEngine.Update((uint8_t)(_lowMem[0x0033] & 3));
    // Stage-load latch window (2026-08-04): the pad mirrors are excluded from the hash in normal
    // play (the next poll overwrites them; hashing them would split identical-future states every
    // frame). But during the load stall ($0C PPU_CTRL shadow == 0x10, screen off) polls stop, and
    // the value latched by the start frame's poll IS causal: the in-flight stage-init iteration
    // processes it (movement banks +1/+2 px before the stall ends). Without this, a start-frame
    // input child and the null child hash identically for ~13 frames and dedup extinguishes the
    // latch lineage before its position diverges -- no search could ever exploit the start-frame
    // input.
    if (_lowMem[0x000C] == 0x10)
    {
      hashEngine.Update(_lowMem[0x0012]);
      hashEngine.Update(_lowMem[0x0013]);
    }
    // Player position (tile + pixel)
    hashEngine.Update(_lowMem[0x0028]);
    hashEngine.Update(_lowMem[0x0029]);
    hashEngine.Update(_lowMem[0x002A]);
    hashEngine.Update(_lowMem[0x002B]);
    // Tile map (bricks, bombs, exit/power-up cells)
    hashEngine.Update(&_lowMem[_mapBase], _mapRows * _mapStride);
    // Bomb table: active flags, tile X, tile Y ($3A0-$3BD); per-bomb elapsed at FULL resolution
    // (de-quantized: 8-frame buckets merged lineages whose bomb-detonation timing differed --
    // kill setups are frame-sensitive against moving enemies)
    hashEngine.Update(&_lowMem[0x03A0], 30);
    // Bomb elapsed timers: meaningless once the Detonator is held (bombs never auto-explode;
    // only B matters) -- hashing them full-res splits identical-future states every frame.
    // Keyed on $77 directly (hashed above, so hash-consistent): a deathless run carries the
    // Detonator into stages 4+, where the machinery must stay active from entry.
    if (_lowMem[0x0077] == 0) hashEngine.Update(&_lowMem[0x03D2], 10);
    // Explosion/effect object arrays (live flames are lethal beyond the 1-frame map transient)
    hashEngine.Update(&_lowMem[0x042C], 10);
    hashEngine.Update(&_lowMem[0x047C], 10);
    hashEngine.Update(&_lowMem[0x04CC], 10);
    hashEngine.Update(&_lowMem[0x051C], 10);
    // Enemy table: FULL positions (tile + pixel-in-tile) + state, AI timers + directions
    // ($5CA-$5DD). Pixel positions re-added: tile-level quantization merged states whose enemy
    // approach geometry differed (bomb-timing/kill setups depend on sub-tile enemy positions).
    hashEngine.Update(&_lowMem[_erX], _erN);
    hashEngine.Update(&_lowMem[_erPX], _erN);
    hashEngine.Update(&_lowMem[_erY], _erN);
    hashEngine.Update(&_lowMem[_erPY], _erN);
    hashEngine.Update(&_lowMem[_erST], _erN);
    hashEngine.Update(&_lowMem[0x05C6 + _enemyRecBase], 20);
    // Tracked player position (path-dependent input to the allowed-input pruning)
    hashEngine.Update(_trackedTileX);
    hashEngine.Update(_trackedTileY);
    hashEngine.Update(_trackedPixX);
    hashEngine.Update(_trackedPixY);
    hashEngine.Update(_trackedMismatch);
    hashEngine.Update(_bombPlacedInFlames);
    hashEngine.Update(_caughtInBlast);
    hashEngine.Update(_enemyPathBricksBroken);
    // Grab-time replanned exit chain is lineage state (feeds A-relevance input gating)
    if (_replanChainsOnGrab)
    {
      hashEngine.Update(_exitReplanned);
      hashEngine.Update(_exitChainLen);
      hashEngine.Update(_exitChainCells.data(), sizeof(uint16_t) * _exitChainCells.size());
    }
  }
  // Updating derivative values after updating the internal state
  // Updating derivative values after updating the internal state.
  // ORDER MATTERS: each block only consumes values computed above it in the SAME pass
  // (a previous ordering bug fed anticipatory gains with stale cross-lineage data).
  __INLINE__ void stateUpdatePostHook() override
  {
    // (1) Continuity-filtered player position. Alias guard: when the player makes no map access
    // (standing still, e.g. grab pause), the $28/$20 scratch holds the LAST entity that read the
    // map -- an enemy. A raw candidate sitting exactly on an alive enemy's tile is an alias
    // (player-on-enemy-tile means death anyway), so never accept or resync onto it.
    {
      bool alias = false;
      for (uint8_t e = 0; e < _erN; e++)
        if (_lowMem[_erST + e] < 32 && _lowMem[_erX + e] != 0 && *_playerTileX == _lowMem[_erX + e] && *_playerTileY == _lowMem[_erY + e]) alias = true;
      const int dx = std::abs((int)*_playerTileX - (int)_trackedTileX);
      const int dy = std::abs((int)*_playerTileY - (int)_trackedTileY);
      _isLoading   = (_lowMem[0x000C] == 0x10);
      // Stage-load snap (2026-08-04): during the board draw ($0C PPU_CTRL shadow = 0x10, screen
      // off) the raw spawn coordinates are authoritative and the previous stage's tracked
      // position is stale garbage. Without this, the continuity filter holds the stale position
      // through the load and the parity gate offers the WRONG directions on the first playable
      // frame (cost: the play-start input, measured 1 frame on the stage05 micro-search).
      if (_lowMem[0x000C] == 0x10)
      {
        _trackedTileX    = *_playerTileX;
        _trackedTileY    = *_playerTileY;
        _trackedPixX     = *_playerPixelX;
        _trackedPixY     = *_playerPixelY;
        _trackedMismatch = 0;
      }
      else if (alias) { /* hold last good tracked position; do not advance the mismatch counter */ }
      else if (dx + dy <= 1)
      {
        _trackedTileX    = *_playerTileX;
        _trackedTileY    = *_playerTileY;
        _trackedPixX     = *_playerPixelX;
        _trackedPixY     = *_playerPixelY;
        _trackedMismatch = 0;
      }
      else if (++_trackedMismatch >= 3)
      {
        _trackedTileX    = *_playerTileX;
        _trackedTileY    = *_playerTileY;
        _trackedPixX     = *_playerPixelX;
        _trackedPixY     = *_playerPixelY;
        _trackedMismatch = 0;
      }
    }
    _playerPosX = (uint16_t)_trackedTileX * 16 + _trackedPixX;
    _playerPosY = (uint16_t)_trackedTileY * 16 + _trackedPixY;

    // (2) Input-gate phase prediction. The Speed stat ($75) bypasses the phase gate entirely
    // (ROM $CCA4) -- with skates, every frame processes input.
    _nextFrameAcceptsInput = (_lowMem[0x0075] != 0) || (((*_frameCounter + 1) & 3) != 0);

    // (2b) Bomb-slot shadow: remember each ticking bomb's tile; after the slot frees at
    // detonation, keep the slot marked 'exploding' while a flame object still burns on that tile
    for (uint8_t i = 0; i < 10; i++)
    {
      if (_lowMem[0x03A0 + i] != 0)
      {
        _bombLastX[i]      = _lowMem[0x03AA + i];
        _bombLastY[i]      = _lowMem[0x03B4 + i];
        _bombShadow[i]     = 1;
        _bombWasTicking[i] = 1;
      }
      else if (_bombWasTicking[i] != 0)
      {
        // DETONATION FRAME. Pre-Flamepass, standing anywhere inside this bomb's blast cross at
        // this instant is a dead end: the game can be tricked into survival (fresh bombs shield
        // the flame ray) but the lineage is pinned into endless chain-bombing. The cross is
        // computed PIERCING other bombs (the shield IS the exploit), stopping at walls, and
        // including the first brick per ray.
        _bombWasTicking[i] = 0;
        const int r0       = (int)(_lowMem[0x0073] >> 4);
        const int bx = _bombLastX[i], by = _bombLastY[i];
        // PIXEL-accurate lethality: tile-granular matching mass-failed the OPTIMAL kill lines
        // (detonating while the player's tile is nominally in the cross but its pixels are
        // already clear -- the game's hitbox legitimately spares those; observed 6-7k fail
        // spikes per detonation step and a frozen 6/6 alive count). Fail only when the player's
        // center is within HITBOX px of a cross-tile center.
        constexpr int wpx    = 10;
        const int     pwx    = (int)*_playerTileX * 16 + (int)*_playerPixelX;
        const int     pwy    = (int)*_playerTileY * 16 + (int)*_playerPixelY;
        auto          lethal = [&](const int cc, const int rr) { return std::abs(pwx - (cc * 16 + 8)) < wpx && std::abs(pwy - (rr * 16 + 8)) < wpx; };
        if (lethal(bx, by) && _lowMem[0x0079] == 0) _caughtInBlast = true; // Flamepass: own flames harmless
        static constexpr int8_t dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dd : dirs4)
          for (int st = 1; st <= r0; st++)
          {
            const int cc = bx + dd[0] * st, rr = by + dd[1] * st;
            if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
            const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
            if (t == 1) break;
            if (lethal(cc, rr) && _lowMem[0x0079] == 0) _caughtInBlast = true;
            if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
            {
              // Route-brick LADDER: this blast destroys the brick; if it sits on the current
              // optimal enemy route, bank permanent credit. Monotone counter -> stable under
              // route re-targeting; only route bricks count -> not farmable. This is what makes
              // breaking DEEP (multi-brick) pocket walls beat the hold-annuity at every depth
              // (approach alone pays ~+16 for non-reconnecting intermediate bricks).
              if (_enemyPathBricks[rr * _mapCols + cc] != 0 && _enemyPathBricksBroken < 250) _enemyPathBricksBroken++;
              break;
            }
          }
      }
      if (_lowMem[0x03A0 + i] == 0 && _bombShadow[i] != 0)
      {
        bool burning = false;
        for (uint8_t j = 0; j < 10; j++)
          if (_lowMem[0x042C + j] != 0 && _lowMem[0x047C + j] == _bombLastX[i] && _lowMem[0x04CC + j] == _bombLastY[i]) burning = true;
        if (burning == false) _bombShadow[i] = 0;
      }
    }

    // (2c) Self-trap detector (Detonator only): with $77 held, bombs never auto-explode, so a
    // player whose ENTIRE reachable region (bricks/concrete/bombs as walls) lies inside ticking-
    // bomb crosses is a permanent zombie -- B is (correctly) never offered inside a cross, the
    // bomb never clears, and the lineage can neither progress nor die. Observed stage 6: bomb
    // sealed a 2-cell dead-end pocket, the zombie's chain-head proximity out-ranked every live
    // lineage for 2700+ steps and its wiggle-variants flooded the DB. Flag -> config Fail rule.
    _blastTrapped       = false;
    _coveredEnemies     = 0;
    _allEnemiesCovered  = false;
    _enemiesKilledCount = 0;
    for (uint8_t e = 0; e < _erN; e++)
      if (_lowMem[_erST + e] >= 32 && _lowMem[_erX + e] != 0) _enemiesKilledCount++;
    // Flamepass ($79) makes own flames harmless -- a "trapped in own blasts" state is NOT a
    // zombie (player walks out through the flames), so the self-trap fail must not fire.
    if ((_lowMem[0x0077] != 0 && _lowMem[0x0079] == 0) || _coveredEnemiesHandled)
    {
      bool anyTicking = false;
      for (uint8_t i = 0; i < 10 && anyTicking == false; i++)
        if (_lowMem[0x03A0 + i] != 0) anyTicking = true;
      if (anyTicking)
      {
        // Union of all ticking-bomb crosses (blast stops at concrete and at the first brick)
        bool      cross[13 * 31] = {};
        const int r0             = (int)_flameCount;
        for (uint8_t i = 0; i < 10; i++)
        {
          if (_lowMem[0x03A0 + i] == 0) continue;
          const int bx = _lowMem[0x03AA + i], by = _lowMem[0x03B4 + i];
          cross[by * _mapCols + bx]           = true;
          static constexpr int8_t dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& dd : dirs4)
            for (int st = 1; st <= r0; st++)
            {
              const int cc = bx + dd[0] * st, rr = by + dd[1] * st;
              if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
              const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
              if (t == 1) break;
              cross[rr * _mapCols + cc] = true;
              if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) break;
            }
        }
        // Covered-enemy credit (stage-6 right-pocket stall fix): with the pending class zeroed
        // post-detonator, a bomb covering an enemy paid NOTHING until the pop landed, so the
        // commit-and-escape lineage's intermediate frames ranked below the hover band and were
        // evicted mid-sequence (observed: 400+ step 2-alive stall on a legal ambush). Half a
        // kill (+1500) per alive, non-burning enemy inside a ticking cross; monotone into the
        // +3000 kill, pop forced-available outside the cross, credit drops if the enemy wanders
        // out -- no latch, no annuity.
        uint8_t aliveNonBurning = 0;
        for (uint8_t e = 0; e < _erN; e++)
        {
          const uint8_t st = _lowMem[_erST + e];
          if (st >= 32 || _lowMem[_erX + e] == 0) continue;
          const uint8_t ec = _lowMem[_erX + e], er = _lowMem[_erY + e];
          bool          burning = false;
          for (uint8_t fl = 0; fl < 10; fl++)
            if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == ec && _lowMem[0x04CC + fl] == er) burning = true;
          if (burning) continue;
          aliveNonBurning++;
          if (er < _mapRows && ec < _mapCols && cross[er * _mapCols + ec]) _coveredEnemies++;
        }
        // All remaining enemies inside ticking crosses = the whole roster is HANDLED (even though
        // a blast can still miss): the endgame walk may start now. Knob-gated like the rest of
        // the covered-handled semantics; exposed as property "All Enemies Covered" and folded
        // into exitPhase so the exit chain computes/pays while the bombs finish the job.
        if (_coveredEnemiesHandled && aliveNonBurning > 0 && _coveredEnemies >= aliveNonBurning) _allEnemiesCovered = true;
        const int pc = (int)*_playerTileX, pr = (int)*_playerTileY;
        if (_lowMem[0x0077] != 0 && _lowMem[0x0079] == 0 && pr >= 0 && pr < _mapRows && pc >= 0 && pc < _mapCols && cross[pr * _mapCols + pc])
        {
          // BFS over walkable cells (empty / revealed power-up / revealed exit; bombs solid)
          bool     seen[13 * 31] = {};
          uint16_t queue[13 * 31];
          int      qh = 0, qt = 0;
          queue[qt++]              = (uint16_t)(pr * _mapCols + pc);
          seen[pr * _mapCols + pc] = true;
          bool escape              = false;
          while (qh < qt && escape == false)
          {
            const int cur = queue[qh++], cc = cur % _mapCols, rr = cur / _mapCols;
            if (cross[cur] == false)
            {
              escape = true;
              break;
            }
            static constexpr int8_t dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& dd : dirs4)
            {
              const int nc = cc + dd[0], nr = rr + dd[1];
              if (nc < 0 || nc >= _mapCols || nr < 0 || nr >= _mapRows) continue;
              const int ni = nr * _mapCols + nc;
              if (seen[ni]) continue;
              const uint8_t t = _lowMem[_mapBase + nr * _mapStride + nc];
              // Bombpass ($78): bombs walkable; Wallpass ($76): bricks walkable -- neither can
              // seal a pocket anymore once held.
              const bool walkable = (t == 0 || t == _tilePowerup || t == _tileExit || (t == 3 && _lowMem[0x0078] != 0) ||
                                     ((t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) && _lowMem[0x0076] != 0));
              if (walkable == false) continue;
              seen[ni]    = true;
              queue[qt++] = (uint16_t)ni;
            }
          }
          if (escape == false) _blastTrapped = true;
        }
      }
    }

    // (3) Power-up stats
    _flameCount      = _lowMem[0x0073] >> 4;
    _powerupObtained = _powerupStatSpeed       ? (_lowMem[0x0075] != 0)
                       : _powerupStatDetonator ? (_lowMem[0x0077] != 0)
                       : _powerupStatBombpass  ? (_lowMem[0x0078] != 0)
                       : _powerupStatWallpass  ? (_lowMem[0x0076] != 0)
                       : _powerupStatFlamepass ? (_lowMem[0x0079] != 0)
                       : _powerupStatBombs     ? (_lowMem[0x0074] >= _powerupBombsThreshold)
                                               : (_flameCount >= _powerupFlameThreshold);
    _powerupProgress = (uint8_t)((_flameCount > 0 ? _flameCount - 1 : 0) + _lowMem[0x0074] + (_lowMem[0x0077] != 0 ? 1 : 0) + (_lowMem[0x0075] != 0 ? 1 : 0));

    // (4) Enemy proximity aggregates (Manhattan, alive only)
    _closestEnemyDist = 0.0f;
    _sumEnemyDist     = 0.0f;
    {
      float closest = 1e9f;
      for (uint8_t i = 0; i < _erN; i++)
      {
        const uint8_t st = _lowMem[_erST + i];
        if (st >= 32 || _lowMem[_erX + i] == 0) continue;
        const float ex = (float)_lowMem[_erX + i] * 16.0f + (float)_lowMem[_erPX + i];
        const float ey = (float)_lowMem[_erY + i] * 16.0f + (float)_lowMem[_erPY + i];
        const float dd = std::abs(ex - (float)_playerPosX) + std::abs(ey - (float)_playerPosY);
        _sumEnemyDist += dd;
        if (dd < closest) closest = dd;
      }
      if (closest < 1e9f) _closestEnemyDist = closest;
    }
    _enemiesAlive = 0;
    for (uint8_t i = 0; i < _erN; i++)
      if (_lowMem[_erST + i] < 32 && _lowMem[_erX + i] != 0) _enemiesAlive++;

    // (5) Map scan: exit, power-up, brick count
    _exitTileX = _exitTileY = _powerupTileX = _powerupTileY = 255;
    _exitRevealed                                           = false;
    _bricksLeft                                             = 0;
    for (uint8_t r = 0; r < _mapRows; r++)
      for (uint8_t c = 0; c < _mapCols; c++)
      {
        const uint8_t t = _lowMem[_mapBase + r * _mapStride + c];
        if (t == 2) _bricksLeft++;
        if (t == _tileExitBrick || t == _tileExit)
        {
          _exitTileX    = c;
          _exitTileY    = r;
          _exitRevealed = (t == _tileExit);
        }
        if (t == _tilePowerupBrick || t == _tilePowerup)
        {
          _powerupTileX = c;
          _powerupTileY = r;
        }
      }
    _powerupPresent = (_powerupTileX != 255);

    // (6) Brick-chain plan (once per stage) + current chain status
    // The load window ($0C == 0x10) resets the latch: a seed that replays through EARLIER stages
    // would otherwise latch the FIRST stage's plan and never recompute for the searched stage --
    // its powerup/exit cells aren't in the stale chain, so the chain reward aims at the previous
    // board's cells (and the map overlay shows 'U' instead of the chain-head 'H'). The reset also
    // fires on stage-card screen-offs (old board still in $0200); that intermediate plan is
    // discarded by the next load window, and the last computation always happens on the searched
    // stage's final board.
    if (_isLoading)
    {
      _chainComputed = false;
      _exitReplanned = false;
    }
    else if (_chainComputed == false && (_powerupTileX != 255 || _exitTileX != 255))
    {
      computeChainPlan();
      _initialBricks = _bricksLeft; // stage-constant baseline for the any-brick ladder
    }
    // Grab-time replan: once, at the frame the powerup lands, from the actual player position
    if (_replanChainsOnGrab && _exitReplanned == false && _powerupObtained && _exitTileX != 255 && _trackedTileX < _mapCols && _trackedTileY < _mapRows)
    {
      computeChainTo(_trackedTileX, _trackedTileY, _exitTileX, _exitTileY, _exitChainCells.data(), _exitChainLen);
      _exitReplanned = true;
    }
    _chainRemaining = 0;
    _chainCrumble   = 0.0f;
    _chainHeadX     = _powerupTileX;
    _chainHeadY     = _powerupTileY;
    if (_chainComputed)
      for (uint8_t i = 0; i < _chainLen; i++)
      {
        const uint8_t r = (uint8_t)(_chainCells[i] / _mapCols), c = (uint8_t)(_chainCells[i] % _mapCols);
        const uint8_t t = _lowMem[_mapBase + r * _mapStride + c];
        if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
        {
          if (_chainRemaining == 0)
          {
            _chainHeadX = c;
            _chainHeadY = r;
          }
          _chainRemaining++;
          // Brick already burning (flame object on its tile): crumbling is irreversible, pay
          // 5/6 of the brick reward at crumble start ramping to 6/6 at removal ($25 = flame
          // phase counter, ~8 ticks per burn).
          for (uint8_t fl = 0; fl < 10; fl++)
            if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == c && _lowMem[0x04CC + fl] == r)
            {
              _chainCrumble += (5.0f + std::min(1.0f, (float)_lowMem[0x0025] / 8.0f)) / 6.0f;
              break;
            }
        }
      }

    // Exit chain status (same logic as the power-up chain)
    _exitChainRemaining = 0;
    _exitChainCrumble   = 0.0f;
    _exitChainHeadX     = _exitTileX;
    _exitChainHeadY     = _exitTileY;
    if (_chainComputed)
      for (uint8_t i = 0; i < _exitChainLen; i++)
      {
        const uint8_t r = (uint8_t)(_exitChainCells[i] / _mapCols), c = (uint8_t)(_exitChainCells[i] % _mapCols);
        const uint8_t t = _lowMem[_mapBase + r * _mapStride + c];
        if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
        {
          if (_exitChainRemaining == 0)
          {
            _exitChainHeadX = c;
            _exitChainHeadY = r;
          }
          _exitChainRemaining++;
          for (uint8_t fl = 0; fl < 10; fl++)
            if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == c && _lowMem[0x04CC + fl] == r)
            {
              _exitChainCrumble += (5.0f + std::min(1.0f, (float)_lowMem[0x0025] / 8.0f)) / 6.0f;
              break;
            }
        }
      }

    // (7) Path-distance fields (brick-aware)
    // Phase-gated: each Dijkstra runs only when its phase is active. Gates derive from the
    // STATE (Flame Count / Enemies Left = the same conditions the reward rules use), NEVER from
    // magnet members (those hold the previous evaluated state's values -- stale-data hazard).
    const bool prePickupPhase = (_powerupObtained == false);
    const bool killPhase      = _powerupObtained && (_lowMem[0x009C] > 0);
    const bool exitPhase      = _powerupObtained && (_lowMem[0x009C] == 0 || _exitChainAlwaysActive || _allEnemiesCovered);
    _pathDistPowerup          = 0.0f;
    _pathDistExit             = 0.0f;
    _pathGainPowerup          = 0.0f;
    _pathGainExit             = 0.0f;
    _pathDistEnemy            = killPhase ? computePathDistance(true, 255, 255, false, false, _coveredEnemiesHandled) : 0.0f;
    // Enemy-path brick set (kill phase): bricks on the current optimal route toward the nearest
    // enemy. This is the dynamic 'enemy-reaching chain' -- expressed as a per-frame mark set so
    // all its rewards are instantaneous/event-anchored (no ladder to destabilize when the route
    // re-targets as enemies move). Feeds pending + crumble; the permanent payoff is implicit in
    // the 144-per-brick approach weight.
    for (auto& v : _enemyPathBricks) v = 0;
    _enemyPathCrumble = 0.0f;
    if (killPhase) markEnemyPathBricks();
    _chainHeadDist     = prePickupPhase ? computePathDistance(false, _chainHeadX, _chainHeadY) : 0.0f;
    _exitChainHeadDist = (exitPhase && _exitTileX != 255) ? computePathDistance(false, _exitChainHeadX, _exitChainHeadY) : 0.0f;

    // (8) Bomb pending totals + threat flags + fuse progress
    _pendingBricks = _pendingKills = _pendingHazard = 0.0f;
    _bombMaxProgress                                = 0.0f;
    _headFuseProgress                               = 0.0f;
    _bombEnemyDist                                  = 1e9f;
    for (auto& v : _threatOpen) v = 0;
    {
      const uint8_t radius = _flameCount;
      // At-will (Detonator) gating: pending is the value of a DETONATABLE setup. If the player
      // stands inside any ticking bomb's cross, pressing B is a doomed move (blast-trap) -- paying
      // pending there finances camping in the blast zone while every detonation child dies
      // (observed: bomb fence held forever, enemy pacing at the cross edge). Zero it until the
      // player is clear.
      bool atWillBlocked = false;
      if (_lowMem[0x0077] != 0)
      {
        const int prC = _trackedTileX, prR = _trackedTileY;
        for (uint8_t i = 0; i < 10 && atWillBlocked == false; i++)
        {
          if (_lowMem[0x03A0 + i] == 0) continue;
          const int bx = _lowMem[0x03AA + i], by = _lowMem[0x03B4 + i];
          if (bx == prC && by == prR) atWillBlocked = true;
          static constexpr int8_t dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& dd : dirs4)
            for (int st = 1; st <= (int)radius && atWillBlocked == false; st++)
            {
              const int cc = bx + dd[0] * st, rr = by + dd[1] * st;
              if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
              const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
              if (t == 1) break;
              if (cc == prC && rr == prR) atWillBlocked = true;
              if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) break;
            }
        }
      }
      for (uint8_t i = 0; i < 10; i++)
      {
        // A slot contributes while its bomb ticks AND while its flames still burn (shadow):
        // dropping the pending reward at the detonation frame creates a reward valley right
        // before the kill registers, and a byte-capped DB evicts the valley lineages.
        const bool ticking   = _lowMem[0x03A0 + i] != 0;
        const bool exploding = (ticking == false) && (_bombShadow[i] != 0);
        if (ticking == false && exploding == false) continue;
        // With the Detonator held, detonation is at-will (B): a ticking bomb is always fully
        // 'ripe' for pending/anticipation purposes regardless of its elapsed counter.
        const bool  atWill   = (_lowMem[0x0077] != 0);
        const float progress = ticking ? (atWill ? 1.0f : (float)_lowMem[0x03D2 + i] / 160.0f) : 1.0f;
        if (progress > _bombMaxProgress) _bombMaxProgress = progress;
        const uint8_t bx = ticking ? _lowMem[0x03AA + i] : _bombLastX[i];
        const uint8_t by = ticking ? _lowMem[0x03B4 + i] : _bombLastY[i];
        // Bomb-enemy proximity (2026-08-04, user): once the player closes in, what matters is a
        // PLANTED bomb near the enemy -- pendingKills only pays for enemies already inside the
        // cross (progress^2-weighted, ~zero early in the fuse), leaving no gradient between
        // "bomb near enemy" and "bomb far away". Track the best (closest) ticking-bomb-to-enemy
        // pixel distance; the reward term below turns it into a closeness bonus.
        if (ticking)
          for (uint8_t e = 0; e < _erN; e++)
          {
            const uint8_t st = _lowMem[_erST + e];
            if (st >= 32 || _lowMem[_erX + e] == 0) continue;
            const float ex = (float)_lowMem[_erX + e] * 16.0f + (float)_lowMem[_erPX + e];
            const float ey = (float)_lowMem[_erY + e] * 16.0f + (float)_lowMem[_erPY + e];
            const float d  = std::abs(ex - ((float)bx * 16.0f + 8.0f)) + std::abs(ey - ((float)by * 16.0f + 8.0f));
            if (d < _bombEnemyDist) _bombEnemyDist = d;
          }
        float bricks = 0.0f, kills = 0.0f, hazard = 0.0f;
        for (uint8_t e = 0; e < _erN; e++)
        {
          const uint8_t st = _lowMem[_erST + e];
          if (st >= 32 || _lowMem[_erX + e] == 0) continue;
          if (_lowMem[_erX + e] == bx && _lowMem[_erY + e] == by) kills += 1.0f;
        }
        static constexpr int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : dirs)
          for (uint8_t step = 1; step <= radius; step++)
          {
            const int cc = bx + dir[0] * step, rr = by + dir[1] * step;
            if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
            const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
            if (t == 1) break;
            if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
            {
              // Pending-brick pays ONLY for the current chain HEAD (phase-appropriate): threats
              // against random bricks are not progress. _threatOpen stays unconditional (it feeds
              // anticipation and committed-head detection, not reward).
              const bool isPuHead    = prePickupPhase && _chainComputed && _chainRemaining > 0 && cc == (int)_chainHeadX && rr == (int)_chainHeadY;
              const bool isExitHead  = exitPhase && _chainComputed && _exitChainRemaining > 0 && cc == (int)_exitChainHeadX && rr == (int)_exitChainHeadY;
              const bool isEnemyPath = _enemyPathBricks[rr * _mapCols + cc] != 0;
              if (isPuHead || isExitHead || isEnemyPath) bricks += 1.0f;
              // Head Fuse magnet: dedicated inverse-time-to-detonation gradient while a TICKING
              // bomb's blast covers the powerup-chain head. Fuse mode only -- in at-will
              // (Detonator) mode progress pins at 1.0 and this would become a farmable annuity
              // (the exact pathology the DETONATOR RESET removed).
              if (isPuHead && ticking && atWill == false && progress > _headFuseProgress) _headFuseProgress = progress;
              _threatOpen[rr * _mapCols + cc] = 1;
              break;
            }
            if (t == _tilePowerup || t == _tileExit)
            {
              hazard += 1.0f;
              break;
            }
            for (uint8_t e = 0; e < _erN; e++)
            {
              const uint8_t st = _lowMem[_erST + e];
              if (st >= 32 || _lowMem[_erX + e] == 0) continue;
              if (_lowMem[_erX + e] == cc && _lowMem[_erY + e] == rr) kills += 1.0f;
            }
          }
        // At-will (Detonator) mode: a held bomb's brick-pending is an ANNUITY (progress pinned
        // 1.0) -- at full weight (150) it beats the one-time payoff of actually breaking an
        // enemy-path wall (approach gain 144 at magnet 1.0), so the search holds bombs forever.
        // Halving it (75) puts every brick class strictly below its break payoff.
        // DETONATOR RESET: with at-will detonation there is no fuse latency to bridge, so the
        // entire pending (anticipation) class is obsolete -- and every at-will annuity variant
        // proved farmable (5 incidents). Events (kill ladder, crumbles, breaks) and distances
        // (approach pays ~+128 permanently per on-route brick broken) carry the incentive.
        if (atWill)
        {
          bricks = 0.0f;
          kills  = 0.0f;
          hazard = 0.0f;
        }
        if (atWillBlocked)
        {
          bricks = 0.0f;
          kills  = 0.0f;
        }
        _pendingBricks += progress * bricks * (atWill ? 0.5f : 1.0f);
        // Kills use progress^2: an enemy in the cross NOW only predicts a kill if detonation is
        // NEAR (enemies re-tile every ~20-30 frames vs the 160-frame fuse). Linear weighting
        // created a mirage -- lineages banked pending at placement time, hovered as best, and
        // the enemy walked out before the blast (measured: -485 collapse 8 frames pre-detonation,
        // detonation killing nothing). Bricks stay linear: they do not walk away.
        _pendingKills += progress * progress * kills;
        _pendingHazard += progress * hazard;
      }
    }

    // (9) Anticipatory gains (threatened bricks + bombs treated as clearing), clamped to dist
    _chainHeadGain     = 0.0f;
    _exitChainHeadGain = 0.0f;
    if (_bombMaxProgress > 0.0f)
    {
      if (prePickupPhase && _chainComputed && _chainRemaining > 0)
      {
        const float ant = computePathDistance(false, _chainHeadX, _chainHeadY, true);
        if (ant < _chainHeadDist) _chainHeadGain = std::min(_chainHeadDist - ant, _chainHeadDist);
      }
      if (exitPhase && _chainComputed && _exitChainRemaining > 0 && _exitTileX != 255)
      {
        const float ant = computePathDistance(false, _exitChainHeadX, _exitChainHeadY, true);
        if (ant < _exitChainHeadDist) _exitChainHeadGain = std::min(_exitChainHeadDist - ant, _exitChainHeadDist);
      }
    }

    // (9b) Next-brick pre-positioning: once the head is COMMITTED (a ticking bomb's blast line
    // reaches it, or it is already crumbling), a small PLAYER-sourced pull toward the next
    // remaining chain cell (or the prize tile itself when the head is last) lets the player use
    // the fuse to pre-position. Expressed as a closeness bonus so committing is never punished;
    // the fuse-weighted escape penalty keeps the approach outside the blast area.
    _chainNextDist          = -1.0f;
    _exitChainNextDist      = -1.0f;
    _enemyNextDist          = -1.0f;
    _chainHeadCommitted     = false;
    _exitChainHeadCommitted = false;
    bool anyBombTicking     = false;
    for (uint8_t i = 0; i < 10; i++)
      if (_lowMem[0x03A0 + i] != 0) anyBombTicking = true;
    _chainHeadThreat     = false;
    _exitChainHeadThreat = false;
    if (prePickupPhase && _chainComputed && _chainRemaining > 0)
    {
      const uint16_t headIdx = (uint16_t)(_chainHeadY * _mapCols + _chainHeadX);
      _chainHeadCommitted    = (_threatOpen[headIdx] != 0) || (_chainCrumble > 0.0f);
      // Head-threat credit flag (stage-6 stall fix): a ticking bomb whose cross covers the head,
      // BEFORE crumble starts. Paying half a brick reward here lets the productive placement
      // instantly out-rank same-band hover variants in an eviction-bound DB (observed: 2700-step
      // pin at cp level 2, 586k new states/step, head-break explorers evicted mid-sequence).
      // Threat->crumble->broken is reward-monotone: +3000 -> ~+5900 (crumble ladder) -> +6000
      // (latched), so no valley at any hand-off; farming it is dominated because the pop from
      // outside (forced by the connects ladder) upgrades the credit to the full latched break.
      _chainHeadThreat = (_threatOpen[headIdx] != 0) && (_chainCrumble == 0.0f);
      // Secondary pull while a bomb ticks but the head is NOT yet committed: the bomb owns the
      // primary approach, so weakly draw the PLAYER toward the current head meanwhile. The pull's
      // job is TRAVEL only: once the player is within placement-reach of the head, it pays ZERO --
      // otherwise camping beside the head with a junk bomb cycling farms the bonus and outranks
      // committing (whose crumble payoff is deferred a full fuse; observed on stage 2).
      // PLATEAU, not cliff: within placement-reach the pull pays its MAXIMUM (dist 0), so
      // approaching is monotone and committing strictly dominates (pending+crumble stack on
      // top). A zero-in-reach cutoff created a repulsive fence: camping just OUTSIDE reach
      // out-earned stepping in (observed +758/step hover one tile off the exit head).
      if (anyBombTicking && _chainHeadCommitted == false)
        _chainNextDist = bombAtPlayerReaches(_chainHeadX, _chainHeadY) ? 0.0f : computePathDistance(false, _chainHeadX, _chainHeadY, false, true);
      if (_chainHeadCommitted)
      {
        uint8_t nx = _powerupTileX, ny = _powerupTileY;
        bool    pastHead = false;
        for (uint8_t i = 0; i < _chainLen; i++)
        {
          const uint8_t r = (uint8_t)(_chainCells[i] / _mapCols), c = (uint8_t)(_chainCells[i] % _mapCols);
          const uint8_t t       = _lowMem[_mapBase + r * _mapStride + c];
          const bool    isBrick = (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick);
          if (c == _chainHeadX && r == _chainHeadY)
          {
            pastHead = true;
            continue;
          }
          if (pastHead && isBrick)
          {
            nx = c;
            ny = r;
            break;
          }
        }
        _chainNextDist = computePathDistance(false, nx, ny, false, true);
      }
    }
    if (exitPhase && _chainComputed && _exitChainRemaining > 0 && _exitTileX != 255)
    {
      const uint16_t headIdx  = (uint16_t)(_exitChainHeadY * _mapCols + _exitChainHeadX);
      _exitChainHeadCommitted = (_threatOpen[headIdx] != 0) || (_exitChainCrumble > 0.0f);
      _exitChainHeadThreat    = (_threatOpen[headIdx] != 0) && (_exitChainCrumble == 0.0f);
      if (anyBombTicking && _exitChainHeadCommitted == false)
        _exitChainNextDist = bombAtPlayerReaches(_exitChainHeadX, _exitChainHeadY) ? 0.0f : computePathDistance(false, _exitChainHeadX, _exitChainHeadY, false, true);
      if (_exitChainHeadCommitted)
      {
        uint8_t nx = _exitTileX, ny = _exitTileY;
        bool    pastHead = false;
        for (uint8_t i = 0; i < _exitChainLen; i++)
        {
          const uint8_t r = (uint8_t)(_exitChainCells[i] / _mapCols), c = (uint8_t)(_exitChainCells[i] % _mapCols);
          const uint8_t t       = _lowMem[_mapBase + r * _mapStride + c];
          const bool    isBrick = (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick);
          if (c == _exitChainHeadX && r == _exitChainHeadY)
          {
            pastHead = true;
            continue;
          }
          if (pastHead && isBrick)
          {
            nx = c;
            ny = r;
            break;
          }
        }
        _exitChainNextDist = computePathDistance(false, nx, ny, false, true);
      }
    }
    // Kill phase: while a bomb ticks, weakly draw the player toward the NEXT enemy (closest
    // alive enemy not already covered by the ticking bomb's blast) so the fuse doubles as
    // travel time toward the following kill.
    if (killPhase && anyBombTicking) _enemyNextDist = computePathDistance(true, 255, 255, false, true, true);

    // (10) Blast-line escape danger
    _bombEscapeDanger = 0.0f;
    {
      const int r = (int)(_lowMem[0x0073] >> 4);
      for (uint8_t b = 0; b < 10; b++)
      {
        if (_lowMem[0x03A0 + b] == 0) continue;
        const float progress = (float)_lowMem[0x03D2 + b] / 160.0f;
        const int   dx       = (int)_trackedTileX - (int)_lowMem[0x03AA + b];
        const int   dy       = (int)_trackedTileY - (int)_lowMem[0x03B4 + b];
        int         dist     = -1;
        if (dy == 0 && dx >= -r && dx <= r) dist = (dx < 0) ? -dx : dx;
        if (dx == 0 && dy >= -r && dy <= r) dist = (dy < 0) ? -dy : dy;
        if (dist >= 0) _bombEscapeDanger += progress * (float)(r + 1 - dist);
      }
    }

    // (11) Anti-exploit predicate: fresh bomb on a live flame cell
    _bombPlacedInFlames = false;
    for (uint8_t b = 0; b < 10; b++)
      if (_lowMem[0x03A0 + b] != 0 && _lowMem[0x03D2 + b] <= 2)
        for (uint8_t fl = 0; fl < 10; fl++)
          if (_lowMem[0x0079] == 0 && _lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == _lowMem[0x03AA + b] && _lowMem[0x04CC + fl] == _lowMem[0x03B4 + b])
            _bombPlacedInFlames = true;
  }

  // One-shot chain-plan computation for both targets (see stateUpdatePostHook step 6)
  __INLINE__ void computeChainPlan()
  {
    if (_powerupTileX != 255) computeChainTo(1, 1, _powerupTileX, _powerupTileY, _chainCells.data(), _chainLen);
    // Exit chain sources from the POWER-UP tile (grab -> exit strategy): the exit is approached
    // from wherever the kill phase ends, not from spawn -- a spawn-sourced plan can pick a route
    // with MORE bricks than needed (observed: 2-brick left approach when a 1-brick right
    // approach exists). PU tile is a stable static proxy for the post-grab region.
    if (_exitTileX != 255)
      computeChainTo(_powerupTileX != 255 ? _powerupTileX : 1, _powerupTileY != 255 ? _powerupTileY : 1, _exitTileX, _exitTileY, _exitChainCells.data(), _exitChainLen);
    _chainComputed = true;
  }

  // Brick chain along the shortest brick-penalized path src -> target (target cell included)
  __INLINE__ void computeChainTo(const uint8_t srcC, const uint8_t srcR, const uint8_t tgtC, const uint8_t tgtR, uint16_t* cells, uint8_t& outLen)
  {
    constexpr uint16_t INF = 0xFFFF;
    uint16_t           dist[_mapRows * _mapCols];
    int16_t            prev[_mapRows * _mapCols];
    for (auto& v : dist) v = INF;
    for (auto& v : prev) v = -1;
    static thread_local std::vector<uint16_t> bq[512];
    for (auto& b : bq) b.clear();
    dist[srcR * _mapCols + srcC] = 0;
    bq[0].push_back(srcR * _mapCols + srcC);
    for (uint16_t dd = 0; dd < 8192; dd += 16)
      for (size_t qi = 0; qi < bq[dd / 16].size(); qi++)
      {
        const uint16_t n = bq[dd / 16][qi];
        if (dist[n] != dd) continue;
        const uint8_t           r = n / _mapCols, c = n % _mapCols;
        static constexpr int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : dirs)
        {
          const int cc = c + dir[0], rr = r + dir[1];
          if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) continue;
          const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
          if (t == 1) continue;
          // Wallpass/Bombpass-aware: with $76 held only the TARGET brick itself needs breaking
          // (the chain collapses to length 1: walk through walls, bomb the covering brick);
          // with $78 held bombs never block a chain route.
          const bool     brickObs = (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) && _lowMem[0x0076] == 0;
          const bool     bombObs  = (t == 3) && _lowMem[0x0078] == 0;
          const uint16_t w        = (brickObs || bombObs) ? 144 : 16;
          const uint16_t nd       = dd + w;
          if (nd >= 8192) continue;
          const uint16_t idx = rr * _mapCols + cc;
          if (nd < dist[idx])
          {
            dist[idx] = nd;
            prev[idx] = (int16_t)n;
            bq[nd / 16].push_back(idx);
          }
        }
      }
    outLen      = 0;
    int16_t cur = (int16_t)(tgtR * _mapCols + tgtC);
    if (dist[cur] == INF) return;
    uint16_t      rev[64];
    uint8_t       rn     = 0;
    const int16_t srcIdx = (int16_t)(srcR * _mapCols + srcC);
    // Wallpass: path bricks are walked THROUGH, not broken -- normally the chain is exactly the
    // target's covering brick. EXCEPTION (user): a target boxed on all four sides (brick/concrete
    // neighbors only) is UNHITTABLE -- no cell exists to bomb it from, because the blast stops at
    // the first brick in each direction; a brick is hittable iff it has >=1 EMPTY 4-neighbor.
    // Then one hittable brick-neighbor must fall first: a 2-chain. Deeper nesting falls back to
    // the ordinary path-chain below.
    if (_lowMem[0x0076] != 0)
    {
      const uint8_t tt = _lowMem[_mapBase + tgtR * _mapStride + tgtC];
      if (tt == 2 || tt == _tileExitBrick || tt == _tilePowerupBrick)
      {
        auto isBrick = [&](const int c2, const int r2)
        {
          if (c2 < 0 || c2 >= _mapCols || r2 < 0 || r2 >= _mapRows) return false;
          const uint8_t t2 = _lowMem[_mapBase + r2 * _mapStride + c2];
          return t2 == 2 || t2 == _tileExitBrick || t2 == _tilePowerupBrick;
        };
        auto hittable = [&](const int c2, const int r2)
        {
          static constexpr int8_t d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& dd : d4)
          {
            const int nc = c2 + dd[0], nr = r2 + dd[1];
            if (nc < 0 || nc >= _mapCols || nr < 0 || nr >= _mapRows) continue;
            if (_lowMem[_mapBase + nr * _mapStride + nc] == 0) return true;
          }
          return false;
        };
        if (hittable(tgtC, tgtR))
        {
          cells[rn++] = (uint16_t)(tgtR * _mapCols + tgtC);
          outLen      = rn;
          return;
        }
        static constexpr int8_t d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dd : d4)
        {
          const int nc = tgtC + dd[0], nr = tgtR + dd[1];
          if (isBrick(nc, nr) && hittable(nc, nr))
          {
            cells[rn++] = (uint16_t)(nr * _mapCols + nc);
            cells[rn++] = (uint16_t)(tgtR * _mapCols + tgtC);
            outLen      = rn;
            return;
          }
        }
        // triple-boxed (very rare): fall through to the ordinary path-chain
      }
      else
      {
        outLen = 0;
        return;
      }
    }
    while (cur >= 0 && rn < 64)
    {
      const uint8_t t = _lowMem[_mapBase + (cur / _mapCols) * _mapStride + (cur % _mapCols)];
      // The source cell's own brick (e.g. the PU brick when the exit chain sources from the PU
      // tile) is not part of the route -- it is broken by its own chain.
      if (cur != srcIdx && (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)) rev[rn++] = (uint16_t)cur;
      cur = prev[cur];
    }
    for (uint8_t i = 0; i < rn; i++) cells[i] = rev[rn - 1 - i];
    outLen = rn;
  }

  // Dijkstra over the tile map from a target set (all alive enemies, or a single tile), returning
  // the player's smoothed distance in world pixels. Terrain: concrete impassable; bricks and bombs
  // cost +8 tiles on top of the step (a bombing cycle); everything else 1 tile = 16 px. With
  // openThreatened, bomb-threatened bricks AND bombs are treated as clearing (post-explosion world).
  // Marks bricks on the optimal (brick-penalized) route from the player to the nearest alive
  // enemy, and accumulates the crumble ramp for any of them currently burning.
  __INLINE__ void markEnemyPathBricks()
  {
    // Wallpass: the player walks through bricks -- no route ever needs opening, and the
    // route-brick crumble reward would be pure farmable noise. Leave the set empty.
    if (_lowMem[0x0076] != 0)
    {
      for (auto& v : _enemyPathBricks) v = 0;
      _enemyPathCrumble = 0.0f;
      return;
    }
    constexpr uint16_t INF = 0xFFFF;
    uint16_t           dist[_mapRows * _mapCols];
    int16_t            prev[_mapRows * _mapCols];
    for (auto& v : dist) v = INF;
    for (auto& v : prev) v = -1;
    static thread_local std::vector<uint16_t> bq[512];
    for (auto& b : bq) b.clear();
    bool any = false;
    for (uint8_t e = 0; e < _erN; e++)
    {
      const uint8_t st = _lowMem[_erST + e];
      if (st >= 32 || _lowMem[_erX + e] == 0) continue;
      const uint8_t c = _lowMem[_erX + e], r = _lowMem[_erY + e];
      if (c >= _mapCols || r >= _mapRows) continue;
      dist[r * _mapCols + c] = 0;
      bq[0].push_back(r * _mapCols + c);
      any = true;
    }
    if (any == false) return;
    for (uint16_t dd = 0; dd < 8192; dd += 16)
      for (size_t qi = 0; qi < bq[dd / 16].size(); qi++)
      {
        const uint16_t n = bq[dd / 16][qi];
        if (dist[n] != dd) continue;
        const uint8_t           r = n / _mapCols, c = n % _mapCols;
        static constexpr int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : dirs)
        {
          const int cc = c + dir[0], rr = r + dir[1];
          if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) continue;
          if (cc == 0 || cc == _mapCols - 1 || rr == 0 || rr == _mapRows - 1 || ((cc % 2) == 0 && (rr % 2) == 0)) continue;
          const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
          if (t == 1) continue;
          const uint16_t w   = (t == 2 || t == 3 || t == _tileExitBrick || t == _tilePowerupBrick) ? 144 : 16;
          const uint16_t nd  = dd + w;
          const uint16_t idx = rr * _mapCols + cc;
          if (nd < 8192 && nd < dist[idx])
          {
            dist[idx] = nd;
            prev[idx] = (int16_t)n;
            bq[nd / 16].push_back(idx);
          }
        }
      }
    int16_t cur = (int16_t)(_trackedTileY * _mapCols + _trackedTileX);
    if (cur < 0 || dist[cur] == INF) return;
    while (cur >= 0)
    {
      const uint8_t t = _lowMem[_mapBase + (cur / _mapCols) * _mapStride + (cur % _mapCols)];
      if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
      {
        _enemyPathBricks[cur] = 1;
        for (uint8_t fl = 0; fl < 10; fl++)
          if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == (uint8_t)(cur % _mapCols) && _lowMem[0x04CC + fl] == (uint8_t)(cur / _mapCols))
            _enemyPathCrumble += (5.0f + std::min(1.0f, (float)_lowMem[0x0025] / 8.0f)) / 6.0f;
      }
      cur = prev[cur];
    }
  }

  // True if a bomb placed at the player's CURRENT tile would blast the target tile (straight
  // line within flame radius, unblocked by walls; the first brick per ray is reachable).
  __INLINE__ bool bombAtPlayerReaches(const uint8_t tc, const uint8_t tr) const
  {
    const int pc = _trackedTileX, pr = _trackedTileY, r = (int)_flameCount;
    if (pc == tc && pr == tr) return true;
    if (pc != tc && pr != tr) return false;
    const int d = std::abs(pc - tc) + std::abs(pr - tr);
    if (d > r) return false;
    const int dc = (tc > pc) ? 1 : (tc < pc) ? -1 : 0, dr = (tr > pr) ? 1 : (tr < pr) ? -1 : 0;
    for (int st = 1; st < d; st++)
    {
      const uint8_t t = _lowMem[_mapBase + (pr + dr * st) * _mapStride + (pc + dc * st)];
      if (t != 0) return false; // wall/brick/bomb blocks the ray before the target
    }
    return true;
  }

  __INLINE__ float computePathDistance(const bool toEnemies, const uint8_t tgtC, const uint8_t tgtR, const bool openThreatened = false, const bool playerSource = false,
                                       const bool excludeBombTargets = false) const
  {
    constexpr uint16_t INF = 0xFFFF;
    uint16_t           dist[_mapRows * _mapCols];
    for (auto& v : dist) v = INF;

    constexpr uint16_t                        maxD = 8192;
    static thread_local std::vector<uint16_t> buckets[maxD / 16];
    for (auto& b : buckets) b.clear();

    bool anyTarget = false;
    if (toEnemies)
    {
      const int radius = (int)_flameCount;
      for (uint8_t e = 0; e < _erN; e++)
      {
        const uint8_t st = _lowMem[_erST + e];
        if (st >= 32 || _lowMem[_erX + e] == 0) continue;
        const uint8_t c = _lowMem[_erX + e], r = _lowMem[_erY + e];
        if (c >= _mapCols || r >= _mapRows) continue;
        if (excludeBombTargets)
        {
          // Skip enemies already covered by a ticking bomb's blast cross: they are that bomb's
          // business; the secondary pull steers the player toward the NEXT objective instead.
          bool covered = false;
          for (uint8_t b = 0; b < 10; b++)
          {
            if (_lowMem[0x03A0 + b] == 0) continue;
            const int bc = _lowMem[0x03AA + b], br = _lowMem[0x03B4 + b];
            if ((br == (int)r && std::abs(bc - (int)c) <= radius) || (bc == (int)c && std::abs(br - (int)r) <= radius)) covered = true;
          }
          if (covered) continue;
        }
        dist[r * _mapCols + c] = 0;
        buckets[0].push_back(r * _mapCols + c);
        anyTarget = true;
      }
    }
    else if (tgtC < _mapCols && tgtR < _mapRows)
    {
      dist[tgtR * _mapCols + tgtC] = 0;
      buckets[0].push_back(tgtR * _mapCols + tgtC);
      anyTarget = true;
    }
    if (anyTarget == false) return 0.0f;

    for (uint16_t d = 0; d < maxD; d += 16)
      for (size_t qi = 0; qi < buckets[d / 16].size(); qi++)
      {
        const uint16_t n = buckets[d / 16][qi];
        if (dist[n] != d) continue;
        const uint8_t           r = n / _mapCols, c = n % _mapCols;
        static constexpr int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : dirs)
        {
          const int cc = c + dir[0], rr = r + dir[1];
          if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) continue;
          const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
          if (t == 1) continue;
          const bool clearing = openThreatened && (_threatOpen[rr * _mapCols + cc] != 0 || t == 3);
          // Powerup-aware obstacle model: Wallpass ($76) makes bricks free to walk; Bombpass
          // ($78) makes bombs free. Post-Wallpass the only true walls are concrete -- routing,
          // magnets, and door-run walks all inherit the new geometry through this one weight.
          const bool     brickObs = (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) && _lowMem[0x0076] == 0;
          const bool     bombObs  = (t == 3) && _lowMem[0x0078] == 0;
          const uint16_t w        = (clearing == false && (brickObs || bombObs)) ? 144 : 16;
          const uint16_t nd       = d + w;
          if (nd >= maxD) continue;
          const uint16_t idx = rr * _mapCols + cc;
          if (nd < dist[idx])
          {
            dist[idx] = nd;
            buckets[nd / 16].push_back(idx);
          }
        }
      }

    // Source: a PLACED bomb does the approaching (user directive) -- while any bomb ticks (or
    // its flames burn), proximity is measured from the best-placed bomb, freeing the player to
    // retreat. With no bomb down, measure from the player with sub-tile smoothing. The magnet
    // intensities must be strong enough that approach dominates side rewards (pending kill).
    float best       = 65535.0f;
    bool  bombActive = false;
    for (uint8_t i = 0; i < 10 && playerSource == false; i++)
    {
      const bool ticking   = _lowMem[0x03A0 + i] != 0;
      const bool exploding = (ticking == false) && (_bombShadow[i] != 0);
      if (ticking == false && exploding == false) continue;
      const uint8_t bc = ticking ? _lowMem[0x03AA + i] : _bombLastX[i];
      const uint8_t br = ticking ? _lowMem[0x03B4 + i] : _bombLastY[i];
      if (bc >= _mapCols || br >= _mapRows) continue;
      // RELEVANCE GATE (stage-13 tail fix): a bomb only anchors the distance source if its
      // blast actually ENGAGES the target set -- covers an alive enemy (toEnemies) or the
      // target tile itself. A stale, unrelated bomb otherwise FREEZES the approach gradient
      // (observed: 948-frame tail where walking to the exit paid zero because a leftover
      // kill-phase bomb was the measuring point). Axis-aligned radius check, obstruction-free
      // (slightly generous is fine -- the gate only decides who measures, not who scores).
      {
        const int rad     = (int)_flameCount;
        bool      engages = false;
        if (toEnemies)
        {
          for (uint8_t e = 0; e < _erN && engages == false; e++)
          {
            if (_lowMem[_erST + e] >= 32 || _lowMem[_erX + e] == 0) continue;
            const int ec = _lowMem[_erX + e], er2 = _lowMem[_erY + e];
            if ((er2 == (int)br && std::abs(ec - (int)bc) <= rad) || (ec == (int)bc && std::abs(er2 - (int)br) <= rad)) engages = true;
          }
        }
        else if (tgtC < _mapCols && tgtR < _mapRows)
          engages = ((int)tgtR == (int)br && std::abs((int)tgtC - (int)bc) <= rad) || ((int)tgtC == (int)bc && std::abs((int)tgtR - (int)br) <= rad);
        if (engages == false) continue;
      }
      bombActive = true;
      // The distance field charges the bomb's own body (obstacle weight 144) on the hop into its
      // tile, which would penalize a perfectly-placed bomb ~9 tiles. A bomb is not an obstacle to
      // itself: enter its tile from the cheapest neighbor at normal cost.
      const uint16_t idx = br * _mapCols + bc;
      float          v   = (dist[idx] == 0) ? 0.0f : 65535.0f;
      if (v > 0.0f)
      {
        static constexpr int8_t nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& d2 : nb)
        {
          const int cc = bc + d2[0], rr = br + d2[1];
          if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) continue;
          if (dist[rr * _mapCols + cc] == INF) continue;
          const float cand2 = (float)dist[rr * _mapCols + cc] + 16.0f;
          if (cand2 < v) v = cand2;
        }
      }
      if (v < best) best = v;
    }
    if (bombActive == false)
    {
      const float             px = (float)_playerPosX, py = (float)_playerPosY;
      const int               pc = _trackedTileX, pr = _trackedTileY;
      static constexpr int8_t cand[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& cd : cand)
      {
        const int cc = pc + cd[0], rr = pr + cd[1];
        if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) continue;
        if (dist[rr * _mapCols + cc] == INF) continue;
        const float cx = (float)cc * 16.0f + 8.0f, cy = (float)rr * 16.0f + 8.0f;
        const float v = (float)dist[rr * _mapCols + cc] + std::abs(px - cx) + std::abs(py - cy);
        if (v < best) best = v;
      }
    }
    return (best >= 65535.0f) ? 8192.0f : best;
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    _playerPosXMagnet.intensity = 0.0f;
    _playerPosXMagnet.pos       = 0.0f;
    _playerPosYMagnet.intensity = 0.0f;
    _playerPosYMagnet.pos       = 0.0f;
    _exitMagnet                 = 0.0f;
    _enemiesLeftMagnet          = 0.0f;
    _closestEnemyMagnet         = 0.0f;
    _powerupMagnet              = 0.0f;
    _powerupProgressMagnet      = 0.0f;
    _pendingBrickMagnet         = 0.0f;
    _pendingKillMagnet          = 0.0f;
    _pendingHazardMagnet        = 0.0f;
    _headFuseMagnet             = 0.0f;
    _bombEnemyMagnet            = 0.0f;
    _enemiesKilledMagnet        = 0.0f;
    _bombEscapeMagnet           = 0.0f;
    _chainHeadMagnet            = 0.0f;
    _chainBrickReward           = 0.0f;
    _grabBonus                  = 0.0f;
    _exitChainHeadMagnet        = 0.0f;
    _exitChainBrickReward       = 0.0f;
  }
  __INLINE__ void ruleUpdatePostHook() override
  {
    _playerDistanceToPointX = std::abs((float)_playerPosXMagnet.pos - (float)_playerPosX);
    _playerDistanceToPointY = std::abs((float)_playerPosYMagnet.pos - (float)_playerPosY);
    // Manhattan world-pixel distance to the exit tile's center (valid once the exit is known;
    // zero contribution while unknown)
    _playerDistanceToExit = 0.0f;
    if (_exitTileX != 255)
    {
      const float ex        = (float)_exitTileX * 16.0f + 8.0f;
      const float ey        = (float)_exitTileY * 16.0f + 8.0f;
      _playerDistanceToExit = std::abs(ex - (float)_playerPosX) + std::abs(ey - (float)_playerPosY);
    }
  }
  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_currentStep, sizeof(_currentStep));
    serializer.push(&_lastInput, sizeof(_lastInput));
    serializer.push(&_trackedTileX, sizeof(_trackedTileX));
    serializer.push(&_trackedTileY, sizeof(_trackedTileY));
    serializer.push(&_trackedPixX, sizeof(_trackedPixX));
    serializer.push(&_trackedPixY, sizeof(_trackedPixY));
    serializer.push(&_trackedMismatch, sizeof(_trackedMismatch));
    serializer.push(_bombLastX, sizeof(_bombLastX));
    serializer.push(_bombLastY, sizeof(_bombLastY));
    serializer.push(_bombShadow, sizeof(_bombShadow));
    serializer.push(_bombWasTicking, sizeof(_bombWasTicking));
    serializer.push(&_caughtInBlast, sizeof(_caughtInBlast));
    serializer.push(&_enemyPathBricksBroken, sizeof(_enemyPathBricksBroken));
    if (_replanChainsOnGrab)
    {
      serializer.push(&_exitReplanned, sizeof(_exitReplanned));
      serializer.push(&_exitChainLen, sizeof(_exitChainLen));
      serializer.push(_exitChainCells.data(), sizeof(uint16_t) * _exitChainCells.size());
    }
  }
  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.pop(&_currentStep, sizeof(_currentStep));
    deserializer.pop(&_lastInput, sizeof(_lastInput));
    deserializer.pop(&_trackedTileX, sizeof(_trackedTileX));
    deserializer.pop(&_trackedTileY, sizeof(_trackedTileY));
    deserializer.pop(&_trackedPixX, sizeof(_trackedPixX));
    deserializer.pop(&_trackedPixY, sizeof(_trackedPixY));
    deserializer.pop(&_trackedMismatch, sizeof(_trackedMismatch));
    deserializer.pop(_bombLastX, sizeof(_bombLastX));
    deserializer.pop(_bombLastY, sizeof(_bombLastY));
    deserializer.pop(_bombShadow, sizeof(_bombShadow));
    deserializer.pop(_bombWasTicking, sizeof(_bombWasTicking));
    deserializer.pop(&_caughtInBlast, sizeof(_caughtInBlast));
    deserializer.pop(&_enemyPathBricksBroken, sizeof(_enemyPathBricksBroken));
    if (_replanChainsOnGrab)
    {
      deserializer.pop(&_exitReplanned, sizeof(_exitReplanned));
      deserializer.pop(&_exitChainLen, sizeof(_exitChainLen));
      deserializer.pop(_exitChainCells.data(), sizeof(uint16_t) * _exitChainCells.size());
    }
  }
  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;
    // Optional position magnets (zero unless a config sets an intensity).
    reward += -1.0 * _playerPosXMagnet.intensity * _playerDistanceToPointX;
    reward += -1.0 * _playerPosYMagnet.intensity * _playerDistanceToPointY;
    // One-off power-up grab bonus (rule-gated on Flame Count >= 2; latched by the stat itself)
    reward += _grabBonus;

    // Brick-chain plan: ladder per destroyed chain brick + pull toward the chain head
    reward += _chainBrickReward * ((float)(_chainLen - _chainRemaining) + _chainCrumble);
    if (_chainHeadThreat) reward += 0.5f * _chainBrickReward; // head covered by a ticking bomb (pre-crumble)
    reward += -1.0 * _chainHeadMagnet * (_chainHeadDist - _bombMaxProgress * _chainHeadGain);
    if (_chainNextDist >= 0.0f) reward += 0.2f * _chainHeadMagnet * std::max(0.0f, 800.0f - _chainNextDist);

    // Exit chain (identical logic, active once all enemies are dead)
    reward += _exitChainBrickReward * ((float)(_exitChainLen - _exitChainRemaining) + _exitChainCrumble);
    if (_exitChainHeadThreat) reward += 0.5f * _exitChainBrickReward;
    reward += -1.0 * _exitChainHeadMagnet * (_exitChainHeadDist - _bombMaxProgress * _exitChainHeadGain);
    if (_exitChainNextDist >= 0.0f) reward += 0.2f * _exitChainHeadMagnet * std::max(0.0f, 800.0f - _exitChainNextDist);
    // Reward for reducing the enemy count (the stage's primary objective)
    reward += -1.0 * _enemiesLeftMagnet * (float)*_enemiesLeft;
    // Per-kill payoff from the enemy TABLE (robust: $9C reads 0 transiently; table state >= 32
    // latches at death). Config action "Set Enemies Killed Magnet"; v2 configs use this at 10k
    // and zero the $9C ladder above.
    reward += _enemiesKilledMagnet * (float)_enemiesKilledCount;
    // Covered-enemy credit: half a kill while a ticking cross covers an enemy (Detonator mode,
    // or any mode with "Covered Enemies Handled")
    reward += 1500.0f * (float)_coveredEnemies;
    // Bonus mode: the objective is the kill counter itself ($9E, +1 per kill, hashed).
    // AREA DENIAL (user, 2026-07-30: bonus C vs avoider Dolls was suboptimal): +250 per ticking
    // bomb keeps the arena carpeted so wanderers walk into coverage -- a covered kill pays
    // 3000+1500, so popping still strictly dominates holding when a target is in the cross.
    if (_bonusMode)
    {
      reward += 3000.0f * (float)_lowMem[0x009E];
      uint8_t ticking = 0;
      uint8_t bx[10], by[10];
      for (uint8_t i = 0; i < 10; i++)
        if (_lowMem[0x03A0 + i] != 0)
        {
          bx[ticking] = _lowMem[0x03AA + i];
          by[ticking] = _lowMem[0x03B4 + i];
          ticking++;
        }
      // CARPET SHAPING -- bomb count (+250 each), pairwise bomb separation (10/tile) and
      // player-bomb separation (8/tile; avoiders shun the player, so coverage near the player
      // is wasted). CRITICAL (user caught skipped kills): these SUSPEND while any enemy is
      // covered -- shaping must never tax the pop. The ROM pops the NEWEST slot first, so a
      // kill via an old bomb dismantles the newer carpet; with shaping active during that
      // sequence, marginal kills lost to the friction. With it suspended, pop-to-kill (+3000)
      // dominates unconditionally the moment a target enters a cross.
      if (_coveredEnemies == 0)
      {
        reward += 250.0f * (float)ticking;
        float spread = 0.0f;
        for (uint8_t i = 0; i < ticking; i++)
          for (uint8_t j = i + 1; j < ticking; j++) spread += (float)(std::abs((int)bx[i] - (int)bx[j]) + std::abs((int)by[i] - (int)by[j]));
        reward += 10.0f * spread;
        float     pspread = 0.0f;
        const int ptx = (int)*_playerTileX, pty = (int)*_playerTileY;
        for (uint8_t i = 0; i < ticking; i++) pspread += (float)(std::abs(ptx - (int)bx[i]) + std::abs(pty - (int)by[i]));
        reward += 8.0f * pspread;
        // ENEMY CENTER-OF-MASS pull (user): carpet WHERE the roster is, not the spawn corner --
        // each ticking bomb earns up to 180 shrinking with tile distance to the alive-enemy
        // centroid (6/tile inside 30). Total <= ~720, still under one kill.
        float   ecx = 0.0f, ecy = 0.0f;
        uint8_t na = 0;
        for (uint8_t e = 0; e < _erN; e++)
        {
          if (_lowMem[_erST + e] >= 32 || _lowMem[_erX + e] == 0) continue;
          ecx += (float)_lowMem[_erX + e];
          ecy += (float)_lowMem[_erY + e];
          na++;
        }
        if (na > 0 && ticking > 0)
        {
          ecx /= (float)na;
          ecy /= (float)na;
          for (uint8_t i = 0; i < ticking; i++)
          {
            const float dd = std::abs((float)bx[i] - ecx) + std::abs((float)by[i] - ecy);
            reward += 6.0f * std::max(0.0f, 30.0f - dd);
          }
        }
      }
    }
    // Pull toward the closest alive enemy (brick-aware path distance)
    // APPROACH CLAMP (Pass-hover mirage, deployed 2026-07-31 after s23 6-alive pin): within 48px
    // (3 tiles) the term flattens -- being AT a chaser pays no more than being NEAR it, so
    // adjacency hovers stop out-ranking commit-and-kill lines. Chasers close the last 3 tiles
    // themselves; wanderers get committed to via the covered-enemy credit.
    reward += -1.0 * _closestEnemyMagnet * std::max(_pathDistEnemy, 48.0f);
    reward += 1000.0f * _enemyPathCrumble; // burning brick on the current enemy route (wall-opening commitment)
    // Small ANY-BRICK ladder (user): +10 per plain brick destroyed -- softens long routes
    // without outbidding real goals (kill 3000, chains 6000, route approach ~128).
    if (_initialBricks > _bricksLeft) reward += _anyBrickLadderReward * (float)(_initialBricks - _bricksLeft);
    // Route-brick ladder retired: the wandering-enemy route sweep made +400/brick farmable
    // (tunnel-mining), and with the at-will annuities gone the approach term's ~+128/brick
    // permanent gain suffices. Counter retained for display/diagnostics only.
    // 0.5/px (user, 2026-07-29: 0.1 was too weak -- best state wandered off to side motivations
    // instead of pre-positioning toward the next target while the current bomb ticks)
    if (_enemyNextDist >= 0.0f) reward += 0.5f * _closestEnemyMagnet * std::max(0.0f, 800.0f - _enemyNextDist);
    // Power-up pickup ladder (permanent stat gains)
    reward += _powerupProgressMagnet * (float)_powerupProgress;
    // Escape-the-blast shaping (pre-Flamepass): penalize lingering in a maturing bomb's cross
    reward += -1.0 * _bombEscapeMagnet * _bombEscapeDanger;
    // Bomb-in-flight pending value (grows every fuse frame; temporal continuity)
    reward += _pendingBrickMagnet * _pendingBricks;
    reward += _pendingKillMagnet * _pendingKills;
    reward += -1.0 * _pendingHazardMagnet * _pendingHazard;
    // Dedicated inverse-time-to-detonation gradient for the head-covering bomb (fuse mode only)
    reward += _headFuseMagnet * _headFuseProgress;
    // Planted-bomb-to-enemy closeness bonus (no bomb ticking -> 0; never a penalty)
    if (_bombEnemyDist < 1e9f) reward += _bombEnemyMagnet * std::max(0.0f, 320.0f - _bombEnemyDist);
    // Returning reward
    return reward;
  }
  // Function to report what all the possible input that the game might require
  __INLINE__ std::set<std::string> getAllPossibleInputs() override { return {}; }
  // Function to enable a game code to provide additional allowed inputs based on complex decisions
  // Corridor-parity input pruning (all classes byte-exact-proven no-ops, see MECHANICS.md):
  // in a horizontal-corridor cell (odd row, even col) the pillars above/below make U/D dead at
  // any pixel offset (no cornering assist; turn window < +-4 px so straddled tiles stay safe);
  // symmetric for vertical corridors; border-facing directions are dead at edge crossroads.
  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override
  {
    if (_nextFrameAcceptsInput == false) return; // phase-0 frames swallow all input
    if (_disableInputRestrictions)
    {
      // Everything-goes: the full joypad universe, every frame, no gating.
      allowedInputSet.clear();
      const InputSet::inputIndex_t all[] = {_nullInputIdx, _inputU,  _inputD,  _inputL,  _inputR,  _inputA,  _inputB,   _inputUA,  _inputDA,  _inputLA,
                                            _inputRA,      _inputUB, _inputDB, _inputLB, _inputRB, _inputAB, _inputUAB, _inputDAB, _inputLAB, _inputRAB};
      for (auto i : all) allowedInputSet.push_back(i);
      if (_allowCompositeDirections)
      {
        const InputSet::inputIndex_t comps[] = {_inputUL, _inputUR, _inputDL, _inputDR, _inputLR, _inputUD};
        for (auto i : comps) allowedInputSet.push_back(i);
      }
      return;
    }
    if (_bonusMode)
    {
      // BONUS STAGE (user, 2026-07-29): player is invincible (ROM $C524 sets $7A/$7D=1), the
      // 30-tick timer is input-independent, and the goal is pure kill count. Free policy:
      // movement per corridor parity, A at spare capacity ANYWHERE (no relevance/hazard gates),
      // B whenever a bomb ticks -- INCLUDING inside its cross (standing-on-bomb point-blank
      // pops are legal and optimal here). The $7B latch null cadence stays (ROM-required).
      if (_lowMem[0x007B] != 0)
      {
        allowedInputSet.clear();
        allowedInputSet.push_back(_nullInputIdx);
        return;
      }
      const uint8_t r = _trackedTileY, c = _trackedTileX;
      const bool    rOdd = (r & 1) != 0, cOdd = (c & 1) != 0;
      // Same straddle windows as the main path (assist engages from corridor-cell edges)
      const bool strX = (_trackedPixX <= 2) || (_trackedPixX >= 13);
      const bool strY = (_trackedPixY <= 2) || (_trackedPixY >= 13);
      const bool bU = (cOdd || strX) && r > 1, bD = (cOdd || strX) && r < _mapRows - 2, bL = (rOdd || strY) && c > 1, bR = (rOdd || strY) && c < _mapCols - 2;
      if (bU) allowedInputSet.push_back(_inputU);
      if (bD) allowedInputSet.push_back(_inputD);
      if (bL) allowedInputSet.push_back(_inputL);
      if (bR) allowedInputSet.push_back(_inputR);
      if (_allowCompositeDirections)
      {
        // Diagonals where both components are legal; opposing pairs where either component is
        // (U+D / L+R nudge-then-freeze works with a single live component).
        if (bU && bL) allowedInputSet.push_back(_inputUL);
        if (bU && bR) allowedInputSet.push_back(_inputUR);
        if (bD && bL) allowedInputSet.push_back(_inputDL);
        if (bD && bR) allowedInputSet.push_back(_inputDR);
        if (bL || bR) allowedInputSet.push_back(_inputLR);
        if (bU || bD) allowedInputSet.push_back(_inputUD);
      }
      uint8_t nBombs = 0;
      for (uint8_t i = 0; i < 10; i++)
        if (_lowMem[0x03A0 + i] != 0) nBombs++;
      if (nBombs <= _lowMem[0x0074]) allowedInputSet.push_back(_inputA);
      if (nBombs > 0)
      {
        allowedInputSet.push_back(_inputB);
        if (cOdd && r > 1) allowedInputSet.push_back(_inputUB);
        if (cOdd && r < _mapRows - 2) allowedInputSet.push_back(_inputDB);
        if (rOdd && c > 1) allowedInputSet.push_back(_inputLB);
        if (rOdd && c < _mapCols - 2) allowedInputSet.push_back(_inputRB);
      }
      // Refused-A align tool (see main path below for the full rationale)
      if (_allowCompositeDirections && nBombs > _lowMem[0x0074] && _lowMem[_mapBase + r * _mapStride + c] == 0 && (_trackedPixX != 8 || _trackedPixY != 8))
      {
        allowedInputSet.push_back(_inputA);
        if (bU) allowedInputSet.push_back(_inputUA);
        if (bD) allowedInputSet.push_back(_inputDA);
        if (bL) allowedInputSet.push_back(_inputLA);
        if (bR) allowedInputSet.push_back(_inputRA);
      }
      return;
    }
    const uint8_t r = _trackedTileY, c = _trackedTileX;
    if (r >= _mapRows || c >= _mapCols) return; // not in play
    const bool rOdd = (r & 1) != 0, cOdd = (c & 1) != 0;
    // Straddle windows (2026-08-04): the corridor-parity no-op proofs (E1/E2) only hold at
    // centered / >=4px-off positions. Within ~3px of a corridor cell's edge the neighboring
    // cell along the corridor is a crossroad and the cornering assist already engages there
    // (verified: DR from a corridor cell at offset 15 moves +2x+1y). So perpendicular
    // directions must also be offered while straddling.
    const bool strX    = (_trackedPixX <= 2) || (_trackedPixX >= 13);
    const bool strY    = (_trackedPixY <= 2) || (_trackedPixY >= 13);
    const bool allowUD = cOdd || strX, allowLR = rOdd || strY;
    const bool bU = allowUD && r > 1, bD = allowUD && r < _mapRows - 2, bL = allowLR && c > 1, bR = allowLR && c < _mapCols - 2;
    if (bU) allowedInputSet.push_back(_inputU);
    if (bD) allowedInputSet.push_back(_inputD);
    if (bL) allowedInputSet.push_back(_inputL);
    if (bR) allowedInputSet.push_back(_inputR);
    if (_allowCompositeDirections)
    {
      // Diagonals where both components are legal (crossroad cells; this is where the
      // cornering-assist stacking lives). Opposing pairs where either component is legal
      // (1px-nudge-then-freeze hover).
      if (bU && bL) allowedInputSet.push_back(_inputUL);
      if (bU && bR) allowedInputSet.push_back(_inputUR);
      if (bD && bL) allowedInputSet.push_back(_inputDL);
      if (bD && bR) allowedInputSet.push_back(_inputDR);
      if (bL || bR) allowedInputSet.push_back(_inputLR);
      if (bU || bD) allowedInputSet.push_back(_inputUD);
    }
    uint8_t activeBombs = 0;
    for (uint8_t i = 0; i < 10; i++)
      if (_lowMem[0x03A0 + i] != 0) activeBombs++;
    // Refused-A align tool (2026-08-04; supersedes the old "auto-center is replicable by
    // direction holds" assumption, which the align mechanic disproves). Every A press over an
    // EMPTY tile runs the align-to-center routines (ROM $CD01: JSR $CE10/$CE1F, 1px per axis
    // toward offset 8) BEFORE the free-slot check. At capacity the press places nothing but the
    // nudge still fires and STACKS with same-frame movement: 2px/frame through each tile's
    // approach half, plus a bare-A diagonal center-pull no direction combo can produce. Offered
    // only at capacity (with a slot free, A means an actual bomb placement -- relevance-gated
    // below), on an empty tile (handler bails otherwise), and off-center (at dead center the
    // routines no-op). The forced-detonation cadence later in this function intentionally
    // overrides these offers where it clears the set. Note this also runs during the doorRun
    // endgame: the old A/B noise prune predates the align discovery, and align-A is exactly a
    // door-approach accelerant.
    if (_allowCompositeDirections && activeBombs > _lowMem[0x0074] && _lowMem[_mapBase + (size_t)r * _mapStride + c] == 0 && (_trackedPixX != 8 || _trackedPixY != 8))
    {
      allowedInputSet.push_back(_inputA);
      if (bU) allowedInputSet.push_back(_inputUA);
      if (bD) allowedInputSet.push_back(_inputDA);
      if (bL) allowedInputSet.push_back(_inputLA);
      if (bR) allowedInputSet.push_back(_inputRA);
    }
    // A (as a bomb placement) only when a slot is actually available (capacity = $74 + 1).
    // Detonator: B (detonate at will) is offered whenever a bomb is ticking AND the trigger
    // latch $7B is clear. ROM $CCB5: the latch (set 0x40 on each detonation) clears ONLY on a
    // frame where the ENTIRE pad reads zero -- so a null input frame is re-offered exactly when
    // the latch must be cleared. CRITICAL: this must live OUTSIDE the capacity-gated A block
    // below -- at full capacity (both bombs down) that block is skipped entirely, and nesting
    // the B offer inside it made every two-bomb lineage a permanent zombie (no fuse, no B).
    // ENDGAME PRUNE (user): power-up held + all enemies dead + exit door already open -- the
    // only remaining task is walking to the door. A and B are pure noise branches; suppress
    // both entirely.
    const bool doorRun = _powerupObtained && _enemiesAlive == 0 && _exitRevealed;
    bool       detNoA  = false; // detonator cadence: A suppressed while escaping own blast zone
    if (_lowMem[0x0077] != 0 && doorRun == false)
    {
      // USER SPEC (final): post-pickup -- (1) B never offered inside a blast cross, and
      // forced-EXCLUSIVE once outside (with a bomb ticking); (2) ROM $C939: B detonates exactly
      // ONE bomb, the HIGHEST-index active slot (stranded low slots clear via consecutive
      // B/null cycles, which this cadence produces); (3) null appears ONLY as the forced re-arm
      // frame while $7B is latched (ROM-required: the latch clears only on an all-zero pad on a
      // phase 1-3 frame -- inert frames are skipped by the clear code).
      bool anyTick = false;
      for (uint8_t bi = 0; bi < 10; bi++)
        if (_lowMem[0x03A0 + bi] != 0) anyTick = true;
      if (_lowMem[0x007B] != 0)
      {
        allowedInputSet.clear();
        allowedInputSet.push_back(_nullInputIdx);
        return;
      }
      if (anyTick)
      {
        bool inCross = false;
        if (_lowMem[0x0079] == 0) // Flamepass: standing in a cross is safe -- no escape mode, B offered anywhere
        {
          const int pc = _trackedTileX, pr = _trackedTileY, r0 = (int)_flameCount;
          for (uint8_t bi = 0; bi < 10 && inCross == false; bi++)
          {
            if (_lowMem[0x03A0 + bi] == 0) continue;
            const int bx = _lowMem[0x03AA + bi], by = _lowMem[0x03B4 + bi];
            if (bx == pc && by == pr) inCross = true;
            static constexpr int8_t dirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& dd : dirs4)
              for (int st = 1; st <= r0 && inCross == false; st++)
              {
                const int cc = bx + dd[0] * st, rr = by + dd[1] * st;
                if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
                const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
                if (t == 1) break;
                if (cc == pc && rr == pr) inCross = true;
                if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick) break;
              }
          }
        }
        if (inCross == false)
        {
          // FORCE IMMEDIATE DETONATION (user, 2026-08-01): with Flamepass held, laying bombs as
          // traps and holding them has ~zero value, and the "when to pop" degree of freedom blew
          // up exploration (ghost-chasing, caged-enemy deadlocks needing 30GB). Force B the instant
          // ANY bomb ticks -- the only remaining freedom is WHERE to lay (the A block, reached only
          // when no bomb is down). Offer move+B composites (move AND detonate in one frame, per
          // parity) + plain B (detonate in place), EXCLUSIVE (clear everything else). The $7B
          // latch-null cadence above interleaves nulls between consecutive pops, so a stack of
          // bombs rips through as B/null/B/null. A→B→A→B is now the whole bomb game: lay, pop,
          // repeat -- "wreak havoc" instead of setting traps.
          const uint8_t r2 = _trackedTileY, c2 = _trackedTileX;
          const bool    rOdd2 = (r2 & 1) != 0, cOdd2 = (c2 & 1) != 0;
          if (_forceImmediateDetonation)
          {
            // FORCE IMMEDIATE DETONATION: B the instant any bomb ticks -- exclusive.
            allowedInputSet.clear();
            if (cOdd2 && r2 > 1) allowedInputSet.push_back(_inputUB);
            if (cOdd2 && r2 < _mapRows - 2) allowedInputSet.push_back(_inputDB);
            if (rOdd2 && c2 > 1) allowedInputSet.push_back(_inputLB);
            if (rOdd2 && c2 < _mapCols - 2) allowedInputSet.push_back(_inputRB);
            allowedInputSet.push_back(_inputB);
            return;
          }
          // FLEXIBLE detonation: offer B and the move+B composites as ADDITIONS (movement is already
          // in the set; the A-placement block below still runs). The search may pop now, move-and-pop,
          // or move without popping (delaying the blast) -- at the cost of a much larger branching factor.
          allowedInputSet.push_back(_inputB);
          if (cOdd2 && r2 > 1) allowedInputSet.push_back(_inputUB);
          if (cOdd2 && r2 < _mapRows - 2) allowedInputSet.push_back(_inputDB);
          if (rOdd2 && c2 > 1) allowedInputSet.push_back(_inputLB);
          if (rOdd2 && c2 < _mapCols - 2) allowedInputSet.push_back(_inputRB);
          // fall through: the A-placement block below remains in play
        }
        else
          detNoA = true; // inside own blast zone (no flamepass): movement only (escape)
      }
    }
    if (doorRun == false && detNoA == false && activeBombs <= _lowMem[0x0074])
    {
      if (_forceImmediateBomb)
      {
        // Conditional forced drop: A becomes the ONLY input when a bomb placed HERE would be
        // useful -- its blast reaches a remaining chain brick of the relevant phase (power-up
        // chain pre-pickup; exit chain in kill/exit phases: pre-breaking the door path is
        // always worthwhile), or an APPROACHING enemy (heading reduces distance; $5D4:
        // 1=right 2=up 3=left 4=down) is within 16*(radius+2) px. Useless placements exclude
        // A entirely. Hazard veto: a blast that would touch the revealed power-up or the door
        // never places.
        bool                    useful = false, hazardV = false, brickAny = false;
        const int               r  = (int)_flameCount;
        const uint8_t           pc = _trackedTileX, pr = _trackedTileY;
        const bool              prePickup  = (_powerupObtained == false);
        static constexpr int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : dirs)
          for (int step = 1; step <= r; step++)
          {
            const int cc = pc + dir[0] * step, rr = pr + dir[1] * step;
            if (cc < 0 || cc >= _mapCols || rr < 0 || rr >= _mapRows) break;
            const uint8_t t = _lowMem[_mapBase + rr * _mapStride + cc];
            if (t == 1) break;
            if (t == _tilePowerup || t == _tileExit)
            {
              hazardV = true;
              break;
            }
            if (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick)
            {
              const uint16_t idx = (uint16_t)(rr * _mapCols + cc);
              // Marginal coverage (multi-bomb): a brick already threatened by a ticking bomb is
              // that bomb's business -- a second bomb for it is a junk branch. _threatOpen is
              // all-zero when no bomb ticks, so this degenerates to the old behavior at capacity 1.
              if (_threatOpen[idx] == 0)
              {
                if (prePickup)
                  for (uint8_t ci = 0; ci < _chainLen; ci++)
                    if (_chainCells[ci] == idx) useful = true;
                for (uint8_t ci = 0; ci < _exitChainLen; ci++)
                  if (_exitChainCells[ci] == idx) useful = true;
                brickAny = true; // reachable un-threatened brick: offer A as a branch (path-opening)
              }
              break;
            }
          }
        bool nearby = false;
        if (hazardV == false)
        {
          const float px = (float)_playerPosX, py = (float)_playerPosY;
          const float vic = 16.0f * (float)(r + 2);
          for (uint8_t e = 0; e < _erN; e++)
          {
            const uint8_t st = _lowMem[_erST + e];
            if (st >= 32 || _lowMem[_erX + e] == 0) continue;
            // A doomed enemy is no basis for a bomb (user): dying latches 1-2 frames after flame
            // contact, so an enemy standing in live flames is still nominally alive but already
            // dead for planning purposes.
            {
              bool burning = false;
              for (uint8_t fl = 0; fl < 10; fl++)
                if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == _lowMem[_erX + e] && _lowMem[0x04CC + fl] == _lowMem[_erY + e]) burning = true;
              if (burning) continue;
            }
            const float ex = (float)_lowMem[_erX + e] * 16.0f + (float)_lowMem[_erPX + e];
            const float ey = (float)_lowMem[_erY + e] * 16.0f + (float)_lowMem[_erPY + e];
            if (std::abs(ex - px) + std::abs(ey - py) > vic) continue;
            // Marginal coverage (multi-bomb): an enemy already inside a ticking bomb's blast
            // cross is covered; a second bomb for it is redundant.
            bool covered = false;
            for (uint8_t b = 0; b < 10; b++)
            {
              if (_lowMem[0x03A0 + b] == 0) continue;
              const int bc2 = _lowMem[0x03AA + b], br2 = _lowMem[0x03B4 + b];
              const int ec = _lowMem[_erX + e], er = _lowMem[_erY + e];
              if ((br2 == er && std::abs(bc2 - ec) <= r) || (bc2 == ec && std::abs(br2 - er) <= r)) covered = true;
            }
            if (covered) continue;
            nearby                    = true;
            const uint8_t hd          = _lowMem[_erAI + e];
            const bool    approaching = (hd == 1 && ex < px) || (hd == 3 && ex > px) || (hd == 4 && ey < py) || (hd == 2 && ey > py);
            if (approaching && useful == false) useful = true;
          }
        }
        // Placement-timing freedom (user, 2026-07-28): A is offered as a BRANCH whenever the
        // placement is RELEVANT -- useful (chain-head reach / approaching enemy), defensive
        // (enemy close, any heading), or path-opening (un-threatened brick in blast) -- never
        // FORCED exclusively. The search chooses WHEN to place among all frames where the
        // condition holds, instead of being locked to the earliest possible frame. Relevance
        // gating (incl. marginal coverage vs existing bombs + hazard veto) is unchanged.
        if ((useful || nearby || brickAny) && hazardV == false) allowedInputSet.push_back(_inputA);
        // otherwise: A not offered at all
      }
      else
        allowedInputSet.push_back(_inputA);
    }
  }

  __INLINE__ uint64_t getStateMoveHash() const { return 0; }
  void                printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Current Step:                     %04u\n", _currentStep);
    jaffarCommon::logger::log("[J+]  + Level:                            %02u\n", *_level);
    jaffarCommon::logger::log("[J+]  + Player Tile / Pixel:              (%2u,%2u) / (%2u,%2u)  World: (%3u,%3u)\n", *_playerTileX, *_playerTileY, *_playerPixelX, *_playerPixelY,
                              _playerPosX, _playerPosY);
    jaffarCommon::logger::log("[J+]  + Enemies Left ($9C) / Alive (table): %02u / %02u  slots: [%u %u %u %u %u %u %u]\n", *_enemiesLeft, _enemiesAlive, _lowMem[_erST + 0],
                              _lowMem[_erST + 1], _lowMem[_erST + 2], _lowMem[_erST + 3], _lowMem[_erST + 4], _lowMem[_erST + 5], _lowMem[_erST + 6]);
    jaffarCommon::logger::log("[J+]  + Cumulative Enemies Killed ($9E):    %u\n", _lowMem[0x009E]);
    {
      char          line[128];
      int           n   = snprintf(line, sizeof(line), "[J+]  + Bombs [Pn=till-blast X=expl np un]:");
      const uint8_t cap = (uint8_t)(_lowMem[0x0074] + 1);
      for (uint8_t i = 0; i < 10; i++)
      {
        if (n >= (int)sizeof(line) - 8) break;
        if (_lowMem[0x03A0 + i] != 0)
          n += snprintf(line + n, sizeof(line) - n, " P%u", _lowMem[0x03D2 + i] <= 160 ? 160u - _lowMem[0x03D2 + i] : 0u);
        else if (_bombShadow[i] != 0)
          n += snprintf(line + n, sizeof(line) - n, " X");
        else
          n += snprintf(line + n, sizeof(line) - n, " %s", i < cap ? "np" : "un");
      }
      jaffarCommon::logger::log("%s\n", line);
    }
    jaffarCommon::logger::log("[J+]  + Exit:                             (%2u,%2u) Revealed: %s\n", _exitTileX, _exitTileY, _exitRevealed ? "True" : "False");
    jaffarCommon::logger::log("[J+]  + Powerup:                          (%2u,%2u)\n", _powerupTileX, _powerupTileY);
    jaffarCommon::logger::log("[J+]  + Bricks Left:                      %02u\n", _bricksLeft);
    jaffarCommon::logger::log("[J+]  + Time Left:                        %03u\n", *_timeLeft);
    jaffarCommon::logger::log("[J+]  + Lives:                            %02u\n", *_lives);
    jaffarCommon::logger::log("[J+]  + Bombs / Radius / Detonator:       %02u / %02u / %02u\n", *_bombMax, *_bombRadius, *_hasDetonator);
    jaffarCommon::logger::log("[J+]  + Game End / Dying:                 %02u / %02u\n", *_gameEndStatus, *_dyingFlag);
    jaffarCommon::logger::log("[J+]  + Path Dist (Enemy/Powerup/Exit):   %.1f / %.1f / %.1f\n", _pathDistEnemy, _pathDistPowerup, _pathDistExit);
    jaffarCommon::logger::log("[J+]  + Pending (Bricks/Kills/Hazard):    %.3f / %.3f / %.3f\n", _pendingBricks, _pendingKills, _pendingHazard);
    jaffarCommon::logger::log("[J+]  + Frame Counter (phase):            %03u (%u) Input Frame: %s\n", *_frameCounter, *_frameCounter & 3,
                              _nextFrameAcceptsInput ? "True" : "False");
    jaffarCommon::logger::log("[J+]  + RNG State:                        %02X %02X %02X %02X\n", *_rngState1, *_rngState2, *_rngState3, *_rngState4);
    // Tile map render
    jaffarCommon::logger::log("[J+]  + Map [P=player E=enemy x=brick B=bomb *=flames C/H=powerup chain c/h=exit chain U/u=powerup D/d=door]:\n");
    for (uint8_t r = 0; r < _mapRows; r++)
    {
      char line[_mapCols + 1];
      for (uint8_t c = 0; c < _mapCols; c++)
      {
        const uint8_t t    = _lowMem[_mapBase + r * _mapStride + c];
        const char    sym  = (t == 0)                   ? '.'
                             : (t == 1)                 ? '#'
                             : (t == 2)                 ? 'x'
                             : (t == 3)                 ? 'B'
                             : (t == _tileExitBrick)    ? 'D'
                             : (t == _tilePowerupBrick) ? 'U'
                             : (t == _tilePowerup)      ? 'u'
                             : (t == _tileExit)         ? 'd'
                                                        : '*';
        char          osym = sym;
        if (_chainComputed && (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick))
          for (uint8_t ci = 0; ci < _chainLen; ci++)
            if (_chainCells[ci] == (uint16_t)(r * _mapCols + c)) osym = (c == _chainHeadX && r == _chainHeadY) ? 'H' : 'C';
        if (_chainComputed && (t == 2 || t == _tileExitBrick || t == _tilePowerupBrick))
          for (uint8_t ci = 0; ci < _exitChainLen; ci++)
            if (_exitChainCells[ci] == (uint16_t)(r * _mapCols + c)) osym = (c == _exitChainHeadX && r == _exitChainHeadY) ? 'h' : 'c';
        // live flames '*' (the map tile shows the crumbling brick during the burn, hiding the blast)
        for (uint8_t fl = 0; fl < 10; fl++)
          if (_lowMem[0x042C + fl] != 0 && _lowMem[0x047C + fl] == c && _lowMem[0x04CC + fl] == r) osym = '*';
        // live entities on top: enemies 'E', player 'P'
        for (uint8_t en = 0; en < _erN; en++)
          if (_lowMem[_erST + en] < 32 && _lowMem[_erX + en] != 0 && _lowMem[_erX + en] == c && _lowMem[_erY + en] == r) osym = 'E';
        if (_trackedTileX == c && _trackedTileY == r) osym = 'P';
        line[c] = osym;
      }
      line[_mapCols] = '\0';
      jaffarCommon::logger::log("[J+]    %s\n", line);
    }
    if (std::abs(_playerPosXMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos X Magnet               Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _playerPosXMagnet.intensity, _playerPosXMagnet.pos,
                                _playerDistanceToPointX);
    if (std::abs(_playerPosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos Y Magnet               Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _playerPosYMagnet.intensity, _playerPosYMagnet.pos,
                                _playerDistanceToPointY);
    if (std::abs(_exitMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Exit Magnet                       Intensity: %.5f, Distance: %.5f\n", _exitMagnet, _playerDistanceToExit);
    // Per-source reward breakdown (mirrors calculateGameSpecificReward exactly)
    {
      char plan[256];
      plan[0] = '\0';
      int pn  = 0;
      for (uint8_t ci = 0; ci < _chainLen && pn < 230; ci++) pn += snprintf(plan + pn, sizeof(plan) - pn, "(%u,%u) ", _chainCells[ci] % _mapCols, _chainCells[ci] / _mapCols);
      jaffarCommon::logger::log("[J+]  + Powerup chain: %s| %u/%u left, head (%2u,%2u) dist %.1f\n", plan, _chainRemaining, _chainLen, _chainHeadX, _chainHeadY, _chainHeadDist);
      char eplan[256];
      eplan[0] = '\0';
      int epn  = 0;
      for (uint8_t ci = 0; ci < _exitChainLen && epn < 230; ci++)
        epn += snprintf(eplan + epn, sizeof(eplan) - epn, "(%u,%u) ", _exitChainCells[ci] % _mapCols, _exitChainCells[ci] / _mapCols);
      jaffarCommon::logger::log("[J+]  + Exit chain: %s| %u/%u left, head (%2u,%2u) dist %.1f\n", eplan, _exitChainRemaining, _exitChainLen, _exitChainHeadX, _exitChainHeadY,
                                _exitChainHeadDist);
    }
    jaffarCommon::logger::log("[J+]  + Reward Breakdown:\n");
    jaffarCommon::logger::log("[J+]    + Kills ladder:      %+.1f (Enemies Left %u x %.0f)\n", -1.0 * _enemiesLeftMagnet * (float)*_enemiesLeft, *_enemiesLeft, _enemiesLeftMagnet);
    jaffarCommon::logger::log("[J+]    + Covered-enemy credit: %+.1f (%u covered x 1500)\n", 1500.0f * (float)_coveredEnemies, _coveredEnemies);
    {
      jaffarCommon::logger::log("[J+]    + Enemies killed:    %+.1f earned (%u killed x %.0f)\n", (float)_enemiesKilledCount * _enemiesKilledMagnet, _enemiesKilledCount,
                                _enemiesKilledMagnet);
    }
    jaffarCommon::logger::log(
        "[J+]    + Powerup progress:  %+.1f = chain bricks %+.1f (%u/%u broken x %.0f) + crumbling %+.1f + approach %+.1f (headDist %.1f, antGain %.1f, fuse %.2f)\n",
        _chainBrickReward * ((float)(_chainLen - _chainRemaining) + _chainCrumble) - _chainHeadMagnet * (_chainHeadDist - _bombMaxProgress * _chainHeadGain),
        _chainBrickReward * (float)(_chainLen - _chainRemaining), (unsigned)(_chainLen - _chainRemaining), _chainLen, _chainBrickReward, _chainBrickReward * _chainCrumble,
        -1.0 * _chainHeadMagnet * (_chainHeadDist - _bombMaxProgress * _chainHeadGain), _chainHeadDist, _chainHeadGain, _bombMaxProgress);
    jaffarCommon::logger::log(
        "[J+]    + Exit progress:     %+.1f = chain bricks %+.1f (%u/%u broken x %.0f) + crumbling %+.1f + approach %+.1f (headDist %.1f, antGain %.1f, fuse %.2f)%s\n",
        _exitChainBrickReward * ((float)(_exitChainLen - _exitChainRemaining) + _exitChainCrumble) -
            _exitChainHeadMagnet * (_exitChainHeadDist - _bombMaxProgress * _exitChainHeadGain),
        _exitChainBrickReward * (float)(_exitChainLen - _exitChainRemaining), (unsigned)(_exitChainLen - _exitChainRemaining), _exitChainLen, _exitChainBrickReward,
        _exitChainBrickReward * _exitChainCrumble, -1.0 * _exitChainHeadMagnet * (_exitChainHeadDist - _bombMaxProgress * _exitChainHeadGain), _exitChainHeadDist,
        _exitChainHeadGain, _bombMaxProgress, _exitChainHeadMagnet == 0.0f ? " (off)" : "");
    jaffarCommon::logger::log("[J+]    + Bricks-broken ladder: %+.1f (%d broken x %.2f)\n",
                              _initialBricks > _bricksLeft ? _anyBrickLadderReward * (float)(_initialBricks - _bricksLeft) : 0.0f,
                              _initialBricks > _bricksLeft ? (int)(_initialBricks - _bricksLeft) : 0, _anyBrickLadderReward);
    jaffarCommon::logger::log("[J+]    + Enemy-path crumble: %+.1f (bricks on route: %u)\n", 1000.0f * _enemyPathCrumble,
                              (unsigned)std::count(std::begin(_enemyPathBricks), std::end(_enemyPathBricks), (uint8_t)1));
    jaffarCommon::logger::log("[J+]    + Next-enemy pull:   %+.1f (nextDist %.1f)\n",
                              _enemyNextDist >= 0.0f ? 0.5f * _closestEnemyMagnet * std::max(0.0f, 800.0f - _enemyNextDist) : 0.0f, _enemyNextDist);
    jaffarCommon::logger::log("[J+]    + Next-brick pull:   PU %+.1f (%s, nextDist %.1f) / Exit %+.1f (%s, nextDist %.1f)\n",
                              _chainNextDist >= 0.0f ? 0.2f * _chainHeadMagnet * std::max(0.0f, 800.0f - _chainNextDist) : 0.0f, _chainHeadCommitted ? "committed" : "idle",
                              _chainNextDist, _exitChainNextDist >= 0.0f ? 0.2f * _exitChainHeadMagnet * std::max(0.0f, 800.0f - _exitChainNextDist) : 0.0f,
                              _exitChainHeadCommitted ? "committed" : "idle", _exitChainNextDist);
    jaffarCommon::logger::log("[J+]    + Powerup grab bonus: %+.1f%s\n", _grabBonus, _grabBonus == 0.0f ? " (pays +100000 once Flame Count >= 2)" : "");
    jaffarCommon::logger::log("[J+]    + Enemy approach:     %+.1f (pathDist %.1f)\n", -1.0 * _closestEnemyMagnet * _pathDistEnemy, _pathDistEnemy);
    jaffarCommon::logger::log("[J+]    + Escape penalty:    %+.1f (danger %.2f)\n", -1.0 * _bombEscapeMagnet * _bombEscapeDanger, _bombEscapeDanger);
    jaffarCommon::logger::log("[J+]    + Head fuse:          %+.1f (progress %.3f x %.0f)\n", _headFuseMagnet * _headFuseProgress, _headFuseProgress, _headFuseMagnet);
    jaffarCommon::logger::log("[J+]    + Bomb-enemy:         %+.1f (dist %.0f x %.0f)\n", _bombEnemyDist < 1e9f ? _bombEnemyMagnet * std::max(0.0f, 320.0f - _bombEnemyDist) : 0.0f,
                              _bombEnemyDist < 1e9f ? _bombEnemyDist : -1.0f, _bombEnemyMagnet);
    jaffarCommon::logger::log("[J+]    + Pending kill/brick/hazard: %+.1f / %+.1f / %+.1f\n", _pendingKillMagnet * _pendingKills, _pendingBrickMagnet * _pendingBricks,
                              -1.0 * _pendingHazardMagnet * _pendingHazard);
    if (std::abs(_enemiesLeftMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Enemies Left Magnet               Intensity: %.5f\n", _enemiesLeftMagnet);
    if (std::abs(_closestEnemyMagnet) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Closest Enemy Magnet              Intensity: %.5f, Distance: %.5f (Sum: %.5f)\n", _closestEnemyMagnet, _closestEnemyDist, _sumEnemyDist);
  }
  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;
    if (actionType == "Set Player Pos X Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_playerPosXMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }
    if (actionType == "Set Player Pos Y Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_playerPosYMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }
    if (actionType == "Set Exit Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_exitMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Enemies Killed Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_enemiesKilledMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Bomb Enemy Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_bombEnemyMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Closest Enemy Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_closestEnemyMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Powerup Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_powerupMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Powerup Progress Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_powerupProgressMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Powerup Grab Bonus")
    {
      auto value = jaffarCommon::json::getNumber<float>(actionJs, "Value");
      rule.addAction([=, this]() { this->_grabBonus = value; });
      recognizedActionType = true;
    }

    if (actionType == "Set Exit Chain Magnet")
    {
      auto head  = jaffarCommon::json::getNumber<float>(actionJs, "Head Intensity");
      auto brick = jaffarCommon::json::getNumber<float>(actionJs, "Brick Reward");
      rule.addAction(
          [=, this]()
          {
            this->_exitChainHeadMagnet  = head;
            this->_exitChainBrickReward = brick;
          });
      recognizedActionType = true;
    }

    if (actionType == "Set Chain Magnet")
    {
      auto head  = jaffarCommon::json::getNumber<float>(actionJs, "Head Intensity");
      auto brick = jaffarCommon::json::getNumber<float>(actionJs, "Brick Reward");
      rule.addAction(
          [=, this]()
          {
            this->_chainHeadMagnet  = head;
            this->_chainBrickReward = brick;
          });
      recognizedActionType = true;
    }
    if (actionType == "Set Bomb Escape Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_bombEscapeMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Bomb Pending Magnets")
    {
      auto brick  = jaffarCommon::json::getNumber<float>(actionJs, "Brick Intensity");
      auto kill   = jaffarCommon::json::getNumber<float>(actionJs, "Kill Intensity");
      auto hazard = jaffarCommon::json::getNumber<float>(actionJs, "Hazard Intensity");
      // Optional (default 0 = archived-config behavior preserved): dedicated inverse-time-to-
      // detonation gradient for a ticking bomb whose blast covers the CURRENT powerup-chain head
      // (pre-pickup). Pays Intensity * elapsed/160, so lineages that placed the head bomb earlier
      // strictly dominate all fuse-wait wandering -- the generic Brick Intensity gradient
      // (intensity/160 per frame) is too weak to order the wait against position-magnet noise.
      float headFuse = 0.0f;
      if (actionJs.contains("Head Fuse Intensity")) headFuse = jaffarCommon::json::getNumber<float>(actionJs, "Head Fuse Intensity");
      rule.addAction(
          [=, this]()
          {
            this->_pendingBrickMagnet  = brick;
            this->_pendingKillMagnet   = kill;
            this->_pendingHazardMagnet = hazard;
            this->_headFuseMagnet      = headFuse;
          });
      recognizedActionType = true;
    }
    if (actionType == "Set Enemies Left Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_enemiesLeftMagnet = intensity; });
      recognizedActionType = true;
    }
    return recognizedActionType;
  }
  // Datatype to describe a point magnet
  struct pointMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    float pos       = 0.0; // What is the point of attraction
  };
  pointMagnet_t          _playerPosXMagnet;
  pointMagnet_t          _playerPosYMagnet;
  float                  _playerDistanceToPointX;
  float                  _playerDistanceToPointY;
  float                  _exitMagnet;
  float                  _enemiesLeftMagnet;
  float                  _closestEnemyMagnet;
  float                  _powerupMagnet;
  float                  _powerupProgressMagnet;
  float                  _pendingBrickMagnet;
  float                  _headFuseMagnet;
  float                  _bombEnemyMagnet;
  float                  _enemiesKilledMagnet;
  uint8_t                _enemiesKilledCount = 0; // table-based dead-entry count (robust vs glitchy $9C)
  float                  _bombEnemyDist;          // min px distance ticking-bomb -> alive enemy (1e9 = no ticking bomb)
  float                  _headFuseProgress;       // max elapsed/160 among ticking bombs covering the powerup-chain head (0 in at-will mode)
  float                  _pendingKillMagnet;
  float                  _pendingHazardMagnet;
  float                  _bombEscapeMagnet;
  float                  _bombEscapeDanger;
  float                  _playerDistanceToExit;
  InputSet::inputIndex_t _lastInput;
  InputSet::inputIndex_t _nullInputIdx;
  uint8_t*               _lowMem;
  uint8_t*               _playerTileX;
  uint8_t*               _playerTileY;
  uint8_t*               _playerPixelX;
  uint8_t*               _playerPixelY;
  uint8_t*               _level;
  uint8_t*               _lives;
  uint8_t*               _bombRadius;
  uint8_t*               _bombMax;
  uint8_t*               _hasDetonator;
  uint8_t*               _timeLeft;
  uint8_t*               _enemiesLeft;
  uint8_t*               _frameCounter;
  uint8_t*               _rngState1;
  uint8_t*               _rngState2;
  uint8_t*               _rngState3;
  uint8_t*               _rngState4;
  uint16_t               _playerPosX;
  uint16_t               _playerPosY;
  uint8_t                _exitTileX;
  uint8_t                _exitTileY;
  bool                   _exitRevealed;
  uint8_t                _powerupTileX;
  uint8_t                _powerupTileY;
  uint8_t                _bricksLeft;
  bool                   _nextFrameAcceptsInput;
  float                  _closestEnemyDist;
  float                  _sumEnemyDist;
  float                  _pathDistEnemy;
  float                  _pathDistPowerup;
  float                  _pathDistExit;
  float                  _pendingBricks;
  float                  _pendingKills;
  float                  _pendingHazard;
  uint8_t                _flameCount;
  bool                   _powerupPresent;
  uint8_t                _powerupProgress;
  uint8_t*               _gameEndStatus;
  uint8_t*               _dyingFlag;
  InputSet::inputIndex_t _inputU;
  InputSet::inputIndex_t _inputD;
  InputSet::inputIndex_t _inputL;
  InputSet::inputIndex_t _inputR;
  InputSet::inputIndex_t _inputA;
  InputSet::inputIndex_t _inputB;
  InputSet::inputIndex_t _inputUB;
  InputSet::inputIndex_t _inputDB;
  InputSet::inputIndex_t _inputLB;
  InputSet::inputIndex_t _inputRB;
  uint8_t                _trackedTileX;
  uint8_t                _trackedTileY;
  uint8_t                _trackedPixX;
  uint8_t                _trackedPixY;
  uint8_t                _trackedMismatch;
  uint8_t                _bombLastX[10]; // last tile of a bomb per slot (for post-detonation attribution)
  uint8_t                _bombLastY[10];
  uint8_t                _bombShadow[10];                        // 1 while slot's flames are still burning after the slot freed
  uint8_t                _bombWasTicking[10];                    // for detonation-frame edge detection
  bool                   _caughtInBlast;                         // latched: player inside a blast cross at detonation (pre-Flamepass survival exploit = dead end)
  bool                   _blastTrapped;                          // derived: Detonator held + whole reachable region inside ticking crosses = permanent zombie
  uint8_t                _coveredEnemies;                        // derived: alive non-burning enemies inside ticking-bomb crosses (Detonator credit)
  bool                   _bonusMode;                             // config: bonus-stage kill-frenzy mode (invincible; free A/B; kill-count reward)
  bool                   _powerupStatBombs;                      // config: stage's power-up is Bombs
  bool                   _powerupStatDetonator;                  // config: stage's power-up is the Detonator ($77; B detonates at will)
  bool                   _powerupStatSpeed;                      // config: stage's power-up is Speed ($75; stage 4 only, 2x walk speed)
  bool                   _powerupStatBombpass;                   // config: stage's power-up is Bombpass ($78; walk through bombs)
  bool                   _powerupStatWallpass;                   // config: stage's power-up is Wallpass ($76; walk through bricks)
  bool                   _powerupStatFlamepass;                  // config: stage's power-up is Flamepass ($79; immune to own blasts)
  uint8_t                _powerupBombsThreshold;                 // config: $74 value that counts as "grabbed" for the Bombs stat (carried count + 1)
  uint8_t                _powerupFlameThreshold;                 // config: flame count that counts as "grabbed" for the Flame stat (carried count + 1)
  uint8_t                _enemyRecBase;                          // config: first enemy record index (10 - rosterSize); default 3
  uint8_t                _erN;                                   // derived: enemy record count
  uint16_t               _erX, _erPX, _erY, _erPY, _erST, _erAI; // derived: enemy table base addresses
  bool                   _powerupObtained;                       // stage-appropriate grab signal (Flame>=2 or Bombs>=1)
  bool                   _bombPlacedInFlames;
  bool                   _forceImmediateBomb;
  bool                   _forceImmediateDetonation;
  float                  _anyBrickLadderReward;
  bool                   _disableInputRestrictions;
  InputSet::inputIndex_t _inputUA, _inputDA, _inputLA, _inputRA, _inputAB;
  InputSet::inputIndex_t _inputUAB, _inputDAB, _inputLAB, _inputRAB;
  // Composite directions (diagonals + opposing pairs), offered only with "Allow Composite Directions"
  InputSet::inputIndex_t                   _inputUL, _inputUR, _inputDL, _inputDR, _inputLR, _inputUD;
  bool                                     _allowCompositeDirections;
  bool                                     _coveredEnemiesHandled;
  bool                                     _replanChainsOnGrab;
  bool                                     _exitReplanned     = false; // grab-time exit-chain replan done (lineage state under the knob)
  bool                                     _allEnemiesCovered = false; // all alive non-burning enemies inside ticking crosses (knob-gated)
  bool                                     _isLoading         = false; // $0C == 0x10 (stage-load screen-off window)
  bool                                     _exitChainAlwaysActive;
  bool                                     _chainComputed = false;
  std::array<uint16_t, 64>                 _chainCells;
  uint8_t                                  _chainLen = 0;
  uint8_t                                  _chainRemaining;
  uint8_t                                  _chainHeadX;
  uint8_t                                  _chainHeadY;
  float                                    _chainHeadDist;
  float                                    _chainCrumble;              // crumbling chain bricks: (5+phase)/6 each, pays brickReward fraction
  float                                    _exitChainCrumble;          // same for the exit chain
  float                                    _chainNextDist;             // player-sourced dist to the NEXT chain cell while head committed (-1 = inactive)
  float                                    _exitChainNextDist;         // same for the exit chain
  float                                    _enemyNextDist;             // player-sourced dist to next un-covered enemy while a bomb ticks (-1 = inactive)
  uint8_t                                  _enemyPathBricks[13 * 32];  // bricks on the CURRENT optimal player->enemy path (kill phase, recomputed per frame)
  float                                    _enemyPathCrumble;          // burning enemy-path bricks, crumble-ramp weighted
  uint16_t                                 _initialBricks         = 0; // plain-brick count at stage load (process-constant)
  uint8_t                                  _enemyPathBricksBroken = 0; // monotone LADDER: route-bricks destroyed (counted at detonation while marked)
  bool                                     _chainHeadCommitted;        // head threatened by ticking bomb or crumbling
  bool                                     _chainHeadThreat;           // head covered by a ticking bomb, crumble not yet started (credit flag)
  bool                                     _exitChainHeadThreat;       // exit-chain analog of _chainHeadThreat
  bool                                     _exitChainHeadCommitted;    // same for the exit chain
  float                                    _chainHeadMagnet;
  float                                    _chainBrickReward;
  float                                    _grabBonus;
  uint8_t                                  _enemiesAlive;
  std::array<uint16_t, 64>                 _exitChainCells;
  uint8_t                                  _exitChainLen = 0;
  uint8_t                                  _exitChainRemaining;
  uint8_t                                  _exitChainHeadX;
  uint8_t                                  _exitChainHeadY;
  float                                    _exitChainHeadDist;
  float                                    _exitChainHeadGain;
  float                                    _exitChainHeadMagnet;
  float                                    _exitChainBrickReward;
  float                                    _pathGainPowerup;
  float                                    _pathGainExit;
  float                                    _bombMaxProgress;
  float                                    _chainHeadGain;
  std::array<uint8_t, _mapRows * _mapCols> _threatOpen;
  uint32_t                                 _currentStep;
};
} // namespace nes
} // namespace games
} // namespace jaffarPlus
