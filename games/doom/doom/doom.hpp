#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <r_defs.h>

// Players information
extern "C" void                        P_SetThingPosition(mobj_t* thing, int refind);
extern "C" __STORAGE_MODIFIER int      enableOutput;
extern "C" __STORAGE_MODIFIER player_t players[MAX_MAXPLAYERS];
extern "C" __STORAGE_MODIFIER int      preventLevelExit;
extern "C" __STORAGE_MODIFIER int      preventGameEnd;
extern "C" __STORAGE_MODIFIER int      reachedLevelExit;
extern "C" __STORAGE_MODIFIER int      reachedGameEnd;
extern "C" __STORAGE_MODIFIER int      gamemap;
extern "C" __STORAGE_MODIFIER int      gametic;
extern "C" __STORAGE_MODIFIER int      numlines;
extern "C" __STORAGE_MODIFIER line_t*  lines;

namespace jaffarPlus
{

namespace games
{

namespace doom
{

#define __JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE 1024 * 1024 * 32
class Doom final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "Doom"; }

  Doom(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    _dummyBuffer = (uint8_t*)malloc(__JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE);

    _pointMagnet.intensity = 0;
    _pointMagnet.x         = 0;
    _pointMagnet.y         = 0;
    _pointMagnet.z         = 0;

    _playerPosX              = 0;
    _playerPosY              = 0;
    _playerPosZ              = 0;
    _playerMomentumX         = 0;
    _playerMomentumY         = 0;
    _playerMomentumZ         = 0;
    _player1DistanceToPointX = 0;
    _player1DistanceToPointY = 0;
    _player1DistanceToPointZ = 0;
    _player1DistanceToPoint  = 0;
    _momentumMagnet          = 0;

    _currentStep = 0;

    // Getting emulator name (for runtime use)
    _traceFilePath = jaffarCommon::json::popString(_gameConfigRemaining, "Trace File Path");

    // Loading trace
    if (_traceFilePath != "")
    {
      _useTrace = true;
      std::string traceData;
      bool        status = jaffarCommon::file::loadStringFromFile(traceData, _traceFilePath.c_str());
      if (status == false && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr) JAFFAR_THROW_LOGIC("Could not find/read trace file: %s\n", _traceFilePath.c_str());

      std::istringstream f(traceData);
      std::string        line;
      while (std::getline(f, line))
      {
        auto  coordinates = jaffarCommon::string::split(line, ' ');
        float x           = std::atof(coordinates[0].c_str());
        float y           = std::atof(coordinates[1].c_str());
        _trace.push_back(traceEntry_t{.x = x, .y = y});
      }
    }

    // Configurable dedup-hash quantization. Defaults reproduce full precision (no bucketing)
    // so existing configs behave exactly as before. posFracBits / momFracBits = number of
    // 16.16 fractional bits KEPT (0 = whole map units, 16 = raw); angleBits = number of high
    // bits of the 32-bit BAM angle KEPT (e.g. 8 -> 256 directions, 32 = raw).
    int posFracBits = _gameConfigRemaining.contains("Hash Pos Frac Bits") ? jaffarCommon::json::popNumber<int>(_gameConfigRemaining, "Hash Pos Frac Bits") : 16;
    int momFracBits = _gameConfigRemaining.contains("Hash Mom Frac Bits") ? jaffarCommon::json::popNumber<int>(_gameConfigRemaining, "Hash Mom Frac Bits") : 16;
    int angleBits   = _gameConfigRemaining.contains("Hash Angle Bits") ? jaffarCommon::json::popNumber<int>(_gameConfigRemaining, "Hash Angle Bits") : 32;
    _hashIncludeZ   = _gameConfigRemaining.contains("Hash Include Z") ? jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Include Z") : true;
    posFracBits     = std::max(0, std::min(16, posFracBits));
    momFracBits     = std::max(0, std::min(16, momFracBits));
    angleBits       = std::max(1, std::min(32, angleBits));
    _hashPosShift   = 16 - posFracBits;
    _hashMomShift   = 16 - momFracBits;
    _hashAngleShift = 32 - angleBits;

    // Getting index for a non input
    _nullInputIdx = _emulator->registerInput("||   0,   0,   0,   0,...|");

    // All recognized game-configuration keys have now been consumed; reject any leftover (unrecognized) key.
  }

  ~Doom() { free(_dummyBuffer); }

private:
  __INLINE__ void registerGameProperties() override
  {
    // Registering native game properties
    registerGameProperty("Current Step", &_currentStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Player Pos X", &_playerPosX, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Pos Y", &_playerPosY, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Pos Z", &_playerPosZ, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Momentum X", &_playerMomentumX, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Momentum Y", &_playerMomentumY, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Player Momentum Z", &_playerMomentumZ, Property::datatype_t::dt_float32, Property::endianness_t::little);

    for (int i = 0; i < numlines; i++)
    {
      std::string propertyName = std::string("Line[") + std::to_string(lines[i].iLineID) + std::string("] Activations");
      registerGameProperty(propertyName, &lines[i].player_activations, Property::datatype_t::dt_int32, Property::endianness_t::little);
    }
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Running emulator
    _emulator->advanceState(input);

    // Advancing current step
    _currentStep++;

    // Storing current input
    _currentInputIdx = input;
  }

  bool hasDirectStateHash() const override { return true; }

  __INLINE__ jaffarCommon::hash::hash_t getDirectStateHash() const override
  {
    // Serializing current state
    // jaffarCommon::serializer::Contiguous s(_dummyBuffer, __JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE);
    // _emulator->serializeState(s);

    // Advancing state once
    // _emulator->advanceState(_currentInputIdx);

    // Now calculating hash
    jaffarCommon::hash::hash_t value = {0, 0};
    if (players[0].mo != nullptr)
    {
      value.first |= ((uint64_t)((uint16_t)(players[0].mo->x >> FRACBITS))) << 0;
      value.first |= ((uint64_t)((uint16_t)(players[0].mo->y >> FRACBITS))) << 16;
      value.first |= ((uint64_t)((uint16_t)(players[0].mo->momx >> FRACBITS))) << 32;
      value.first |= ((uint64_t)((uint16_t)(players[0].mo->momy >> FRACBITS))) << 48;
      value.second |= ((uint64_t)((uint16_t)(players[0].mo->angle >> FRACBITS))) << 0;
    }

    // Restoring state
    // jaffarCommon::deserializer::Contiguous d(_dummyBuffer, __JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE);
    // _emulator->deserializeState(d);

    return value;
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    // hash.Update(reachedLevelExit);
    // hash.Update(reachedGameEnd);
    // hashEngine.Update(gamemap);
    // hashEngine.Update(gametic);

    // if (players[0].mo != nullptr)
    // {
    //   hashEngine.Update(players[0].mo->x >> FRACBITS);
    //   hashEngine.Update(players[0].mo->y >> FRACBITS);
    //   hashEngine.Update(players[0].mo->angle >> FRACBITS);
    //   hashEngine.Update(players[0].mo->momx >> FRACBITS);
    //   hashEngine.Update(players[0].mo->momy >> FRACBITS);
    // }

    // Serializing Rcurrent state
    // jaffarCommon::serializer::Contiguous s(_dummyBuffer, __JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE);
    // _emulator->serializeState(s);

    // Advancing state once
    // _emulator->advanceState(_currentInputIdx);

    // Quantized player-state dedup hash. Per-field granularity is configurable (see the
    // constructor knobs); defaults reproduce full 16.16 / 32-bit precision so configs that
    // don't set the knobs behave exactly as before. Coarser buckets collapse the per-step
    // input fan-out (many near-identical successors -> one bucket); the risk is that
    // over-coarse POSITION can merge frame-distinct states near the exit line and, because
    // dedup is first-seen-wins, discard the faster one.
    if (players[0].mo != nullptr)
    {
      hashEngine.Update((uint32_t)(players[0].mo->x >> _hashPosShift));
      hashEngine.Update((uint32_t)(players[0].mo->y >> _hashPosShift));
      if (_hashIncludeZ) hashEngine.Update((uint32_t)(players[0].mo->z >> _hashPosShift));

      hashEngine.Update((uint32_t)((uint32_t)players[0].mo->angle >> _hashAngleShift));

      hashEngine.Update((int32_t)(players[0].mo->momx >> _hashMomShift));
      hashEngine.Update((int32_t)(players[0].mo->momy >> _hashMomShift));
      if (_hashIncludeZ) hashEngine.Update((int32_t)(players[0].mo->momz >> _hashMomShift));
    }

    // Restoring state
    // jaffarCommon::deserializer::Contiguous d(_dummyBuffer, __JAFFAR_PLUS_DOOM_DUMMY_BUFFER_SIZE);
    // _emulator->deserializeState(d);
  }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    _effectiveStateSize = ((jaffarPlus::emulator::QuickerDSDA*)_emulator.get())->getEffectiveStateSize();

    _playerPosX = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->x);
    _playerPosY = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->y);
    _playerPosZ = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->z);

    _playerMomentumX = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->momx);
    _playerMomentumY = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->momy);
    _playerMomentumZ = jaffar::EmuInstance::getFloatFrom1616Fixed(players[0].mo->momz);

    _totalMomentum = sqrt(_playerMomentumX * _playerMomentumX + _playerMomentumY * _playerMomentumY + _playerMomentumZ * _playerMomentumZ);
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    // Resetting magnets ahead of rule re-evaluation
    _pointMagnet.intensity = 0.0;
    _pointMagnet.x         = 0.0;
    _pointMagnet.y         = 0.0;
    _pointMagnet.z         = 0.0;

    _traceMagnet.intensity = 0.0;
    _traceMagnet.offset    = 0;

    _momentumMagnet  = 0.0;
    _momentumXMagnet = 0.0;
    _momentumYMagnet = 0.0;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    // Updating distance to user-defined point
    _player1DistanceToPointX = std::abs(_pointMagnet.x - _playerPosX);
    _player1DistanceToPointY = std::abs(_pointMagnet.y - _playerPosY);
    _player1DistanceToPointZ = std::abs(_pointMagnet.z - _playerPosZ);
    _player1DistanceToPoint =
        sqrtf(_player1DistanceToPointX * _player1DistanceToPointX + _player1DistanceToPointY * _player1DistanceToPointY + _player1DistanceToPointZ * _player1DistanceToPointZ);

    // Updating trace stuff
    if (_useTrace == true)
    {
      _traceStep    = (uint16_t)std::max(std::min((int)_currentStep + _traceMagnet.offset, (int)_trace.size() - 1), 0);
      _traceTargetX = _trace[_traceStep].x;
      _traceTargetY = _trace[_traceStep].y;

      // Updating distance to trace point
      _traceDistanceX = std::abs(_traceTargetX - _playerPosX);
      _traceDistanceY = std::abs(_traceTargetY - _playerPosY);
      _traceDistance  = sqrtf(_traceDistanceX * _traceDistanceX + _traceDistanceY * _traceDistanceY);
    }
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_effectiveStateSize, sizeof(_effectiveStateSize));
    serializer.push(&_currentStep, sizeof(_currentStep));
    serializer.push(&_currentInputIdx, sizeof(_currentInputIdx));

    // serializer.push(&gametic, sizeof(gametic));
    // serializer.push(&players[0].mo->x, sizeof(players[0].mo->x));
    // serializer.push(&players[0].mo->y, sizeof(players[0].mo->y));
    // serializer.push(&players[0].mo->z, sizeof(players[0].mo->z));
    // serializer.push(&players[0].mo->angle, sizeof(players[0].mo->angle));
    // serializer.push(&players[0].mo->momx, sizeof(players[0].mo->momx));
    // serializer.push(&players[0].mo->momy, sizeof(players[0].mo->momy));
    // serializer.push(&players[0].mo->momz, sizeof(players[0].mo->momz));

    // serializer.push(&players[0].mo->sprite, sizeof(players[0].mo->sprite));
    // serializer.push(&players[0].mo->frame, sizeof(players[0].mo->frame));
    // serializer.push(&players[0].mo->floorz, sizeof(players[0].mo->floorz));
    // serializer.push(&players[0].mo->ceilingz, sizeof(players[0].mo->ceilingz));
    // serializer.push(&players[0].mo->dropoffz, sizeof(players[0].mo->dropoffz));
    // serializer.push(&players[0].mo->radius, sizeof(players[0].mo->radius));
    // serializer.push(&players[0].mo->height, sizeof(players[0].mo->height));
    // serializer.push(&players[0].mo->validcount, sizeof(players[0].mo->validcount));
    // serializer.push(&players[0].mo->type, sizeof(players[0].mo->type));
    // serializer.push(&players[0].mo->tics, sizeof(players[0].mo->tics));
    // serializer.push(&players[0].mo->flags, sizeof(players[0].mo->flags));
    // serializer.push(&players[0].mo->intflags, sizeof(players[0].mo->intflags));
    // serializer.push(&players[0].mo->health, sizeof(players[0].mo->health));
    // serializer.push(&players[0].mo->movedir, sizeof(players[0].mo->movedir));
    // serializer.push(&players[0].mo->movecount, sizeof(players[0].mo->movecount));
    // serializer.push(&players[0].mo->strafecount, sizeof(players[0].mo->strafecount));
    // serializer.push(&players[0].mo->reactiontime, sizeof(players[0].mo->reactiontime));
    // serializer.push(&players[0].mo->threshold, sizeof(players[0].mo->threshold));
    // serializer.push(&players[0].mo->pursuecount, sizeof(players[0].mo->pursuecount));
    // serializer.push(&players[0].mo->gear, sizeof(players[0].mo->gear));
    // serializer.push(&players[0].mo->lastlook, sizeof(players[0].mo->lastlook));
    // serializer.push(&players[0].mo->friction, sizeof(players[0].mo->friction));
    // serializer.push(&players[0].mo->movefactor, sizeof(players[0].mo->movefactor));
    // serializer.push(&players[0].mo->pitch, sizeof(players[0].mo->pitch));
    // serializer.push(&players[0].mo->index, sizeof(players[0].mo->index));
    // serializer.push(&players[0].mo->patch_width, sizeof(players[0].mo->patch_width));
    // serializer.push(&players[0].mo->iden_nums, sizeof(players[0].mo->iden_nums));
    // serializer.push(&players[0].mo->gravity, sizeof(players[0].mo->gravity));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.pop(&_effectiveStateSize, sizeof(_effectiveStateSize));
    deserializer.pop(&_currentStep, sizeof(_currentStep));
    deserializer.pop(&_currentInputIdx, sizeof(_currentInputIdx));

    // deserializer.pop(&gametic, sizeof(gametic));
    // deserializer.pop(&players[0].mo->x, sizeof(players[0].mo->x));
    // deserializer.pop(&players[0].mo->y, sizeof(players[0].mo->y));
    // deserializer.pop(&players[0].mo->z, sizeof(players[0].mo->z));
    // deserializer.pop(&players[0].mo->angle, sizeof(players[0].mo->angle));
    // deserializer.pop(&players[0].mo->momx, sizeof(players[0].mo->momx));
    // deserializer.pop(&players[0].mo->momy, sizeof(players[0].mo->momy));
    // deserializer.pop(&players[0].mo->momz, sizeof(players[0].mo->momz));

    // deserializer.pop(&players[0].mo->sprite, sizeof(players[0].mo->sprite));
    // deserializer.pop(&players[0].mo->frame, sizeof(players[0].mo->frame));
    // deserializer.pop(&players[0].mo->floorz, sizeof(players[0].mo->floorz));
    // deserializer.pop(&players[0].mo->ceilingz, sizeof(players[0].mo->ceilingz));
    // deserializer.pop(&players[0].mo->dropoffz, sizeof(players[0].mo->dropoffz));
    // deserializer.pop(&players[0].mo->radius, sizeof(players[0].mo->radius));
    // deserializer.pop(&players[0].mo->height, sizeof(players[0].mo->height));
    // deserializer.pop(&players[0].mo->validcount, sizeof(players[0].mo->validcount));
    // deserializer.pop(&players[0].mo->type, sizeof(players[0].mo->type));
    // deserializer.pop(&players[0].mo->tics, sizeof(players[0].mo->tics));
    // deserializer.pop(&players[0].mo->flags, sizeof(players[0].mo->flags));
    // deserializer.pop(&players[0].mo->intflags, sizeof(players[0].mo->intflags));
    // deserializer.pop(&players[0].mo->health, sizeof(players[0].mo->health));
    // deserializer.pop(&players[0].mo->movedir, sizeof(players[0].mo->movedir));
    // deserializer.pop(&players[0].mo->movecount, sizeof(players[0].mo->movecount));
    // deserializer.pop(&players[0].mo->strafecount, sizeof(players[0].mo->strafecount));
    // deserializer.pop(&players[0].mo->reactiontime, sizeof(players[0].mo->reactiontime));
    // deserializer.pop(&players[0].mo->threshold, sizeof(players[0].mo->threshold));
    // deserializer.pop(&players[0].mo->pursuecount, sizeof(players[0].mo->pursuecount));
    // deserializer.pop(&players[0].mo->gear, sizeof(players[0].mo->gear));
    // deserializer.pop(&players[0].mo->lastlook, sizeof(players[0].mo->lastlook));
    // deserializer.pop(&players[0].mo->friction, sizeof(players[0].mo->friction));
    // deserializer.pop(&players[0].mo->movefactor, sizeof(players[0].mo->movefactor));
    // deserializer.pop(&players[0].mo->pitch, sizeof(players[0].mo->pitch));
    // deserializer.pop(&players[0].mo->index, sizeof(players[0].mo->index));
    // deserializer.pop(&players[0].mo->patch_width, sizeof(players[0].mo->patch_width));
    // deserializer.pop(&players[0].mo->iden_nums, sizeof(players[0].mo->iden_nums));
    // deserializer.pop(&players[0].mo->gravity, sizeof(players[0].mo->gravity));
  }

  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Reward advancing always
    if (std::abs(_pointMagnet.intensity) > 0.0f) reward += -1.0 * _pointMagnet.intensity * _player1DistanceToPoint;

    // Reward total momentum
    if (std::abs(_momentumMagnet) > 0.0f) reward += _totalMomentum * _momentumMagnet;

    // Reward X/Y momentum
    if (std::abs(_momentumXMagnet) > 0.0f) reward += _playerMomentumX * _momentumXMagnet;
    if (std::abs(_momentumYMagnet) > 0.0f) reward += _playerMomentumY * _momentumYMagnet;

    // If trace is used, compute its magnet's effect
    if (_useTrace == true) reward += -1.0 * _traceMagnet.intensity * _traceDistance;

    // Returning reward
    return reward;
  }

  void printInfoImpl() const override
  {
    char mapName[512];
    headlessGetMapName(mapName);
    jaffarCommon::logger::log("[J+] Map:        %s\n", mapName);
    jaffarCommon::logger::log("[J+] Game Tic:   %d\n", gametic);
    jaffarCommon::logger::log("[J+] Level Exit: %s\n", reachedLevelExit == 1 ? "Yes" : "No");
    jaffarCommon::logger::log("[J+] Game End:   %s\n", reachedGameEnd == 1 ? "Yes" : "No");
    jaffarCommon::logger::log("[J+] Effective State Size:   %lu bytes\n", _effectiveStateSize);

    if (players[0].mo != nullptr)
    {
      jaffarCommon::logger::log("[J+] Player 1 Coordinates:    (%f, %f, %f)\n", _playerPosX, _playerPosY, _playerPosZ);
      jaffarCommon::logger::log("[J+] Player 1 Angle:           %u (0x%08X)\n", players[0].mo->angle, players[0].mo->angle);
      jaffarCommon::logger::log("[J+] Player 1 Momentum:        %f (%f, %f, %f)\n", _totalMomentum, _playerMomentumX, _playerMomentumY, _playerMomentumZ);
      jaffarCommon::logger::log("[J+] Player 1 Health:          %d\n", players[0].mo->health);
    }

    // jaffarCommon::logger::log("[] Line Information: \n");
    // for (int i = 0; i < numlines; i++)
    // {
    //   jaffarCommon::logger::log("[] Line %3d: Flags: %d, Activations: %d\n", lines[i].iLineID, lines[i].flags, lines[i].player_activations);
    // }

    if (std::abs(_momentumMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Momentum Magnet                          Intensity: %.5f\n", _momentumMagnet);
    if (std::abs(_momentumXMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Momentum X Magnet                        Intensity: %.5f\n", _momentumXMagnet);
    if (std::abs(_momentumYMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Momentum Y Magnet                        Intensity: %.5f\n", _momentumYMagnet);

    if (std::abs(_pointMagnet.intensity) > 0.0f)
    {
      jaffarCommon::logger::log("[J+]  + Point Magnet                             Intensity: %.5f, X: %3.3f, Y: %3.3f, Z: %3.3f\n", _pointMagnet.intensity, _pointMagnet.x,
                                _pointMagnet.y, _pointMagnet.z);
      jaffarCommon::logger::log("[J+]    + Distance X                             %3.3f\n", _player1DistanceToPointX);
      jaffarCommon::logger::log("[J+]    + Distance Y                             %3.3f\n", _player1DistanceToPointY);
      jaffarCommon::logger::log("[J+]    + Distance Z                             %3.3f\n", _player1DistanceToPointZ);
      jaffarCommon::logger::log("[J+]    + Total Distance                         %3.3f\n", _player1DistanceToPoint);
    }

    if (_useTrace == true)
    {
      if (std::abs(_traceMagnet.intensity) > 0.0f)
      {
        jaffarCommon::logger::log("[J+]  + Trace Magnet                             Intensity: %.5f, Step: %u, X: %3.3f, Y: %3.3f\n", _traceMagnet.intensity, _traceStep,
                                  _traceTargetX, _traceTargetY);
        jaffarCommon::logger::log("[J+]    + Distance X                             %3.3f\n", _traceDistanceX);
        jaffarCommon::logger::log("[J+]    + Distance Y                             %3.3f\n", _traceDistanceY);
        jaffarCommon::logger::log("[J+]    + Total Distance                         %3.3f\n", _traceDistance);
      }
    }
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;

    if (actionType == "Set Trace Magnet")
    {
      if (_useTrace == false) JAFFAR_THROW_LOGIC("Specified Trace Magnet, but no trace file was provided.");
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto offset    = jaffarCommon::json::getNumber<int>(actionJs, "Offset");
      rule.addAction([=, this]() { this->_traceMagnet = traceMagnet_t{.intensity = intensity, .offset = offset}; });
      recognizedActionType = true;
    }

    if (actionType == "Set Point Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto x         = jaffarCommon::json::getNumber<float>(actionJs, "X");
      auto y         = jaffarCommon::json::getNumber<float>(actionJs, "Y");
      auto z         = jaffarCommon::json::getNumber<float>(actionJs, "Z");
      rule.addAction([=, this]() { this->_pointMagnet = pointMagnet_t{.intensity = intensity, .x = x, .y = y, .z = z}; });
      recognizedActionType = true;
    }

    if (actionType == "Set Momentum Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_momentumMagnet = intensity; });
      recognizedActionType = true;
    }

    if (actionType == "Set Momentum X Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_momentumXMagnet = intensity; });
      recognizedActionType = true;
    }

    if (actionType == "Set Momentum Y Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_momentumYMagnet = intensity; });
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
    float z         = 0.0; // What is the y point of attraction
  };

  struct traceEntry_t
  {
    float x;
    float y;
  };

  // Datatype to describe a point magnet
  struct traceMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    int   offset    = 0;   // Which entry (step) to look at wrt the current emulation step
  };

  traceMagnet_t _traceMagnet;

  bool        _isDumpingTrace = false;
  std::string _traceDumpString;

  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override {}

  __INLINE__ void playerPrintCommands() const override { jaffarCommon::logger::log("[J+] t: start/stop trace dumping (%s)\n", _isDumpingTrace ? "On" : "Off"); };

  __INLINE__ bool playerParseCommand(const int command)
  {
    // If storing a trace, do it here
    if (_isDumpingTrace == true) _traceDumpString += std::to_string(_playerPosX) + std::string(" ") + std::to_string(_playerPosY) + std::string("\n");

    if (command == 't')
    {
      if (_isDumpingTrace == false)
      {
        _isDumpingTrace  = true;
        _traceDumpString = "";
        return false;
      }
      else
      {
        const std::string dumpOutputFile = "jaffar.trace";
        jaffarCommon::logger::log("[J+] Dumping trace to file: '%s'", dumpOutputFile.c_str());
        jaffarCommon::file::saveStringToFile(_traceDumpString, dumpOutputFile);
        _isDumpingTrace = false;
        return true;
      }
    }

    return false;
  };

  // Derivative properties

  pointMagnet_t _pointMagnet;
  float         _momentumMagnet;
  float         _momentumXMagnet;
  float         _momentumYMagnet;
  float         _totalMomentum;
  size_t        _effectiveStateSize;

  float _playerPosX;
  float _playerPosY;
  float _playerPosZ;
  float _playerMomentumX;
  float _playerMomentumY;
  float _playerMomentumZ;

  float _player1DistanceToPointX;
  float _player1DistanceToPointY;
  float _player1DistanceToPointZ;
  float _player1DistanceToPoint;

  size_t _maxStateSize;

  // Dedup-hash quantization shifts (precomputed in constructor from the config knobs)
  int  _hashPosShift   = 0;
  int  _hashMomShift   = 0;
  int  _hashAngleShift = 0;
  bool _hashIncludeZ   = true;

  uint16_t _currentStep = 0;

  // Whether we use a trace
  bool _useTrace = false;

  // Location of the trace file
  std::string _traceFilePath;

  // Trace contents
  std::vector<traceEntry_t> _trace;

  // Current trace target
  uint16_t _traceStep;
  float    _traceTargetX;
  float    _traceTargetY;
  float    _traceDistanceX;
  float    _traceDistanceY;
  float    _traceDistance;

  // Space for dummy save state buffer for extra hash calculation
  uint8_t*               _dummyBuffer;
  InputSet::inputIndex_t _nullInputIdx;
  InputSet::inputIndex_t _currentInputIdx;
};

} // namespace doom

} // namespace games

} // namespace jaffarPlus