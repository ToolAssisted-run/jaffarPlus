#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>

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

    // Camera X only matters during the level-start scroll-in (it gates the select scroll-skip);
    // during normal play it follows the player and only adds state noise.
    _hashCamera = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Camera");

    // Enemy 1/8-tile sub-offsets multiply the state space enormously (64^2 per guard). Tile
    // position + timer + facing usually give enough search diversity; disable for big levels.
    _hashEnemyOffsets = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Enemy Offsets");

    // Player sub-offsets: merging them (false) trades movement-timing precision for a ~64x
    // smaller state space -- useful to just complete a big level, not for frame optimization.
    _hashPlayerOffsets = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Player Offsets");

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
    registerGameProperty("Player Alive", &_lowMem[0x009A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Falling", &_lowMem[0x009B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Dig State", &_lowMem[0x00A0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player X", &_playerX, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Y", &_playerY, Property::datatype_t::dt_float32, Property::endianness_t::little);

    // Gold
    registerGameProperty("Gold Remaining", &_lowMem[0x0093], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Gold Collected", &_lowMem[0x00C4], Property::datatype_t::dt_uint8, Property::endianness_t::little);

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

    // Search bookkeeping
    registerGameProperty("Last Input Step", &_lastInputStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Current Step", &_currentStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);

    // Getting some properties' pointers now for quick access later
    _playerTileX   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Tile X")]->getPointer();
    _playerTileY   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Tile Y")]->getPointer();
    _playerOffsetX = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Offset X")]->getPointer();
    _playerOffsetY = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Offset Y")]->getPointer();
    _goldRemaining = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Gold Remaining")]->getPointer();

    // Getting index for a non input
    _nullInputIdx = _emulator->registerInput("|..|........|");
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Increasing counter if input is null
    if (input != _nullInputIdx) _lastInputStep = _currentStep;

    // Running emulator
    _emulator->advanceState(input);

    // Advancing current step
    _currentStep++;
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
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
    if (_hashTimers == true)
    {
      hashEngine.Update(_lowMem[0x0053]);
      hashEngine.Update(_lowMem[0x009E]);
    }

    // Enemies: tile X/Y, timers, facing (sub-offsets optional)
    hashEngine.Update(&_lowMem[0x0661], 3);
    hashEngine.Update(&_lowMem[0x0669], 3);
    hashEngine.Update(&_lowMem[0x0671], 3);
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
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    // Resetting magnets ahead of rule re-evaluation
    _pointMagnet.intensity = 0.0;
    _pointMagnet.x         = 0.0;
    _pointMagnet.y         = 0.0;
    _goldMagnet            = 0.0;
    _nearestChestMagnet    = 0.0;
    _guardCarryMagnet      = 0.0;
    _guardCarryFactor      = 0.0;
    _digProgressMagnet     = 0.0;
    _digProgressRadius     = 0.0;
    _stopProcessingReward  = false;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    // Updating distance to user-defined point
    _playerDistanceToPointX = std::abs((float)_pointMagnet.x - _playerX);
    _playerDistanceToPointY = std::abs((float)_pointMagnet.y - _playerY);
    _playerDistanceToPoint  = sqrtf(_playerDistanceToPointX * _playerDistanceToPointX + _playerDistanceToPointY * _playerDistanceToPointY);

    // Distance to the nearest gold chest still on the map (value 0x07 in the $0400 layout layer,
    // addressed base + Y*28 + X for Y in [1,13]). Guard-carried gold is not on the map.
    _nearestChestDistance = 0.0f;
    if (_nearestChestMagnet != 0.0f)
    {
      float best = -1.0f;
      for (size_t y = 1; y <= 13; y++)
        for (size_t x = 0; x < 28; x++)
          if (_lowMem[0x0400 + y * 28 + x] == 0x07)
          {
            const float d = std::abs((float)x - _playerX) + std::abs((float)y - _playerY);
            if (best < 0.0f || d < best) best = d;
          }
      if (best > 0.0f) _nearestChestDistance = best;
    }
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.pushContiguous(&_lastInputStep, sizeof(_lastInputStep));
    serializer.pushContiguous(&_currentStep, sizeof(_currentStep));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.popContiguous(&_lastInputStep, sizeof(_lastInputStep));
    deserializer.popContiguous(&_currentStep, sizeof(_currentStep));
  }

  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Subtracting reward for having made an input recently (for early termination)
    reward += _lastInputStepReward * _lastInputStep;

    // If this is a win state, then evaluate only w.r.t. how long since the last input
    if (_stopProcessingReward) return reward;

    // Distance to point magnet
    reward += -1.0 * _pointMagnet.intensity * _playerDistanceToPoint;

    // Gold magnet: reward for every chest already collected (gold gone from the level)
    reward += _goldMagnet * (float)(_initialGold - *_goldRemaining);

    // Nearest-chest magnet: pull toward the closest chest still on the map
    reward += -1.0 * _nearestChestMagnet * _nearestChestDistance;

    // Dig-progress magnet: reward each open dug hole near a remaining chest (shapes the long
    // no-feedback excavation chains needed to reach brick-entombed chests)
    if (_digProgressMagnet != 0.0f)
      for (size_t j = 0; j < 8; j++)
        if (_lowMem[0x06E0 + j] != 0)
        {
          const float hx = (float)_lowMem[0x06A0 + j], hy = (float)_lowMem[0x06C0 + j];
          for (size_t y = 1; y <= 13; y++)
            for (size_t x = 0; x < 28; x++)
              if (_lowMem[0x0400 + y * 28 + x] == 0x07 && std::abs((float)x - hx) + std::abs((float)y - hy) <= _digProgressRadius)
              {
                reward += _digProgressMagnet;
                y = 14;
                break; // one credit per hole
              }
        }

    // Guard-carry magnet: reward each guard holding gold (timer < 0), more when near the player
    // (a carrying guard approaching the player is a chest on its way to being deliverable)
    if (_guardCarryMagnet != 0.0f)
      for (size_t i = 0; i < 3; i++)
        if ((int8_t)_lowMem[0x0671 + i] < 0)
        {
          const float d = std::abs((float)_lowMem[0x0661 + i] - _playerX) + std::abs((float)_lowMem[0x0669 + i] - _playerY);
          reward += _guardCarryMagnet - _guardCarryFactor * d;
        }

    // Returning reward
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

    if (std::abs(_pointMagnet.intensity) > 0.0f)
    {
      jaffarCommon::logger::log("[J+]  + Point Magnet                             Intensity: %.5f, X: %3.3f, Y: %3.3f\n", _pointMagnet.intensity, _pointMagnet.x, _pointMagnet.y);
      jaffarCommon::logger::log("[J+]    + Total Distance                         %3.3f\n", _playerDistanceToPoint);
    }
    if (std::abs(_goldMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Gold Magnet                              Intensity: %.5f\n", _goldMagnet);
    if (std::abs(_nearestChestMagnet) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Nearest Chest Magnet                     Intensity: %.5f, Distance: %3.3f\n", _nearestChestMagnet, _nearestChestDistance);
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

    if (actionType == "Set Guard Carry Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto factor    = jaffarCommon::json::getNumber<float>(actionJs, "Distance Factor");
      rule.addAction(
          [=, this]()
          {
            this->_guardCarryMagnet = intensity;
            this->_guardCarryFactor = factor;
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
  float         _guardCarryMagnet   = 0.0;
  float         _guardCarryFactor   = 0.0;
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

  // Whether to hash the enemies' 1/8-tile sub-offsets (large state-count multiplier)
  bool _hashEnemyOffsets;

  // Whether to hash the player's 1/8-tile sub-offsets (movement-timing precision vs state count)
  bool _hashPlayerOffsets;

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
