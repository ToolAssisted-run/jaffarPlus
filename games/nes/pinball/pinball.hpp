#pragma once

#include <atomic>
#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>

namespace jaffarPlus
{

namespace games
{

namespace nes
{

class Pinball final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "NES / Pinball"; }

  Pinball(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    // No game-specific configuration keys; reject any leftover (unrecognized) key in the game configuration.
  }

private:
  __INLINE__ void registerGameProperties() override
  {
    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("LRAM").pointer;

    _springCharge  = (uint8_t*)registerGameProperty("Spring Charge", &_lowMem[0x00E3], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _ballPosX      = (uint8_t*)registerGameProperty("Ball Pos X", &_lowMem[0x0007], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _ballPosY      = (uint8_t*)registerGameProperty("Ball Pos Y", &_lowMem[0x0009], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _ballDirY      = (uint8_t*)registerGameProperty("Ball Dir Y", &_lowMem[0x000D], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _lagFrame      = (uint8_t*)registerGameProperty("Lag Frame", &_lowMem[0x001D], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _score06       = (uint8_t*)registerGameProperty("Score06", &_lowMem[0x0100], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _score05       = (uint8_t*)registerGameProperty("Score05", &_lowMem[0x0101], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _score04       = (uint8_t*)registerGameProperty("Score04", &_lowMem[0x0102], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _score03       = (uint8_t*)registerGameProperty("Score03", &_lowMem[0x0103], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _score02       = (uint8_t*)registerGameProperty("Score02", &_lowMem[0x0104], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _currentScreen = (uint8_t*)registerGameProperty("Current Screen", &_lowMem[0x00BF], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _warpState     = (uint8_t*)registerGameProperty("Warp State", &_lowMem[0x0023], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    _paulinePosX = (uint8_t*)registerGameProperty("Pauline Pos X", &_lowMem[0x00ED], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _paulinePosY = (uint8_t*)registerGameProperty("Pauline Pos Y", &_lowMem[0x011C], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    // Per-frame count of instructions the CPU fetched from work RAM (data-as-code "walk" detector).
    // Normal play is a handful (RAM-resident jump-notes at $00E3); a large value flags the score-warp
    // walk. Registered so search rules can win/fail on it. uint16, provided by the emulator each frame.
    _ramExecCount = (uint16_t*)_emulator->getProperty("CPU RAM Exec Count").pointer;
    registerGameProperty("RAM Exec Count", _ramExecCount, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Frames In Play", &_framesInPlay, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Walk Happened", &_walkHappened, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Frames Since Walk", &_framesSinceWalk, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Score Gain Since Walk", &_scoreGainSinceWalk, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Walk Boost", &_walkBoost, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Play Score", &_playScore, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Score Jump", &_scoreJump, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    // CPU halt latch: 1 once the CPU executed a KIL/JAM (a walk that crashed/froze the game). Registered
    // so the glitch hunt can FAIL such states and only surface walks that keep the game running.
    _cpuHalt = (uint8_t*)_emulator->getProperty("CPU Halt Latch").pointer;
    registerGameProperty("CPU Halt Latch", _cpuHalt, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Frame-timing accumulators (burst_phase, frame_length_extra) that drive the NMI alignment a walk
    // depends on. Registered so a config can list it under "Hash Properties" to stop the dedup from
    // collapsing timing-distinct states (essential for systematically exploring walks).
    _timingState = _emulator->getProperty("CPU Timing State").pointer;
    registerGameProperty("CPU Timing State", _timingState, Property::datatype_t::dt_uint64, Property::endianness_t::little);

    _platform1State = (uint8_t*)registerGameProperty("Platform 1 State", &_lowMem[0x0121], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _platform2State = (uint8_t*)registerGameProperty("Platform 2 State", &_lowMem[0x0137], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    _platform3State = (uint8_t*)registerGameProperty("Platform 3 State", &_lowMem[0x013C], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Ball Launched", &_ballLaunched, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Score", &_score, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    registerGameProperty("Prev Score", &_prevScore, Property::datatype_t::dt_uint32, Property::endianness_t::little);
    _nullInputIdx = _emulator->registerInput("|..|........|");
    _ballLaunched = false;
    _prevScore    = 0;
    _framesInPlay = 0;
    _walkHappened = false;
    _walkStep = 0;
    _scoreAtWalk = 0;
    _framesSinceWalk = 0;
    _scoreGainSinceWalk = 0;
    _scoreBeforeWalk = 0;
    _walkBoost = 0;
    _scoreJump = 0;
    _playScore = 0;
    _escapedChamber = false;

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
    hashEngine.Update(*_springCharge);
    hashEngine.Update(*_ballPosX);
    hashEngine.Update(*_ballPosY);
    hashEngine.Update(*_ballDirY);
    hashEngine.Update(*_lagFrame);
    hashEngine.Update(*_score06);
    hashEngine.Update(*_score05);
    hashEngine.Update(*_score04);
    hashEngine.Update(*_score03);
    hashEngine.Update(*_score02);

    hashEngine.Update(&_lowMem[0x0000], 0x140);
  }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    _prevScore = _score;
    _score     = 100000 * (*_score06) + 10000 * (*_score05) + 1000 * (*_score04) + 100 * (*_score03) + 10 * (*_score02);
    if (*_ballDirY != 0) _ballLaunched = true;
    // Single-frame score jump. Normal scoring is <= ~2200/frame, so a value near +100000 is the boost.
    _scoreJump = (_score > _prevScore) ? (_score - _prevScore) : 0;

    _sumPlatform = 0;
    _sumPlatform += *_platform1State;
    _sumPlatform += *_platform2State;
    _sumPlatform += *_platform3State;

    // Count frames the ball is actually on the table and in play (launched, main screen). A monotonic
    // "time in play" measure: rewarding it drives the search to keep the ball alive with the paddles
    // rather than peaking once and letting it drain.
    if (_ballLaunched == true && *_currentScreen == 0) _framesInPlay++;

    // "Useful walk" detector for the glitch hunt. Latch the first data-as-code walk (RAM Exec > 15) and
    // remember when/what score. A USEFUL walk leaves the game running: only a healthy game keeps scoring,
    // so "score still rising N frames after the walk" rejects crashes, resets/hangs AND flipper-jams
    // (trapped ball can't score) in one test -- exactly the outcomes we want to filter out.
    if (*_ramExecCount > 15 && _walkHappened == false)
    {
      _walkHappened    = true;
      _walkStep        = _currentStep;
      _scoreAtWalk     = _score;
      _scoreBeforeWalk = _prevScore; // score the frame before the walk fired
    }
    _framesSinceWalk    = _walkHappened ? (_currentStep - _walkStep) : 0;
    _scoreGainSinceWalk = (_walkHappened && _score > _scoreAtWalk) ? (_score - _scoreAtWalk) : 0;
    // The walk's own score jump (a "boost" walk vs a benign one). Normal scoring is <= ~2200/frame, so a
    // large value flags a walk that directly awards points -- what we want, provided controls survive.
    _walkBoost = (_walkHappened && _scoreAtWalk > _scoreBeforeWalk) ? (_scoreAtWalk - _scoreBeforeWalk) : 0;
    // "Play score" = score from actual play only, EXCLUDING a boost walk's raw jump (pre-boost score +
    // post-boost scoring). Rewarding this keeps a crash-boost (huge score, then no more scoring) from
    // dominating the beam, and rewards a boost that leaves the game playable (it keeps scoring afterward).
    _playScore = _walkHappened ? (_scoreBeforeWalk + _scoreGainSinceWalk) : _score;

    // Latch: the ball has escaped the launch chamber into the main playfield (launched AND on screen 0).
    // Stays set for the rest of the run (used by the launch/altitude/proximity reward shaping).
    if (_ballLaunched == true && *_currentScreen == 0) _escapedChamber = true;
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    _ballPosXMagnet.intensity = 0.0f;
    _ballPosYMagnet.intensity = 0.0f;
    _ballPosXMagnet.pos       = 0.0f;
    _ballPosYMagnet.pos       = 0.0f;

    _paulinePosXMagnet.intensity = 0.0f;
    _paulinePosYMagnet.intensity = 0.0f;
    _paulinePosXMagnet.pos       = 0.0f;
    _paulinePosYMagnet.pos       = 0.0f;

    _scoreMagnet = 0.0f;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    _ballDistanceToPointX = std::abs((float)_ballPosXMagnet.pos - (float)*_ballPosX);
    _ballDistanceToPointY = std::abs((float)_ballPosYMagnet.pos - (float)*_ballPosY);

    _paulineDistanceToPointX = std::abs((float)_paulinePosXMagnet.pos - (float)*_paulinePosX);
    _paulineDistanceToPointY = std::abs((float)_paulinePosYMagnet.pos - (float)*_paulinePosY);
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_currentStep, sizeof(_currentStep));
    serializer.push(&_lastInput, sizeof(_lastInput));
    serializer.push(&_ballLaunched, sizeof(_ballLaunched));
    serializer.push(&_prevScore, sizeof(_prevScore));
    serializer.push(&_sumPlatform, sizeof(_sumPlatform));
    serializer.push(&_framesInPlay, sizeof(_framesInPlay));
    serializer.push(&_walkHappened, sizeof(_walkHappened));
    serializer.push(&_walkStep, sizeof(_walkStep));
    serializer.push(&_scoreAtWalk, sizeof(_scoreAtWalk));
    serializer.push(&_scoreBeforeWalk, sizeof(_scoreBeforeWalk));
    serializer.push(&_escapedChamber, sizeof(_escapedChamber));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.pop(&_currentStep, sizeof(_currentStep));
    deserializer.pop(&_lastInput, sizeof(_lastInput));
    deserializer.pop(&_ballLaunched, sizeof(_ballLaunched));
    deserializer.pop(&_prevScore, sizeof(_prevScore));
    deserializer.pop(&_sumPlatform, sizeof(_sumPlatform));
    deserializer.pop(&_framesInPlay, sizeof(_framesInPlay));
    deserializer.pop(&_walkHappened, sizeof(_walkHappened));
    deserializer.pop(&_walkStep, sizeof(_walkStep));
    deserializer.pop(&_scoreAtWalk, sizeof(_scoreAtWalk));
    deserializer.pop(&_scoreBeforeWalk, sizeof(_scoreBeforeWalk));
    deserializer.pop(&_escapedChamber, sizeof(_escapedChamber));
  }

  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Optional position magnets (zero unless a config sets an intensity).
    reward += -1.0 * _ballPosXMagnet.intensity * _ballDistanceToPointX;
    reward += -1.0 * _ballPosYMagnet.intensity * _ballDistanceToPointY;
    reward += -1.0 * _paulinePosXMagnet.intensity * _paulineDistanceToPointX;
    reward += -1.0 * _paulinePosYMagnet.intensity * _paulineDistanceToPointY;

    // Launch shaping (from-start searches). Until the ball escapes the launch chamber, reward its
    // ALTITUDE (a strong launch that carries it up and over into the playfield), plus the plunger charge
    // while it is still on the plunger. The moment it escapes into the main playfield, a big discrete
    // bonus makes escaped states dominate; after that, a MINIMAL pull toward the position where the
    // score-boost fires (upper-center bumper region ~(150,64) on screen 0) nudges the ball there.
    if (_escapedChamber == false)
    {
      reward += (255.0f - (float)*_ballPosY);                       // continuous: ball altitude in the chamber
      if (_ballLaunched == false) reward += (float)*_springCharge;  // + plunger charge while on the plunger
    }
    else
    {
      reward += 5000.0f;                                            // discrete: escaped the launch chamber
      if (*_currentScreen == 0)                                     // minimal pull toward the boost position
      {
        float dx = 150.0f - (float)*_ballPosX; if (dx < 0.0f) dx = -dx;
        float dy = 64.0f - (float)*_ballPosY;  if (dy < 0.0f) dy = -dy;
        reward += -0.1f * (dx + dy);
      }
    }

    // Score reward, using the play-score (excludes a boost's raw jump so crash-boosts don't dominate).
    reward += (float)_playScore * _scoreMagnet;

    // Returning reward
    return reward;
  }

  // Function to report what all the possible input that the game might require
  __INLINE__ std::set<std::string> getAllPossibleInputs() override { return {}; }

  // Function to enable a game code to provide additional allowed inputs based on complex decisions
  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override {}

  __INLINE__ uint64_t getStateMoveHash() const { return 0; }

  void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[J+]  + Current Step:                     %04u\n", _currentStep);
    jaffarCommon::logger::log("[J+]  + Score:                            %u (Prev: %u)\n", _score, _prevScore);
    jaffarCommon::logger::log("[J+]  + Current Screen:                   %02u\n", _currentScreen);
    jaffarCommon::logger::log("[J+]  + Spring Charge:                    %02u\n", *_springCharge);
    jaffarCommon::logger::log("[J+]  + Ball Launched:                    %s\n", _ballLaunched ? "True" : "False");
    jaffarCommon::logger::log("[J+]  + Ball Pos X:                       %02u\n", *_ballPosX);
    jaffarCommon::logger::log("[J+]  + Ball Pos Y:                       %02u\n", *_ballPosY);
    jaffarCommon::logger::log("[J+]  + Warp State:                       %02u\n", *_warpState);
    jaffarCommon::logger::log("[J+]  + RAM Exec Count:                   %u\n", *_ramExecCount);
    jaffarCommon::logger::log("[J+]  + Frames In Play:                   %u\n", _framesInPlay);
    jaffarCommon::logger::log("[J+]  + Pauline Pos X:                    %02u\n", *_paulinePosX);
    jaffarCommon::logger::log("[J+]  + Pauline Pos Y:                    %02u\n", *_paulinePosY);

    jaffarCommon::logger::log("[J+]  + Platforms:                        Sum: %02u [ %02u, %02u, %02u ]\n", _sumPlatform, *_platform1State, *_platform2State, *_platform3State);

    if (std::abs(_ballPosXMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Ball Pos X Magnet                 Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _ballPosXMagnet.intensity, _ballPosXMagnet.pos,
                                _ballDistanceToPointX);
    if (std::abs(_ballPosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Ball Pos Y Magnet                 Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _ballPosYMagnet.intensity, _ballPosYMagnet.pos,
                                _ballDistanceToPointY);
    if (std::abs(_scoreMagnet) > 0.0f)

      if (std::abs(_paulinePosXMagnet.intensity) > 0.0f)
        jaffarCommon::logger::log("[J+]  + Pauline Pos X Magnet                 Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _paulinePosXMagnet.intensity, _paulinePosXMagnet.pos,
                                  _paulineDistanceToPointX);
    if (std::abs(_paulinePosYMagnet.intensity) > 0.0f)
      jaffarCommon::logger::log("[J+]  + Pauline Pos Y Magnet                 Intensity: %.5f, Pos: %.5f, Distance: %.5f\n", _paulinePosYMagnet.intensity, _paulinePosYMagnet.pos,
                                _paulineDistanceToPointY);
    if (std::abs(_scoreMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Score Magnet                      Intensity: %.5f\n", _scoreMagnet);
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;

    if (actionType == "Set Ball Pos X Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_ballPosXMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }

    if (actionType == "Set Ball Pos Y Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_ballPosYMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }

    if (actionType == "Set Pauline Pos Y Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_paulinePosYMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }

    if (actionType == "Set Pauline Pos X Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto pos       = jaffarCommon::json::getNumber<float>(actionJs, "Pos");
      auto action    = [=, this]() { this->_paulinePosYMagnet = pointMagnet_t{.intensity = intensity, .pos = pos}; };
      rule.addAction(action);
      recognizedActionType = true;
    }

    if (actionType == "Set Score Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_scoreMagnet = intensity; });
      recognizedActionType = true;
    }

    return recognizedActionType;
  }

  // Datatype to describe a point magnet
  struct pointMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    float pos       = 0.0; // What is the point of attraction X
  };

  float         _scoreMagnet;
  pointMagnet_t _ballPosXMagnet;
  pointMagnet_t _ballPosYMagnet;
  float         _ballDistanceToPointX;
  float         _ballDistanceToPointY;

  pointMagnet_t _paulinePosXMagnet;
  pointMagnet_t _paulinePosYMagnet;
  float         _paulineDistanceToPointX;
  float         _paulineDistanceToPointY;

  InputSet::inputIndex_t _lastInput;
  InputSet::inputIndex_t _nullInputIdx;
  uint8_t*               _lowMem;
  uint32_t               _score;
  bool                   _ballLaunched;
  uint8_t*               _springCharge;
  uint8_t*               _ballPosX;
  uint8_t*               _ballPosY;
  uint8_t*               _ballDirY;
  uint8_t*               _lagFrame;
  uint8_t*               _score06;
  uint8_t*               _score05;
  uint8_t*               _score04;
  uint8_t*               _score03;
  uint8_t*               _score02;
  uint8_t*               _currentScreen;
  uint8_t*               _warpState;
  uint16_t*              _ramExecCount;
  uint8_t*               _cpuHalt;
  uint8_t*               _timingState;
  uint8_t*               _platform1State;
  uint8_t*               _platform2State;
  uint8_t*               _platform3State;
  uint8_t*               _paulinePosX;
  uint8_t*               _paulinePosY;

  uint32_t _currentStep;
  uint32_t _prevScore;
  uint8_t  _sumPlatform;
  uint32_t _framesInPlay;
  bool     _walkHappened;
  uint32_t _walkStep;
  uint32_t _scoreAtWalk;
  uint32_t _framesSinceWalk;
  uint32_t _scoreGainSinceWalk;
  uint32_t _scoreBeforeWalk;
  uint32_t _walkBoost;
  uint32_t _scoreJump;
  uint32_t _playScore;
  bool     _escapedChamber;
};

} // namespace nes

} // namespace games

} // namespace jaffarPlus
