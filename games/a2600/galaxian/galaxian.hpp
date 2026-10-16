#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>

#define ENEMY_ROWS 6
#define ENEMY_COLS 7

#define DIR_NONE 0
#define DIR_LEFT 1
#define DIR_RIGHT 2
#define DIR_STOP 3

namespace jaffarPlus
{

namespace games
{

namespace a2600
{

class Galaxian final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "A2600 / Galaxian"; }

  Galaxian(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config) {}

private:
  __INLINE__ void registerGameProperties() override
  {
    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("RAM").pointer;

    // Registering native game properties
    registerGameProperty("Score x 10", &_lowMem[0x2E], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Score x 100", &_lowMem[0x2D], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player Pos X", &_lowMem[0x64], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Bullet Active", &_lowMem[0x31], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Bullet Pos X", &_lowMem[0x3C], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Bullet Pos Y", &_lowMem[0x0B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 1", &_lowMem[0x2B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 2", &_lowMem[0x2A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 3", &_lowMem[0x29], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 4", &_lowMem[0x28], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 5", &_lowMem[0x27], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Enemy Row 6", &_lowMem[0x26], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Flyer Active", &_lowMem[0x57], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Current Stage", &_lowMem[0x2F], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Player State", &_lowMem[0x32], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Frame Type", &_lowMem[0x00], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Score", &_score, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Remaining Enemies", &_remainingEnemies, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Direction Chosen", &_directionChosen, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Getting some properties' pointers now for quick access later
    _score10      = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Score x 10")]->getPointer();
    _score100     = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Score x 100")]->getPointer();
    _playerPosX   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Player Pos X")]->getPointer();
    _bulletActive = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Bullet Active")]->getPointer();
    _bulletPosX   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Bullet Pos X")]->getPointer();
    _bulletPosY   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Bullet Pos Y")]->getPointer();

    _enemyRow1   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 1")]->getPointer();
    _enemyRow2   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 2")]->getPointer();
    _enemyRow3   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 3")]->getPointer();
    _enemyRow4   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 4")]->getPointer();
    _enemyRow5   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 5")]->getPointer();
    _enemyRow6   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Enemy Row 6")]->getPointer();
    _flyerActive = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Flyer Active")]->getPointer();

    _directionChosen = DIR_NONE;
    updateEnemyMatrix();

    _inputNone  = _emulator->registerInput("|.....|.....|");
    _inputLeft  = _emulator->registerInput("|.....|..L..|");
    _inputRight = _emulator->registerInput("|.....|...R.|");
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Running emulator
    _emulator->advanceState(input);

    if (*_bulletActive > 0 && _directionChosen == DIR_NONE && input == _inputLeft) _directionChosen = DIR_LEFT;
    if (*_bulletActive > 0 && _directionChosen == DIR_NONE && input == _inputRight) _directionChosen = DIR_RIGHT;
    if (*_bulletActive > 0 && _directionChosen != DIR_NONE && input == _inputNone) _directionChosen = DIR_STOP;
    if (*_bulletActive == 0) _directionChosen = DIR_NONE;
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override { hashEngine.Update(_lowMem, 0x80); }

  __INLINE__ void updateEnemyMatrix()
  {
    for (size_t i = 0; i < ENEMY_ROWS; i++)
      for (size_t j = 0; j < ENEMY_COLS; j++) _enemyMatrix[i][j] = 0;

    if (*_enemyRow1 & 0x01) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 1] = 1;
    if (*_enemyRow1 & 0x02) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 2] = 1;
    if (*_enemyRow1 & 0x04) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 3] = 1;
    if (*_enemyRow1 & 0x08) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 4] = 1;
    if (*_enemyRow1 & 0x10) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 5] = 1;
    if (*_enemyRow1 & 0x20) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 6] = 1;
    if (*_enemyRow1 & 0x40) _enemyMatrix[ENEMY_ROWS - 1][ENEMY_COLS - 7] = 1;

    if (*_enemyRow2 & 0x01) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 1] = 1;
    if (*_enemyRow2 & 0x02) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 2] = 1;
    if (*_enemyRow2 & 0x04) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 3] = 1;
    if (*_enemyRow2 & 0x08) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 4] = 1;
    if (*_enemyRow2 & 0x10) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 5] = 1;
    if (*_enemyRow2 & 0x20) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 6] = 1;
    if (*_enemyRow2 & 0x40) _enemyMatrix[ENEMY_ROWS - 2][ENEMY_COLS - 7] = 1;

    if (*_enemyRow3 & 0x01) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 1] = 1;
    if (*_enemyRow3 & 0x02) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 2] = 1;
    if (*_enemyRow3 & 0x04) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 3] = 1;
    if (*_enemyRow3 & 0x08) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 4] = 1;
    if (*_enemyRow3 & 0x10) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 5] = 1;
    if (*_enemyRow3 & 0x20) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 6] = 1;
    if (*_enemyRow3 & 0x40) _enemyMatrix[ENEMY_ROWS - 3][ENEMY_COLS - 7] = 1;

    if (*_enemyRow4 & 0x01) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 1] = 1;
    if (*_enemyRow4 & 0x02) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 2] = 1;
    if (*_enemyRow4 & 0x04) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 3] = 1;
    if (*_enemyRow4 & 0x08) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 4] = 1;
    if (*_enemyRow4 & 0x10) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 5] = 1;
    if (*_enemyRow4 & 0x20) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 6] = 1;
    if (*_enemyRow4 & 0x40) _enemyMatrix[ENEMY_ROWS - 4][ENEMY_COLS - 7] = 1;

    if (*_enemyRow5 & 0x01) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 1] = 1;
    if (*_enemyRow5 & 0x02) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 2] = 1;
    if (*_enemyRow5 & 0x04) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 3] = 1;
    if (*_enemyRow5 & 0x08) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 4] = 1;
    if (*_enemyRow5 & 0x10) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 5] = 1;
    if (*_enemyRow5 & 0x20) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 6] = 1;
    if (*_enemyRow5 & 0x40) _enemyMatrix[ENEMY_ROWS - 5][ENEMY_COLS - 7] = 1;

    if (*_enemyRow6 & 0x01) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 1] = 1;
    if (*_enemyRow6 & 0x02) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 2] = 1;
    if (*_enemyRow6 & 0x04) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 3] = 1;
    if (*_enemyRow6 & 0x08) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 4] = 1;
    if (*_enemyRow6 & 0x10) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 5] = 1;
    if (*_enemyRow6 & 0x20) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 6] = 1;
    if (*_enemyRow6 & 0x40) _enemyMatrix[ENEMY_ROWS - 6][ENEMY_COLS - 7] = 1;

    _remainingEnemies = 0;
    for (size_t i = 0; i < ENEMY_ROWS; i++)
      for (size_t j = 0; j < ENEMY_COLS; j++)
        if (_enemyMatrix[i][j] == 1) _remainingEnemies++;
    _remainingEnemies += *_flyerActive;
  }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    float score10  = ((float)*_score10) / 1.6;
    float score100 = (float)(*_score100 >= 10 ? *_score100 - 6.0 : *_score100);
    _score         = score100 * 100.0 + score10;

    updateEnemyMatrix();
  }

  __INLINE__ void ruleUpdatePreHook() override {}

  __INLINE__ void ruleUpdatePostHook() override {}

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_enemyMatrix, sizeof(_enemyMatrix));
    serializer.push(&_remainingEnemies, sizeof(_remainingEnemies));
    serializer.push(&_directionChosen, sizeof(_directionChosen));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.pop(&_enemyMatrix, sizeof(_enemyMatrix));
    deserializer.pop(&_remainingEnemies, sizeof(_remainingEnemies));
    deserializer.pop(&_directionChosen, sizeof(_directionChosen));
  }

  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Distance to point magnet
    // reward += 1000.0 * _score;

    reward -= _remainingEnemies * 1000.0;
    if (*_bulletActive > 0) reward += 200.0;
    if (*_bulletActive > 0) reward -= *_bulletPosY;

    // Returning reward
    return reward;
  }

  void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Score:                            %f (%u, %u)\n", _score, *_score100, *_score10);
    jaffarCommon::logger::log("[J+]  + Player Pos X:                     %u\n", *_playerPosX);
    jaffarCommon::logger::log("[J+]  + Bullet Active:                    %u\n", *_bulletActive);
    jaffarCommon::logger::log("[J+]  + Bullet Pos X:                     %u\n", *_bulletPosX);
    jaffarCommon::logger::log("[J+]  + Bullet Pos Y:                     %u\n", *_bulletPosY);

    jaffarCommon::logger::log("[J+]  + Flyer Active:                     %u\n", *_flyerActive);
    jaffarCommon::logger::log("[J+]  + Remaining Enemies:                %u\n", _remainingEnemies);
    jaffarCommon::logger::log("[J+]  + Direction Chosen:                 %u\n", _directionChosen);

    for (size_t i = 0; i < ENEMY_ROWS; i++)
    {
      jaffarCommon::logger::log("[J+]  + Row %lu:  ", i);
      for (size_t j = 0; j < ENEMY_COLS; j++) jaffarCommon::logger::log(" %1u", _enemyMatrix[i][j]);
      jaffarCommon::logger::log("\n");
    }
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;

    return recognizedActionType;
  }

  __INLINE__ jaffarCommon::hash::hash_t getStateInputHash() override
  {
    // There is no discriminating state element, so simply return a zero hash
    return jaffarCommon::hash::hash_t();
  }

  // Pointer to emulator's low memory storage
  uint8_t* _lowMem;

  float    _score;
  uint8_t* _score10;
  uint8_t* _score100;
  uint8_t* _playerPosX;
  uint8_t* _bulletActive;
  uint8_t* _bulletPosX;
  uint8_t* _bulletPosY;

  uint8_t* _enemyRow1;
  uint8_t* _enemyRow2;
  uint8_t* _enemyRow3;
  uint8_t* _enemyRow4;
  uint8_t* _enemyRow5;
  uint8_t* _enemyRow6;
  uint8_t* _flyerActive;

  uint8_t _enemyMatrix[ENEMY_ROWS][ENEMY_COLS];
  uint8_t _remainingEnemies;
  uint8_t _directionChosen;

  InputSet::inputIndex_t _inputNone;
  InputSet::inputIndex_t _inputLeft;
  InputSet::inputIndex_t _inputRight;
};

} // namespace a2600

} // namespace games

} // namespace jaffarPlus