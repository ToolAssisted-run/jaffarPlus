#pragma once

// A test game played on the TestEmulator's grid. The cursor must reach a goal cell.
// It exposes a handful of properties of different datatypes and is driven entirely by
// script rules (win / fail / checkpoint / reward / save-solution), so it exercises the
// full JaffarPlus main execution path with a known, provably-optimal solution.

#include "testEmulator/testEmulator.hpp"
#include <cstdlib>
#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>

namespace jaffarPlus
{

namespace games
{

namespace test
{

class GridWalker final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "Test / GridWalker"; }

  GridWalker(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    _goalX = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Goal X");
    _goalY = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Goal Y");

    // All recognized game-configuration keys have now been consumed; reject any leftover (unrecognized) key.
  }

private:
  __INLINE__ void registerGameProperties() override
  {
    // Mapping the cursor position exposed by the emulator
    auto position = (uint8_t*)_emulator->getProperty("Position").pointer;
    _posX         = &position[0];
    _posY         = &position[1];

    registerGameProperty("Pos X", &position[0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Pos Y", &position[1], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Distance", &_distance, Property::datatype_t::dt_int32, Property::endianness_t::little);
    registerGameProperty("Steps", &_steps, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Goal Reached", &_goalReached, Property::datatype_t::dt_bool, Property::endianness_t::little);
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    _emulator->advanceState(input);
    _steps++;
  }

  // Derived values recomputed after every state change (and after deserialization)
  __INLINE__ void stateUpdatePostHook() override
  {
    const int dx = (int)*_posX - (int)_goalX;
    const int dy = (int)*_posY - (int)_goalY;
    _distance    = std::abs(dx) + std::abs(dy);
    _goalReached = (_distance == 0);
  }

  // Only the cursor position (in emulator state) is part of the dedup key, so different
  // routes that reach the same cell collapse to one state and BFS yields a shortest path.
  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override {}

  // Discriminating hash for candidate-input testing: keyed on the cursor position so each
  // candidate input is probed once per cell rather than once for the whole search.
  __INLINE__ jaffarCommon::hash::hash_t getStateInputHash() override
  {
    const uint8_t pos[2] = {*_posX, *_posY};
    return jaffarCommon::hash::calculateMetroHash(pos, sizeof(pos));
  }

  // Direct state hash used when "Bypass Hash Calculation" is enabled: the runner takes this value
  // verbatim instead of running its MetroHash pass over the registered Hash Properties. It must be a
  // faithful dedup key, so -- like the normal hash -- it is keyed on the cursor position; otherwise
  // distinct cells would collapse into one state and BFS would not return a shortest path.
  bool hasDirectStateHash() const override { return true; }

  __INLINE__ jaffarCommon::hash::hash_t getDirectStateHash() const override
  {
    const uint8_t pos[2] = {*_posX, *_posY};
    return jaffarCommon::hash::calculateMetroHash(pos, sizeof(pos));
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_steps, sizeof(_steps));

    // When the emulator's own state serialization is bypassed, the game must persist the cursor
    // position itself (it lives in emulator memory, reached through _posX/_posY).
    if (_bypassEmulatorState)
    {
      serializer.push(_posX, sizeof(uint8_t));
      serializer.push(_posY, sizeof(uint8_t));
    }
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(&_steps, sizeof(_steps));

    if (_bypassEmulatorState)
    {
      deserializer.pop(_posX, sizeof(uint8_t));
      deserializer.pop(_posY, sizeof(uint8_t));
    }
  }

  __INLINE__ float calculateGameSpecificReward() const override
  {
    // Smaller distance to the goal is better
    return -(float)_distance;
  }

  __INLINE__ void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Goal:        (%u, %u)\n", _goalX, _goalY);
    jaffarCommon::logger::log("[J+]  + Distance:    %d\n", _distance);
    jaffarCommon::logger::log("[J+]  + Steps:       %u\n", _steps);
    jaffarCommon::logger::log("[J+]  + Goal Reached: %s\n", _goalReached ? "True" : "False");
  }

  // No game-specific rule actions: everything is covered by the base action types
  __INLINE__ bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override { return false; }

  // Pointers into the emulator's position memory
  uint8_t* _posX = nullptr;
  uint8_t* _posY = nullptr;

  // Goal cell (from config)
  uint8_t _goalX = 0;
  uint8_t _goalY = 0;

  // Game-owned derived state
  int32_t  _distance    = 0;
  uint16_t _steps       = 0;
  bool     _goalReached = false;
};

} // namespace test

} // namespace games

} // namespace jaffarPlus
