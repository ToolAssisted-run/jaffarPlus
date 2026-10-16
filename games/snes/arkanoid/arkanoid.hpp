#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>

#define _ARKANOID_COLUMN_COUNT 10
#define _ARKANOID_ROW_COUNT 16
#define _ARKANOID_ROW_STRIDE 16

#define _ARKANOID_HELD_EXTEND_POWERUP 2
#define _ARKANOID_HELD_LASER_POWERUP 4

#define _ARKANOID_FALLING_M_POWERUP 3
#define _ARKANOID_FALLING_LASER_POWERUP 3
#define _ARKANOID_FALLING_EIGHTBALL_POWERUP 0

#define _ARKANOID_BALL_M_POWERUP 2

namespace jaffarPlus
{

namespace games
{

namespace snes
{

class Arkanoid final : public jaffarPlus::Game
{
public:
  enum target_t
  {
    nothing,
    tile,
    blocked
  };

  static __INLINE__ std::string getName() { return "SNES / Arkanoid"; }

  Arkanoid(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    // Parsing configuration
    _remainingHitsReward           = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Remaining Hits Reward");
    _remainingBlocksReward         = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Remaining Blocks Reward");
    _holdLaserPowerUpReward        = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Hold Laser Powerup Reward");
    _holdMPowerUpReward            = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Hold M Powerup Reward");
    _fallingLaserPowerUpReward     = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Falling Laser Powerup Reward");
    _fallingEightBallPowerUpReward = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Falling Eight Ball Powerup Reward");
    _ballCountReward               = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Ball Count Reward");
    _paddleMovementFrameskip       = jaffarCommon::json::popNumber<uint8_t>(_gameConfigRemaining, "Paddle Movement Frameskip");

    // All recognized game-configuration keys have now been consumed; reject any leftover (unrecognized) key.
  }

private:
  __INLINE__ void registerGameProperties() override
  {
    _inputNothing = _emulator->registerInput("|..|............|............|");
    _inputLeft    = _emulator->registerInput("|..|..L.........|............|");
    _inputRight   = _emulator->registerInput("|..|...R........|............|");
    _inputFire    = _emulator->registerInput("|..|.........A..|............|");

    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("RAM").pointer;

    // Registering native game properties

    registerGameProperty("Game Timer", &_lowMem[0x0172], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Current Round", &_lowMem[0x0154], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Paddle Pos X", &_lowMem[0x04D2], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Power Up 1 Active", &_lowMem[0x051A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Power Up 1 Active", &_lowMem[0x051A], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("PowerUp 1 Active", &_lowMem[0x051A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("PowerUp 1 Type", &_lowMem[0x051E], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("PowerUp 1 Pos X", &_lowMem[0x0525], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("PowerUp 1 Pos Y", &_lowMem[0x0529], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("PowerUp 2 Active", &_lowMem[0x0518], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("PowerUp 2 Type", &_lowMem[0x051C], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("PowerUp 2 Pos X", &_lowMem[0x0524], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("PowerUp 2 Pos Y", &_lowMem[0x0528], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Paddle PowerUp", &_lowMem[0x04F2], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Paddle Stickyness", &_lowMem[0x04EA], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball PowerUp", &_lowMem[0x02F8], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Ball 1 State", &_lowMem[0x01B0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 2 State", &_lowMem[0x01B2], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 3 State", &_lowMem[0x01B4], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 4 State", &_lowMem[0x01B6], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 5 State", &_lowMem[0x01B8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 6 State", &_lowMem[0x01BA], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 7 State", &_lowMem[0x01BC], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Ball 8 State", &_lowMem[0x01BE], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Ball 1 Pos X", &_lowMem[0x0270], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 2 Pos X", &_lowMem[0x0272], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 3 Pos X", &_lowMem[0x0274], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 4 Pos X", &_lowMem[0x0276], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 5 Pos X", &_lowMem[0x0278], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 6 Pos X", &_lowMem[0x027A], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 7 Pos X", &_lowMem[0x027C], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 8 Pos X", &_lowMem[0x027E], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Ball 1 Pos Y", &_lowMem[0x02B0], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 2 Pos Y", &_lowMem[0x02B2], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 3 Pos Y", &_lowMem[0x02B4], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 4 Pos Y", &_lowMem[0x02B6], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 5 Pos Y", &_lowMem[0x02B8], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 6 Pos Y", &_lowMem[0x02BA], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 7 Pos Y", &_lowMem[0x02BC], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Ball 8 Pos Y", &_lowMem[0x02BE], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Ball 1 Vel Y", &_lowMem[0x01F1], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 2 Vel Y", &_lowMem[0x01F3], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 3 Vel Y", &_lowMem[0x01F5], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 4 Vel Y", &_lowMem[0x01F7], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 5 Vel Y", &_lowMem[0x01F9], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 6 Vel Y", &_lowMem[0x01FB], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 7 Vel Y", &_lowMem[0x01FD], Property::datatype_t::dt_int8, Property::endianness_t::little);
    registerGameProperty("Ball 8 Vel Y", &_lowMem[0x01FF], Property::datatype_t::dt_int8, Property::endianness_t::little);

    registerGameProperty("Launch Counter", &_lowMem[0x04E2], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Remaining Blocks", &_remainingBlocks, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Remaining Hits", &_remainingHits, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Active Shots", &_activeShots, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Active Balls", &_activeBalls, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Shot 1 State", &_lowMem[0x059B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Shot 1 Pos X", &_lowMem[0x05AA], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Shot 1 Pos Y", &_lowMem[0x05B2], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Shot 2 State", &_lowMem[0x059D], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Shot 2 Pos X", &_lowMem[0x05AC], Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Shot 2 Pos Y", &_lowMem[0x05B4], Property::datatype_t::dt_uint16, Property::endianness_t::little);

    registerGameProperty("Boss Health 1", &_lowMem[0x0BF7], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Boss Health 2", &_lowMem[0x0901], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Boss 3 Arm 1 HP", &_lowMem[0x0C7F], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Boss 3 Arm 2 HP", &_lowMem[0x0C81], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Boss 3 Torso HP", &_lowMem[0x0C83], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Warp Open", &_lowMem[0x017A], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    _gameTimer    = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Game Timer")]->getPointer();
    _currentRound = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Current Round")]->getPointer();

    _paddlePowerUp    = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Paddle PowerUp")]->getPointer();
    _paddleStickyness = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Paddle Stickyness")]->getPointer();
    _ballPowerUp      = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball PowerUp")]->getPointer();

    _powerUp1Active = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 1 Active")]->getPointer();
    _powerUp1Type   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 1 Type")]->getPointer();
    _powerUp1PosX   = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 1 Pos X")]->getPointer();
    _powerUp1PosY   = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 1 Pos Y")]->getPointer();

    _powerUp2Active = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 2 Active")]->getPointer();
    _powerUp2Type   = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 2 Type")]->getPointer();
    _powerUp2PosX   = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 2 Pos X")]->getPointer();
    _powerUp2PosY   = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("PowerUp 2 Pos Y")]->getPointer();

    _ball1State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 1 State")]->getPointer();
    _ball2State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 2 State")]->getPointer();
    _ball3State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 3 State")]->getPointer();
    _ball4State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 4 State")]->getPointer();
    _ball5State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 5 State")]->getPointer();
    _ball6State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 6 State")]->getPointer();
    _ball7State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 7 State")]->getPointer();
    _ball8State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 8 State")]->getPointer();

    _ball1PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 1 Pos X")]->getPointer();
    _ball2PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 2 Pos X")]->getPointer();
    _ball3PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 3 Pos X")]->getPointer();
    _ball4PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 4 Pos X")]->getPointer();
    _ball5PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 5 Pos X")]->getPointer();
    _ball6PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 6 Pos X")]->getPointer();
    _ball7PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 7 Pos X")]->getPointer();
    _ball8PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 8 Pos X")]->getPointer();

    _ball1PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 1 Pos Y")]->getPointer();
    _ball2PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 2 Pos Y")]->getPointer();
    _ball3PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 3 Pos Y")]->getPointer();
    _ball4PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 4 Pos Y")]->getPointer();
    _ball5PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 5 Pos Y")]->getPointer();
    _ball6PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 6 Pos Y")]->getPointer();
    _ball7PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 7 Pos Y")]->getPointer();
    _ball8PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 8 Pos Y")]->getPointer();

    _ball1VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 1 Vel Y")]->getPointer();
    _ball2VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 2 Vel Y")]->getPointer();
    _ball3VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 3 Vel Y")]->getPointer();
    _ball4VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 4 Vel Y")]->getPointer();
    _ball5VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 5 Vel Y")]->getPointer();
    _ball6VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 6 Vel Y")]->getPointer();
    _ball7VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 7 Vel Y")]->getPointer();
    _ball8VelY = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Ball 8 Vel Y")]->getPointer();

    _shot1State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 1 State")]->getPointer();
    _shot2State = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 2 State")]->getPointer();

    _shot1PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 1 Pos X")]->getPointer();
    _shot2PosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 2 Pos X")]->getPointer();

    _shot1PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 1 Pos Y")]->getPointer();
    _shot2PosY = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Shot 2 Pos Y")]->getPointer();

    _paddlePosX = (uint16_t*)_propertyMap[jaffarCommon::hash::hashString("Paddle Pos X")]->getPointer();

    _launchCounter = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Launch Counter")]->getPointer();

    _bossHealth1 = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Boss Health 1")]->getPointer();
    _bossHealth2 = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Boss Health 2")]->getPointer();

    _boss3Arm1HP  = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Boss 3 Arm 1 HP")]->getPointer();
    _boss3Arm2HP  = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Boss 3 Arm 2 HP")]->getPointer();
    _boss3TorsoHP = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Boss 3 Torso HP")]->getPointer();

    _blocks = &_lowMem[0x0F00];
    for (size_t i = 0; i < _ARKANOID_ROW_COUNT; i++)
      for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++)
      {
        auto propertyName = std::string("Block Value [") + std::to_string(i) + std::string("][") + std::to_string(j) + std::string("]");
        registerGameProperty(propertyName, &_blocks[i * _ARKANOID_ROW_STRIDE + j], Property::datatype_t::dt_uint8, Property::endianness_t::little);
      }

    for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++) columnTileTarget[j] = target_t::nothing;
    for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++) columnTileRow[j] = 0;
    lowestTargetColumn      = 0;
    lowestTargetRow         = 0;
    _activeBalls            = 1;
    _activeShots            = 0;
    _lowestBallPosY         = 0;
    _lowestBallPosX         = 0;
    _lowestBallVelY         = 0;
    _distanceToLowestBall   = 0;
    _distanceToLowestTarget = 0;
    _remainingBlocks        = 255;
    _remainingHits          = 255;

    _lastInputStep = 0;
    _currentStep   = 0;

    // Getting index for a non input
    _nullInputIdx = _emulator->registerInput("|..|............|............|");
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // If this is is a non-null input, update the last input step
    if (input == _nullInputIdx) _lastInputStep = _currentStep;

    // Running emulator
    _emulator->advanceState(input);

    // Advance current step
    _currentStep++;
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override { hashEngine.Update(&_lowMem[0x0000], 0x1000); }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    _remainingBlocks = 0;
    _remainingHits   = 0;

    _lowestBallPosX = 0;
    _lowestBallPosY = 0;
    if (*_ball1State > 0)
      if (*_ball1PosY > _lowestBallPosY)
      {
        _lowestBallPosX = *_ball1PosX;
        _lowestBallPosY = *_ball1PosY;
        _lowestBallVelY = *_ball1VelY;
      }

    // Only count them if they're not the M powerup trails
    if (*_ballPowerUp != _ARKANOID_BALL_M_POWERUP)
    {
      if (*_ball2State > 0)
        if (*_ball2PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball2PosX;
          _lowestBallPosY = *_ball2PosY;
          _lowestBallVelY = *_ball2VelY;
        }
      if (*_ball3State > 0)
        if (*_ball3PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball3PosX;
          _lowestBallPosY = *_ball3PosY;
          _lowestBallVelY = *_ball3VelY;
        }
      if (*_ball4State > 0)
        if (*_ball4PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball4PosX;
          _lowestBallPosY = *_ball4PosY;
          _lowestBallVelY = *_ball4VelY;
        }
      if (*_ball5State > 0)
        if (*_ball5PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball5PosX;
          _lowestBallPosY = *_ball5PosY;
          _lowestBallVelY = *_ball5VelY;
        }
      if (*_ball6State > 0)
        if (*_ball6PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball6PosX;
          _lowestBallPosY = *_ball6PosY;
          _lowestBallVelY = *_ball6VelY;
        }
      if (*_ball7State > 0)
        if (*_ball7PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball7PosX;
          _lowestBallPosY = *_ball7PosY;
          _lowestBallVelY = *_ball7VelY;
        }
      if (*_ball8State > 0)
        if (*_ball8PosY > _lowestBallPosY)
        {
          _lowestBallPosX = *_ball8PosX;
          _lowestBallPosY = *_ball8PosY;
          _lowestBallVelY = *_ball8VelY;
        }
    }

    _distanceToLowestBall = std::abs((int32_t)_lowestBallPosX - ((int32_t)(*_paddlePosX) + 4000));

    _distanceToPowerUp1 = std::abs((int32_t)*_powerUp1PosX - ((int32_t)(*_paddlePosX) + 4000));
    _distanceToPowerUp2 = std::abs((int32_t)*_powerUp2PosX - ((int32_t)(*_paddlePosX) + 4000));

    _activeBalls = 0;
    if (*_ball1State > 0) _activeBalls++;
    if (*_ball2State > 0) _activeBalls++;
    if (*_ball3State > 0) _activeBalls++;
    if (*_ball4State > 0) _activeBalls++;
    if (*_ball5State > 0) _activeBalls++;
    if (*_ball6State > 0) _activeBalls++;
    if (*_ball7State > 0) _activeBalls++;
    if (*_ball8State > 0) _activeBalls++;
    if (*_ballPowerUp == _ARKANOID_BALL_M_POWERUP) _activeBalls = 1; // If M powerup, the trailing ones are not counted

    // Enemy-borne balls
    for (size_t i = 0; i < 10; i++)
      if (_lowMem[0x01DC + i] == 1) _activeBalls++;

    _activeShots = 0;
    if (*_shot1State > 0) _activeShots++;
    if (*_shot2State > 0) _activeShots++;

    for (size_t i = 0; i < _ARKANOID_ROW_COUNT; i++)
      for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++)
      {
        int idx = i * _ARKANOID_ROW_STRIDE + j;
        switch (_blocks[idx])
        {
          case 0x22:
          {
            _remainingHits += 2;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x23:
          {
            _remainingHits += 3;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x24:
          {
            _remainingHits += 4;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x25:
          {
            _remainingHits += 5;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x26:
          {
            _remainingHits += 6;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x27:
          {
            _remainingHits += 7;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x28:
          {
            _remainingHits += 8;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2A:
          {
            _remainingHits += 9;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2B:
          {
            _remainingHits += 10;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2C:
          {
            _remainingHits += 11;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2D:
          {
            _remainingHits += 12;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2E:
          {
            _remainingHits += 13;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x2F:
          {
            _remainingHits += 14;
            _remainingBlocks++;
            break;
          } // Silver Block
          case 0x30: break; // Golden Brick
          case 0x00: break;
          default:
          {
            _remainingHits++;
            _remainingBlocks++;
          }
        }
      }

    // Calculating possible laser targets
    if (*_paddlePowerUp == _ARKANOID_HELD_LASER_POWERUP)
    {
      for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++) columnTileTarget[j] = target_t::nothing;

      for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++)
        for (size_t i = 0; i < _ARKANOID_ROW_COUNT; i++)
        {
          int idx = i * _ARKANOID_ROW_STRIDE + j;
          switch (_blocks[idx])
          {
            case 0x00:
            {
              // Nothing
              break;
            }

            case 0x30:
            {
              columnTileTarget[j] = target_t::blocked; // Golden Brick
              columnTileRow[j]    = i;
              break;
            }

            default:
            {
              columnTileTarget[j] = target_t::tile;
              columnTileRow[j]    = i;
            }
          }
        }

      lowestTargetColumn = 0;
      lowestTargetRow    = 0;
      for (size_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++)
        if (columnTileTarget[j] == target_t::tile)
          if (columnTileRow[j] > lowestTargetRow)
          {
            lowestTargetRow    = columnTileRow[j];
            lowestTargetColumn = j;
          }

      _distanceToLowestTarget = std::abs((int32_t)columnPaddlePos[lowestTargetColumn] - ((int32_t)(*_paddlePosX)));
    }
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    // Resetting magnets ahead of rule re-evaluation
    _ball1PosMagnet.intensity = 0.0;
    _ball1PosMagnet.x         = 0.0;
    _ball1PosMagnet.y         = 0.0;

    _bossHealthMagnet = 0.0;
    _lastInputMagnet  = 0.0;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    // Updating distance to user-defined point
    _ball1DistanceToPointX = std::abs((float)_ball1PosMagnet.x - (float)*_ball1PosX);
    _ball1DistanceToPointY = std::abs((float)_ball1PosMagnet.y - (float)*_ball1PosY);
    _ball1DistanceToPoint  = sqrtf(_ball1DistanceToPointX * _ball1DistanceToPointX + _ball1DistanceToPointY * _ball1DistanceToPointY);
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_lastInputStep, sizeof(_lastInputStep));
    serializer.push(&_currentStep, sizeof(_currentStep));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(&_lastInputStep, sizeof(_lastInputStep));
    deserializer.pop(&_currentStep, sizeof(_currentStep));
  }

  __INLINE__ float calculateGameSpecificReward() const override
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Remaining hits
    reward -= _remainingHitsReward * _remainingHits;
    reward -= _remainingBlocksReward * _remainingBlocks;

    // Rewarding paddle being close to lowest ball
    reward -= 0.000001f * (float)_distanceToLowestBall;

    // Reward M Power
    if (*_ballPowerUp == 5) reward += _holdMPowerUpReward;

    // Reward L Power
    if (*_paddlePowerUp == _ARKANOID_HELD_LASER_POWERUP)
    {
      reward += _holdLaserPowerUpReward;

      // Bring the paddle closer to the lowest column
      reward -= 0.0000001f * (float)_distanceToLowestTarget;

      // Reward shooting laser
      reward += 0.01f * _activeShots;

      // Reward shots going up
      if (*_shot1State > 0) reward -= 0.0000001f * (float)*_shot1PosY;
      if (*_shot2State > 0) reward -= 0.0000001f * (float)*_shot2PosY;
    }

    // Reward L power falling
    bool doRewardLPowerFalling = false;
    if (*_powerUp1Active > 0 && *_powerUp1Type == _ARKANOID_FALLING_LASER_POWERUP) doRewardLPowerFalling = true;
    if (*_powerUp2Active > 0 && *_powerUp2Type == _ARKANOID_FALLING_LASER_POWERUP) doRewardLPowerFalling = true;
    if (doRewardLPowerFalling) reward += _fallingLaserPowerUpReward;

    // Reward Eight Ball powerup falling
    bool doRewardEightBallPowerUpFalling = false;
    if (*_powerUp1Active > 0 && *_powerUp1Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP) doRewardEightBallPowerUpFalling = true;
    if (*_powerUp2Active > 0 && *_powerUp2Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP) doRewardEightBallPowerUpFalling = true;
    if (doRewardEightBallPowerUpFalling) reward += _fallingEightBallPowerUpReward;

    // Reward Additional Balls
    reward += _ballCountReward * _activeBalls;

    // Reward boss health
    reward += _bossHealthMagnet * ((float)*_bossHealth1 + (float)*_bossHealth2 + (float)*_boss3Arm1HP + (float)*_boss3Arm2HP + 2.0 * (float)*_boss3TorsoHP);

    // Distance to point magnet
    reward += -1.0f * _ball1PosMagnet.intensity * _ball1DistanceToPoint;

    // Reward for last input
    reward += _lastInputMagnet * (float)_lastInputStep;

    // Returning reward
    return reward;
  }

  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override
  {
    bool addFireInput  = false;
    bool addLeftInput  = false;
    bool addRightInput = false;

    // Check if there are still valid targets
    bool targetsAvailable = false;
    for (int i = 0; i < _ARKANOID_COLUMN_COUNT; i++)
      if (columnTileTarget[i] == target_t::tile) targetsAvailable = true;

    if (*_paddlePowerUp == _ARKANOID_HELD_LASER_POWERUP) // If we have the laser power up
      if (targetsAvailable == true)                      // If there are targets available
        if (_activeShots < 2)                            // And still can shoot
        {
          // Only if there is a target in this column, and there is no lower golden block in the next one
          if (*_paddlePosX == columnPaddlePos[0])
            if (columnTileTarget[0] == target_t::tile || columnTileTarget[1] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[1])
            if (columnTileTarget[1] == target_t::tile || columnTileTarget[2] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[2])
            if (columnTileTarget[2] == target_t::tile || columnTileTarget[3] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[3])
            if (columnTileTarget[3] == target_t::tile || columnTileTarget[4] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[4])
            if (columnTileTarget[4] == target_t::tile || columnTileTarget[5] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[5])
            if (columnTileTarget[5] == target_t::tile || columnTileTarget[6] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[6])
            if (columnTileTarget[6] == target_t::tile || columnTileTarget[7] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[7])
            if (columnTileTarget[7] == target_t::tile || columnTileTarget[8] == target_t::tile) addFireInput = true;
          if (*_paddlePosX == columnPaddlePos[8])
            if (columnTileTarget[8] == target_t::tile || columnTileTarget[9] == target_t::tile) addFireInput = true;

          // Cancelling if there is a blocking golden block on the left
          if (*_paddlePosX == columnPaddlePos[0])
            if (columnTileTarget[0] == target_t::blocked && columnTileRow[0] > columnTileRow[1]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[1])
            if (columnTileTarget[1] == target_t::blocked && columnTileRow[1] > columnTileRow[2]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[2])
            if (columnTileTarget[2] == target_t::blocked && columnTileRow[2] > columnTileRow[3]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[3])
            if (columnTileTarget[3] == target_t::blocked && columnTileRow[3] > columnTileRow[4]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[4])
            if (columnTileTarget[4] == target_t::blocked && columnTileRow[4] > columnTileRow[5]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[5])
            if (columnTileTarget[5] == target_t::blocked && columnTileRow[5] > columnTileRow[6]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[6])
            if (columnTileTarget[6] == target_t::blocked && columnTileRow[6] > columnTileRow[7]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[7])
            if (columnTileTarget[7] == target_t::blocked && columnTileRow[7] > columnTileRow[8]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[8])
            if (columnTileTarget[8] == target_t::blocked && columnTileRow[8] > columnTileRow[9]) addFireInput = false;

          // Cancelling if there is a blocking golden block on the right
          if (*_paddlePosX == columnPaddlePos[0])
            if (columnTileTarget[1] == target_t::blocked && columnTileRow[1] > columnTileRow[0]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[1])
            if (columnTileTarget[2] == target_t::blocked && columnTileRow[2] > columnTileRow[1]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[2])
            if (columnTileTarget[3] == target_t::blocked && columnTileRow[3] > columnTileRow[2]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[3])
            if (columnTileTarget[4] == target_t::blocked && columnTileRow[4] > columnTileRow[3]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[4])
            if (columnTileTarget[5] == target_t::blocked && columnTileRow[5] > columnTileRow[4]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[5])
            if (columnTileTarget[6] == target_t::blocked && columnTileRow[6] > columnTileRow[5]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[6])
            if (columnTileTarget[7] == target_t::blocked && columnTileRow[7] > columnTileRow[6]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[7])
            if (columnTileTarget[8] == target_t::blocked && columnTileRow[8] > columnTileRow[7]) addFireInput = false;
          if (*_paddlePosX == columnPaddlePos[8])
            if (columnTileTarget[9] == target_t::blocked && columnTileRow[9] > columnTileRow[8]) addFireInput = false;
        }

    if (*_paddlePowerUp == _ARKANOID_HELD_LASER_POWERUP) // If we have the laser power up, we should be able to move sideways
      if (targetsAvailable == true)                      // If there are targets available
      {
        if (*_paddlePosX < 36863) addRightInput = true;
        if (*_paddlePosX > 4096) addLeftInput = true;
      }

    // Checking whether the paddle conditions to move sideways are given
    if (_lowestBallVelY > 0)       // If the lowest ball is going down (we'll have to hit it!)
      if (_lowestBallPosY > 38000) // If the lowest ball is low enough
      {
        if (*_paddlePosX < 36863) addRightInput = true;
        if (*_paddlePosX > 4096) addLeftInput = true;
      }

    // Checking whether the powerup 1 conditions to move sideways are given
    if (*_powerUp1Active > 0) // If the powerup is active
      if (*_powerUp1Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP || *_powerUp1Type == _ARKANOID_FALLING_LASER_POWERUP ||
          *_powerUp1Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP) // Only if something interesting
        if (*_powerUp1PosY > 46000)                              // If it is low enough
        {
          if (*_paddlePosX < 36863) addRightInput = true;
          if (*_paddlePosX > 4096) addLeftInput = true;
        }

    // Checking whether the powerup 2conditions to move sideways are given
    if (*_powerUp2Active > 0) // If the powerup is active
      if (*_powerUp2Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP || *_powerUp2Type == _ARKANOID_FALLING_LASER_POWERUP ||
          *_powerUp2Type == _ARKANOID_FALLING_EIGHTBALL_POWERUP) // Only if something interesting
        if (*_powerUp2PosY > 46000)                              // If it is low enough
        {
          if (*_paddlePosX < 36863) addRightInput = true;
          if (*_paddlePosX > 4096) addLeftInput = true;
        }

    // For key frames, add left/right regardless
    if (*_gameTimer % _paddleMovementFrameskip == 0)
    {
      if (*_paddlePosX < 36863) addRightInput = true;
      if (*_paddlePosX > 4096) addLeftInput = true;
    }

    // Adding inputs
    if (addFireInput) allowedInputSet.push_back(_inputFire);
    if (addLeftInput) allowedInputSet.push_back(_inputLeft);
    if (addRightInput) allowedInputSet.push_back(_inputRight);

    // If nothing chosen, input nothing
    if (allowedInputSet.empty()) allowedInputSet.push_back(_inputNothing);
  };

  void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Current Round: %u\n", *_currentRound + 1);
    jaffarCommon::logger::log("[J+]  + Game Timer: %u\n", *_gameTimer);

    jaffarCommon::logger::log("[J+]  + Active Balls:              %u\n", _activeBalls);
    if (*_ball1State > 0) jaffarCommon::logger::log("[J+]  + Ball 1 Pos: (%u, %u) Vel Y: %d\n", *_ball1PosX, *_ball1PosY, *_ball1VelY);

    // Only show them if they're not the M trail
    if (*_ballPowerUp != _ARKANOID_BALL_M_POWERUP)
    {
      if (*_ball2State > 0) jaffarCommon::logger::log("[J+]  + Ball 2 Pos: (%u, %u) Vel Y: %d\n", *_ball2PosX, *_ball2PosY, *_ball2VelY);
      if (*_ball3State > 0) jaffarCommon::logger::log("[J+]  + Ball 3 Pos: (%u, %u) Vel Y: %d\n", *_ball3PosX, *_ball3PosY, *_ball3VelY);
      if (*_ball4State > 0) jaffarCommon::logger::log("[J+]  + Ball 4 Pos: (%u, %u) Vel Y: %d\n", *_ball4PosX, *_ball4PosY, *_ball4VelY);
      if (*_ball5State > 0) jaffarCommon::logger::log("[J+]  + Ball 5 Pos: (%u, %u) Vel Y: %d\n", *_ball5PosX, *_ball5PosY, *_ball5VelY);
      if (*_ball6State > 0) jaffarCommon::logger::log("[J+]  + Ball 6 Pos: (%u, %u) Vel Y: %d\n", *_ball6PosX, *_ball6PosY, *_ball6VelY);
      if (*_ball7State > 0) jaffarCommon::logger::log("[J+]  + Ball 7 Pos: (%u, %u) Vel Y: %d\n", *_ball7PosX, *_ball7PosY, *_ball7VelY);
      if (*_ball8State > 0) jaffarCommon::logger::log("[J+]  + Ball 8 Pos: (%u, %u) Vel Y: %d\n", *_ball8PosX, *_ball8PosY, *_ball8VelY);
    }

    if (*_powerUp1Active > 0) jaffarCommon::logger::log("[J+]  + PowerUp 1 Type %u, Pos: (%u, %u) Dist: %u\n", *_powerUp1Type, *_powerUp1PosX, *_powerUp1PosY, _distanceToPowerUp1);
    if (*_powerUp2Active > 0) jaffarCommon::logger::log("[J+]  + PowerUp 2 Type %u, Pos: (%u, %u) Dist: %u\n", *_powerUp2Type, *_powerUp2PosX, *_powerUp2PosY, _distanceToPowerUp2);

    if (*_paddlePowerUp == _ARKANOID_HELD_LASER_POWERUP)
    {
      jaffarCommon::logger::log("[J+]  + Active Shots:              %u\n", _activeShots);
      if (*_shot1State > 0) jaffarCommon::logger::log("[J+]  + Shot 1 Pos: (%u, %u)\n", *_shot1PosX, *_shot1PosY);
      if (*_shot2State > 0) jaffarCommon::logger::log("[J+]  + Shot 2 Pos: (%u, %u)\n", *_shot2PosX, *_shot2PosY);

      jaffarCommon::logger::log("[J+]  + Column Tile Target:              [ ");
      for (int i = 0; i < _ARKANOID_COLUMN_COUNT; i++)
      {
        std::string symbol = "-";
        if (columnTileTarget[i] == target_t::tile) symbol = "T";
        if (columnTileTarget[i] == target_t::blocked) symbol = "B";
        jaffarCommon::logger::log("%s", symbol.c_str());
      }
      jaffarCommon::logger::log(" ]\n");

      jaffarCommon::logger::log("[J+]  + Column Tile Row:              [ ");
      for (int i = 0; i < _ARKANOID_COLUMN_COUNT; i++) jaffarCommon::logger::log("%u ", columnTileRow[i]);
      jaffarCommon::logger::log(" ]\n");

      jaffarCommon::logger::log("[J+]  + Lowest Target Row/Col:              %u / %u \n", lowestTargetRow, lowestTargetColumn);
      jaffarCommon::logger::log("[J+]  + Distance to Lowest Target:          %u (Target: %u)\n", _distanceToLowestTarget, columnPaddlePos[lowestTargetColumn]);
    }

    jaffarCommon::logger::log("[J+]  + Paddle Powerup:            %u\n", *_paddlePowerUp);
    jaffarCommon::logger::log("[J+]  + Paddle Stickyness:         %u\n", *_paddleStickyness);
    jaffarCommon::logger::log("[J+]  + Ball Powerup:              %u\n", *_ballPowerUp);

    jaffarCommon::logger::log("[J+]  + Launch Counter:            %u\n", *_launchCounter);
    jaffarCommon::logger::log("[J+]  + Paddle X: %u\n", *_paddlePosX);
    jaffarCommon::logger::log("[J+]  + Lowest Ball Pos: (%u, %u) Vel Y: %d\n", _lowestBallPosX, _lowestBallPosY, _lowestBallVelY);
    jaffarCommon::logger::log("[J+]  + Distance to Lowest Ball:   %u\n", _distanceToLowestBall);

    jaffarCommon::logger::log("[J+]  + Blocks Remaining:          %u\n", _remainingBlocks);
    jaffarCommon::logger::log("[J+]  + Hits Remaining:            %u\n", _remainingHits);

    jaffarCommon::logger::log("[J+]  + Block State:\n");

    for (uint8_t i = 0; i < _ARKANOID_ROW_COUNT; i++)
    {
      jaffarCommon::logger::log("[J+] ");
      for (uint8_t j = 0; j < _ARKANOID_COLUMN_COUNT; j++)
        if (_blocks[i * _ARKANOID_ROW_STRIDE + j] == 0)
          jaffarCommon::logger::log("---- ");
        else
          jaffarCommon::logger::log("0x%02X ", _blocks[i * _ARKANOID_ROW_STRIDE + j]);
      jaffarCommon::logger::log("\n");
    }

    if (std::abs(_ball1PosMagnet.intensity) > 0.0f)
    {
      jaffarCommon::logger::log("[J+]  + Ball 1 Pos Magnet                        Intensity: %.5f, X: %3.3f, Y: %3.3f\n", _ball1PosMagnet.intensity, _ball1PosMagnet.x,
                                _ball1PosMagnet.y);
      jaffarCommon::logger::log("[J+]    + Distance X                             %3.3f\n", _ball1DistanceToPointX);
      jaffarCommon::logger::log("[J+]    + Distance Y                             %3.3f\n", _ball1DistanceToPointY);
      jaffarCommon::logger::log("[J+]    + Total Distance                         %3.3f\n", _ball1DistanceToPoint);
    }

    if (std::abs(_bossHealthMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Boss Health Magnet                     %.5f\n", _bossHealthMagnet);
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;

    if (actionType == "Set Ball 1 Pos Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto x         = jaffarCommon::json::getNumber<float>(actionJs, "X");
      auto y         = jaffarCommon::json::getNumber<float>(actionJs, "Y");
      rule.addAction([=, this]() { this->_ball1PosMagnet = pointMagnet_t{.intensity = intensity, .x = x, .y = y}; });
      recognizedActionType = true;
    }

    if (actionType == "Set Boss Health Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_bossHealthMagnet = intensity; });
      recognizedActionType = true;
    }

    if (actionType == "Set Last Input Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_lastInputMagnet = intensity; });
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

  pointMagnet_t _ball1PosMagnet;

  // Game-Specific values
  float _ball1DistanceToPointX;
  float _ball1DistanceToPointY;
  float _ball1DistanceToPoint;
  float _bossHealthMagnet;
  float _lastInputMagnet;

  uint32_t _distanceToLowestBall;

  uint32_t _distanceToPowerUp1;
  uint32_t _distanceToPowerUp2;

  // Tile value vector
  uint8_t* _blocks;

  // Derivative values
  uint8_t*  _gameTimer;
  uint8_t*  _currentRound;
  uint8_t*  _paddlePowerUp;
  uint8_t*  _paddleStickyness;
  uint8_t*  _ballPowerUp;
  uint8_t*  _powerUp1Active;
  uint8_t*  _powerUp1Type;
  uint16_t* _powerUp1PosX;
  uint16_t* _powerUp1PosY;
  uint8_t*  _powerUp2Active;
  uint8_t*  _powerUp2Type;
  uint16_t* _powerUp2PosX;
  uint16_t* _powerUp2PosY;
  uint8_t*  _ball1State;
  uint8_t*  _ball2State;
  uint8_t*  _ball3State;
  uint8_t*  _ball4State;
  uint8_t*  _ball5State;
  uint8_t*  _ball6State;
  uint8_t*  _ball7State;
  uint8_t*  _ball8State;
  uint16_t* _ball1PosX;
  uint16_t* _ball2PosX;
  uint16_t* _ball3PosX;
  uint16_t* _ball4PosX;
  uint16_t* _ball5PosX;
  uint16_t* _ball6PosX;
  uint16_t* _ball7PosX;
  uint16_t* _ball8PosX;
  uint16_t* _ball1PosY;
  uint16_t* _ball2PosY;
  uint16_t* _ball3PosY;
  uint16_t* _ball4PosY;
  uint16_t* _ball5PosY;
  uint16_t* _ball6PosY;
  uint16_t* _ball7PosY;
  uint16_t* _ball8PosY;

  int8_t* _ball1VelY;
  int8_t* _ball2VelY;
  int8_t* _ball3VelY;
  int8_t* _ball4VelY;
  int8_t* _ball5VelY;
  int8_t* _ball6VelY;
  int8_t* _ball7VelY;
  int8_t* _ball8VelY;

  uint8_t* _bossHealth1;
  uint8_t* _bossHealth2;

  uint8_t* _boss3Arm1HP;
  uint8_t* _boss3Arm2HP;
  uint8_t* _boss3TorsoHP;

  uint16_t* _paddlePosX;
  uint8_t*  _launchCounter;

  uint16_t _lowestBallPosY;
  uint16_t _lowestBallPosX;
  int8_t   _lowestBallVelY;

  uint16_t _remainingBlocks;
  uint16_t _remainingHits;

  uint8_t _activeBalls;
  uint8_t _activeShots;

  uint8_t*  _shot1State;
  uint8_t*  _shot2State;
  uint16_t* _shot1PosX;
  uint16_t* _shot2PosX;
  uint16_t* _shot1PosY;
  uint16_t* _shot2PosY;

  // Config
  float   _remainingHitsReward;
  float   _remainingBlocksReward;
  float   _ballCountReward;
  float   _holdMPowerUpReward;
  float   _holdLaserPowerUpReward;
  float   _fallingLaserPowerUpReward;
  float   _fallingEightBallPowerUpReward;
  uint8_t _paddleMovementFrameskip;

  // Pointer to emulator's low memory storage
  uint8_t* _lowMem;

  InputSet::inputIndex_t _inputNothing;
  InputSet::inputIndex_t _inputLeft;
  InputSet::inputIndex_t _inputRight;
  InputSet::inputIndex_t _inputFire;

  target_t columnTileTarget[_ARKANOID_COLUMN_COUNT];
  uint8_t  columnTileRow[_ARKANOID_COLUMN_COUNT];
  uint16_t columnPaddlePos[_ARKANOID_COLUMN_COUNT] = {4096, 7168, 11776, 16895, 19967, 24575, 29183, 33791, 36863};
  uint32_t _distanceToLowestTarget;

  uint8_t lowestTargetColumn;
  uint8_t lowestTargetRow;

  InputSet::inputIndex_t _nullInputIdx;
  uint16_t               _lastInputStep;
  uint16_t               _currentStep;
};

} // namespace snes

} // namespace games

} // namespace jaffarPlus
