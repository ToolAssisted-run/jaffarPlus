#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>
#include <quickerSDLPoP2/quickerSDLPoP2.hpp>

namespace jaffarPlus
{

namespace games
{

namespace sdlpop2
{

// Rooms 1..32 (the level format has room numbers up to 32; their tiles are at level + (room - 1) * 30)
const size_t roomCount = 32;

/**
 * Prince of Persia 2: The Shadow and the Flame (DOS 1.0), through the QuickerSDLPoP2 emulator.
 *
 * Game Configuration (all optional):
 *   "Hash Tile Modifiers"   [[room, tile], ...] (1-based) tiles whose modifier goes into the state hash (a gate's
 *                           height, a loose floor's state...). Modifiers are not hashed otherwise: torches and other
 *                           animations change them every tick.
 *   "Hash Random Seed"      true: the random seed goes into the state hash (guards and some traps use it)
 *   "Hash Clock"            true: the minutes and ticks left go into the state hash
 */
class PrinceOfPersia2 final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "SDLPoP2 / Prince of Persia 2"; }

  PrinceOfPersia2(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    if (_gameConfigRemaining.contains("Hash Tile Modifiers"))
    {
      const auto tiles = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Hash Tile Modifiers");
      for (const auto& t : tiles) _hashTileModifiers.push_back({t[0].get<int>(), t[1].get<int>() - 1});
    }
    _hashRandomSeed = _gameConfigRemaining.contains("Hash Random Seed") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Random Seed");
    _hashClock      = _gameConfigRemaining.contains("Hash Clock") && jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Clock");
  }

private:
  // The tiles of a room (1..32) and their modifiers, as the game addresses them
  __INLINE__ uint8_t* roomTiles(const size_t room) const { return (uint8_t*)&_s->level + (room - 1) * 30; }
  __INLINE__ uint32_t* roomModifiers(const size_t room) const { return (uint32_t*)((uint8_t*)&_s->level + 0x348 + room * 0x78); }

  void registerCharacter(const std::string& name, quicker::char_type* c)
  {
    using dt = Property::datatype_t;
    const auto le = Property::endianness_t::little;
    registerGameProperty(name + " Pos X", &c->x, dt::dt_int16, le);
    registerGameProperty(name + " Pos Y", &c->y, dt::dt_int16, le);
    registerGameProperty(name + " Frame", &c->frame, dt::dt_uint16, le);
    registerGameProperty(name + " Direction", &c->direction, dt::dt_int8, le);
    registerGameProperty(name + " Current Col", &c->curr_col, dt::dt_int8, le);
    registerGameProperty(name + " Current Row", &c->curr_row, dt::dt_int8, le);
    registerGameProperty(name + " Action", &c->action, dt::dt_uint8, le);
    registerGameProperty(name + " Fall Speed X", &c->fall_x, dt::dt_int8, le);
    registerGameProperty(name + " Fall Speed Y", &c->fall_y, dt::dt_int8, le);
    registerGameProperty(name + " Room", &c->room, dt::dt_uint8, le);
    registerGameProperty(name + " Char Id", &c->charid, dt::dt_uint8, le);
    registerGameProperty(name + " Is Alive", &c->alive, dt::dt_int8, le);
    registerGameProperty(name + " Current HP", &c->f12, dt::dt_uint8, le);
    registerGameProperty(name + " Max HP", &c->f13, dt::dt_uint8, le);
    registerGameProperty(name + " Sequence", &c->seq_id, dt::dt_uint16, le);
  }

  __INLINE__ void registerGameProperties() override
  {
    using dt = Property::datatype_t;
    const auto le = Property::endianness_t::little;

    // The core's savestate, in place (it changes as the emulator's state does)
    _s = (quicker::QuickerSDLPoP2State*)_emulator->getProperty("Game State").pointer;

    registerGameProperty("Current Level", &_s->level_number, dt::dt_uint8, le);
    registerGameProperty("Next Level", &_s->counter_5cec, dt::dt_uint16, le);
    registerGameProperty("Drawn Room", &_s->drawn_room, dt::dt_uint8, le);
    registerGameProperty("Minutes Left", &_s->minutes_left, dt::dt_uint16, le);
    registerGameProperty("Clock Ticks", &_s->clock_ticks, dt::dt_uint16, le);
    registerGameProperty("Random Seed", &_s->random_seed, dt::dt_uint32, le);
    registerGameProperty("Tick", &_s->tick, dt::dt_uint32, le);
    registerGameProperty("Sword Type", &_s->byte_5cba, dt::dt_uint8, le);
    registerGameProperty("Restart Requested", &_s->word_5cd8, dt::dt_uint16, le);
    registerGameProperty("Moving Object Count", &_s->mob_count, dt::dt_uint16, le);
    registerGameProperty("Tile Animation Count", &_s->trob_count, dt::dt_uint16, le);

    registerCharacter("Player", &_s->Kid);
    registerCharacter("Opponent", &_s->Opp);
    for (size_t i = 0; i < 5; i++) registerCharacter("Character[" + std::to_string(i) + "]", &_s->chars[i]);

    // Tiles and their modifiers: "Tile[room][tile]", "Tile Modifier[room][tile]" (1-based)
    for (size_t r = 1; r <= roomCount; r++)
      for (size_t t = 1; t <= 30; t++)
      {
        const auto idx = "[" + std::to_string(r) + "][" + std::to_string(t) + "]";
        registerGameProperty("Tile" + idx, &roomTiles(r)[t - 1], dt::dt_uint8, le);
        registerGameProperty("Tile Modifier" + idx, &roomModifiers(r)[t - 1], dt::dt_uint32, le);
      }

    // Falling floors and other moving objects
    for (size_t i = 0; i < 30; i++)
    {
      const auto idx = "Moving Object[" + std::to_string(i) + "]";
      registerGameProperty(idx + " Pos X", &_s->mobs[i].x, dt::dt_int16, le);
      registerGameProperty(idx + " Pos Y", &_s->mobs[i].y, dt::dt_int16, le);
      registerGameProperty(idx + " Room", &_s->mobs[i].room, dt::dt_uint8, le);
      registerGameProperty(idx + " Type", &_s->mobs[i].type, dt::dt_uint8, le);
      registerGameProperty(idx + " Row", &_s->mobs[i].row, dt::dt_uint8, le);
    }

    // Tile animations (gates, buttons, loose floors, ...)
    for (size_t i = 0; i < 20; i++)
    {
      const auto idx = "Tile Animation[" + std::to_string(i) + "]";
      registerGameProperty(idx + " Tile", &_s->trobs[i].tilepos, dt::dt_int8, le);
      registerGameProperty(idx + " Room", &_s->trobs[i].room, dt::dt_uint8, le);
      registerGameProperty(idx + " State", &_s->trobs[i].state, dt::dt_uint8, le);
      registerGameProperty(idx + " Type", &_s->trobs[i].tile, dt::dt_uint8, le);
    }

    _nullInputIdx = _emulator->registerInput("|.|......|");
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override { _emulator->advanceState(input); }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    hashEngine.Update(_s->level_number);
    hashEngine.Update(_s->counter_5cec);
    hashEngine.Update(_s->drawn_room);
    hashEngine.Update(_s->word_5cd8);
    hashEngine.Update(_s->byte_5cba);
    hashEngine.Update(_s->Kid);
    hashEngine.Update(_s->Opp);
    hashEngine.Update(_s->chars);
    hashEngine.Update(_s->mob_count);
    hashEngine.Update(_s->mobs);
    hashEngine.Update(_s->trob_count);
    hashEngine.Update(_s->trobs);
    for (size_t r = 1; r <= roomCount; r++) hashEngine.Update(roomTiles(r), 30);
    for (const auto& t : _hashTileModifiers) hashEngine.Update(roomModifiers(t.first)[t.second]);
    if (_hashRandomSeed) hashEngine.Update(_s->random_seed);
    if (_hashClock)
    {
      hashEngine.Update(_s->minutes_left);
      hashEngine.Update(_s->clock_ticks);
    }
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    _playerPosXMagnet.intensity = 0.0;
    _playerPosYMagnet.intensity = 0.0;
    _playerDirectionMagnet      = 0.0;
    _opponentHPMagnet           = 0.0;
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override {}
  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) override {}

  __INLINE__ float calculateGameSpecificReward() const override
  {
    float reward = 0.0;
    reward += _playerPosXMagnet.intensity * -std::abs((float)_playerPosXMagnet.position - (float)_s->Kid.x);
    reward += _playerPosYMagnet.intensity * -std::abs((float)_playerPosYMagnet.position - (float)_s->Kid.y);
    reward += (_s->Kid.direction == 0 ? 1.0f : -1.0f) * _playerDirectionMagnet;
    reward += (float)((int)_s->Opp.f13 - (int)_s->Opp.f12) * _opponentHPMagnet;
    return reward;
  }

  void printInfoImpl() const override
  {
    const auto& k = _s->Kid;
    jaffarCommon::logger::log("[J+]  + Level / Next Level:   %2u / %2u\n", _s->level_number, _s->counter_5cec);
    jaffarCommon::logger::log("[J+]  + Time Left:            %u minutes, %u ticks\n", _s->minutes_left, _s->clock_ticks);
    jaffarCommon::logger::log("[J+]  + [Prince]              Room: %u, Pos: %d, %d, Row: %d, Col: %d, Frame: 0x%X, Action: %u, Dir: %d, HP: %u/%u, Alive: %d\n", k.room, k.x,
                              k.y, k.curr_row, k.curr_col, k.frame, k.action, k.direction, k.f12, k.f13, k.alive);
    const auto& o = _s->Opp;
    if (o.room != 0)
      jaffarCommon::logger::log("[J+]  + [Opponent]            Room: %u, Pos: %d, %d, Frame: 0x%X, Char Id: %u, HP: %u/%u, Alive: %d\n", o.room, o.x, o.y, o.frame, o.charid, o.f12,
                                o.f13, o.alive);
    jaffarCommon::logger::log("[J+]  + Random Seed:          0x%08X\n", _s->random_seed);
    for (size_t i = 0; i < _s->mob_count && i < 30; i++)
      jaffarCommon::logger::log("[J+]  + Moving Object %lu:      Room: %u, Pos: %d, %d, Type: %u, Row: %u\n", i, _s->mobs[i].room, _s->mobs[i].x, _s->mobs[i].y, _s->mobs[i].type,
                                _s->mobs[i].row);
    for (size_t i = 0; i < _s->trob_count && i < 20; i++)
      jaffarCommon::logger::log("[J+]  + Tile Animation %lu:     Room: %u, Tile: %d, Type: 0x%02X, State: %u\n", i, _s->trobs[i].room, _s->trobs[i].tilepos + 1, _s->trobs[i].tile,
                                _s->trobs[i].state);
    if (std::abs(_playerPosXMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos X Magnet   - Intensity: %.1f, Center: %d\n", _playerPosXMagnet.intensity, _playerPosXMagnet.position);
    if (std::abs(_playerPosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Player Pos Y Magnet   - Intensity: %.1f, Center: %d\n", _playerPosYMagnet.intensity, _playerPosYMagnet.position);
    if (std::abs(_playerDirectionMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Player Direction Magnet - Intensity: %.1f\n", _playerDirectionMagnet);
    if (std::abs(_opponentHPMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Opponent HP Magnet    - Intensity: %.1f\n", _opponentHPMagnet);
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    // "Set Player Pos X Magnet" / "Set Player Pos Y Magnet": Intensity, Position, Room (the magnet only acts in that room)
    if (actionType == "Set Player Pos X Magnet" || actionType == "Set Player Pos Y Magnet")
    {
      auto  intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto  position  = jaffarCommon::json::getNumber<int16_t>(actionJs, "Position");
      auto  room      = jaffarCommon::json::getNumber<uint8_t>(actionJs, "Room");
      auto& magnet    = actionType == "Set Player Pos X Magnet" ? _playerPosXMagnet : _playerPosYMagnet;
      rule.addAction([=, this, &magnet]() { magnet = pointMagnet_t{.intensity = _s->Kid.room == room ? intensity : 0.0f, .position = position}; });
      return true;
    }
    if (actionType == "Set Player Direction Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _playerDirectionMagnet = intensity; });
      return true;
    }
    if (actionType == "Set Opponent HP Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { _opponentHPMagnet = intensity; });
      return true;
    }
    return false;
  }

  __INLINE__ jaffarCommon::hash::hash_t getStateInputHash() override { return {0, _s->Kid.frame}; }

  struct pointMagnet_t
  {
    float   intensity = 0.0;
    int16_t position  = 0;
  };

  quicker::QuickerSDLPoP2State* _s;

  std::vector<std::pair<size_t, size_t>> _hashTileModifiers;
  bool                                   _hashRandomSeed = false;
  bool                                   _hashClock      = false;

  pointMagnet_t _playerPosXMagnet;
  pointMagnet_t _playerPosYMagnet;
  float         _playerDirectionMagnet = 0.0;
  float         _opponentHPMagnet      = 0.0;

  InputSet::inputIndex_t _nullInputIdx;
};

} // namespace sdlpop2

} // namespace games

} // namespace jaffarPlus
