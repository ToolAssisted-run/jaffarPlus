#pragma once

#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/string.hpp>
#include <limits>

namespace jaffarPlus
{

namespace games
{

namespace nes
{

class MarbleMadness final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "NES / Marble Madness"; }

  MarbleMadness(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    // Parsing configuration
    _lastInputStepReward = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Last Input Step Reward");

    // Getting setting for the repetition of the previous input
    _repeatPrevInputCount = jaffarCommon::json::popNumber<uint16_t>(_gameConfigRemaining, "Repeat Prev Input Times");

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

    // Progress Magnet (optional): phase-space progress reward along a reference trajectory.
    // Rewards how many frames AHEAD of schedule a state sits on the reference corridor (best
    // position+velocity match over a step window), minus its deviation from that corridor.
    if (_gameConfigRemaining.contains("Progress Magnet"))
    {
      auto pm               = jaffarCommon::json::popObject(_gameConfigRemaining, "Progress Magnet");
      _pmIntensity          = jaffarCommon::json::popNumber<float>(pm, "Intensity");
      _pmDeviationWeight    = jaffarCommon::json::popNumber<float>(pm, "Deviation Weight");
      _pmWindowBehind       = jaffarCommon::json::popNumber<int>(pm, "Window Behind");
      _pmWindowAhead        = jaffarCommon::json::popNumber<int>(pm, "Window Ahead");
      _pmPositionScale      = jaffarCommon::json::popNumber<float>(pm, "Position Scale");
      _pmVelocityScale      = jaffarCommon::json::popNumber<float>(pm, "Velocity Scale");
      _pmZWeight            = jaffarCommon::json::popNumber<float>(pm, "Z Weight");
      _pmAirbornePenalty    = jaffarCommon::json::popNumber<float>(pm, "Airborne Mismatch Penalty");
      _pmDeviationGate      = pm.contains("Deviation Gate") ? jaffarCommon::json::popNumber<float>(pm, "Deviation Gate") : std::numeric_limits<float>::infinity();
      _pmOffCorridorPenalty = pm.contains("Off Corridor Penalty") ? jaffarCommon::json::popNumber<float>(pm, "Off Corridor Penalty") : 20.0f;
      const auto ptracePath = jaffarCommon::json::popString(pm, "Trace File Path");
      jaffarCommon::json::checkEmpty(pm, "Game Configuration > Progress Magnet");
      if (ptracePath != "")
      {
        std::string ptraceData;
        bool        status = jaffarCommon::file::loadStringFromFile(ptraceData, ptracePath.c_str());
        if (status == false && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr) JAFFAR_THROW_LOGIC("Could not find/read progress trace file: %s\n", ptracePath.c_str());
        std::istringstream f(ptraceData);
        std::string        line;
        while (std::getline(f, line))
        {
          auto c = jaffarCommon::string::split(line, ' ');
          if (c.size() < 6) continue;
          _ptrace.push_back(ptraceEntry_t{.x   = (float)std::atof(c[0].c_str()),
                                          .y   = (float)std::atof(c[1].c_str()),
                                          .z   = (float)std::atof(c[2].c_str()),
                                          .vx  = (float)std::atof(c[3].c_str()),
                                          .vy  = (float)std::atof(c[4].c_str()),
                                          .air = (uint8_t)std::atoi(c[5].c_str())});
        }
        _usePMagnet = _ptrace.size() > 0;
      }
    }

    // Hash Quantization (optional): coarsen the DEDUP hash (not the states) so sub-pixel micro-variants
    // merge. Position subpixels and the velocity dither bits otherwise make near-identical states hash
    // apart, saturating any state database within a few steps. The raw fine-grained bytes must also be
    // removed from "Hash Properties"; this block adds their quantized stand-ins.
    if (_gameConfigRemaining.contains("Hash Quantization"))
    {
      auto hq          = jaffarCommon::json::popObject(_gameConfigRemaining, "Hash Quantization");
      _hqVelocityShift = jaffarCommon::json::popNumber<int>(hq, "Velocity Shift");
      _hqPositionShift = hq.contains("Position Shift") ? jaffarCommon::json::popNumber<int>(hq, "Position Shift") : 0;
      if (hq.contains("Exact Region"))
      {
        auto er      = jaffarCommon::json::popObject(hq, "Exact Region");
        _hqExactXMin = jaffarCommon::json::popNumber<float>(er, "X Min");
        _hqExactYMax = jaffarCommon::json::popNumber<float>(er, "Y Max");
        jaffarCommon::json::checkEmpty(er, "Hash Quantization > Exact Region");
        _hqUseExactRegion = true;
      }
      _hqHashSubpixels = jaffarCommon::json::popBoolean(hq, "Hash Position Subpixels");
      jaffarCommon::json::checkEmpty(hq, "Game Configuration > Hash Quantization");
      _useHashQuantization = true;
    }

    // Funnel Reward (optional): scripted-transit (funnel) traversal rewards, detected via the game's
    // own scripted-motion state ($7B == 1), not a position box. Discrete reward on entry, a small
    // reward per frame WHILE inside (only during traversal -- after exit the PM resumes and carries
    // the entry-time gradient via position), and a discrete reward on exit.
    if (_gameConfigRemaining.contains("Funnel Reward"))
    {
      auto fj            = jaffarCommon::json::popObject(_gameConfigRemaining, "Funnel Reward");
      _funnelEnterReward = jaffarCommon::json::popNumber<float>(fj, "Enter Reward");
      _funnelFrameReward = jaffarCommon::json::popNumber<float>(fj, "Frame Reward");
      _funnelExitReward  = jaffarCommon::json::popNumber<float>(fj, "Exit Reward");
      jaffarCommon::json::checkEmpty(fj, "Game Configuration > Funnel Reward");
      _useFunnelReward = true;
    }

    // Waypoint Magnet (optional): a sparse chain of arrival boxes placed at strategic course pivots
    // (funnel mouth, sharp turns, bridge approach). Entering the next box in the chain pays a discrete
    // reward and retargets the attraction toward the following box's center. Progress along the chain
    // is path-tracked (serialized and hashed) because course position is not monotone -- the endgame
    // doubles back over earlier coordinates. "Distance Quantum" coarsens the distance term into
    // plateaus so genuine schedule waits (crawls, reversals) do not read as reward regressions.
    if (_gameConfigRemaining.contains("Waypoint Magnet"))
    {
      auto wm            = jaffarCommon::json::popObject(_gameConfigRemaining, "Waypoint Magnet");
      _wpIntensity       = jaffarCommon::json::popNumber<float>(wm, "Intensity");
      _wpBoxReward       = jaffarCommon::json::popNumber<float>(wm, "Box Reward");
      _wpDistanceQuantum = wm.contains("Distance Quantum") ? jaffarCommon::json::popNumber<float>(wm, "Distance Quantum") : 0.0f;
      // Ratchet (default true): distance term = closest-ever approach (monotone through waits, but
      // PATH-DEPENDENT and hash-invisible -- RAM-identical states with different histories shadow
      // each other in dedup). false = live distance: reward is a pure function of RAM (dedup-correct);
      // use for segments whose reference approaches targets monotonically.
      _wpUseRatchet = wm.contains("Ratchet") ? jaffarCommon::json::popBoolean(wm, "Ratchet") : true;
      if (wm.contains("Boxes") == false || wm["Boxes"].is_array() == false) JAFFAR_THROW_LOGIC("Waypoint Magnet requires a 'Boxes' array\n");
      for (auto& bj : wm["Boxes"])
      {
        wpBox_t b;
        b.xMin   = bj.at("X Min").get<float>();
        b.xMax   = bj.at("X Max").get<float>();
        b.yMin   = bj.at("Y Min").get<float>();
        b.yMax   = bj.at("Y Max").get<float>();
        b.cx     = 0.5f * (b.xMin + b.xMax);
        b.cy     = 0.5f * (b.yMin + b.yMax);
        b.reward = bj.contains("Reward") ? bj["Reward"].get<float>() : _wpBoxReward;
        b.zMin   = bj.contains("Z Min") ? bj["Z Min"].get<float>() : -1.0e9f;
        b.zMax   = bj.contains("Z Max") ? bj["Z Max"].get<float>() : 1.0e9f;
        _wpBoxes.push_back(b);
      }
      wm.erase("Boxes");
      // Prefix sums: reward for having collected boxes 0..i-1
      _wpPrefix.push_back(0.0f);
      for (const auto& b : _wpBoxes) _wpPrefix.push_back(_wpPrefix.back() + b.reward);
      jaffarCommon::json::checkEmpty(wm, "Game Configuration > Waypoint Magnet");
      _useWaypointMagnet = _wpBoxes.size() > 0;
    }

    // All recognized game-configuration keys have now been consumed; reject any leftover (unrecognized) key.
  }

private:
  struct traceEntry_t
  {
    float x;
    float y;
  };

  struct ptraceEntry_t
  {
    float   x;
    float   y;
    float   z;
    float   vx;
    float   vy;
    uint8_t air;
  };

  struct wpBox_t
  {
    float xMin;
    float xMax;
    float yMin;
    float yMax;
    float cx;
    float cy;
    float reward;
    float zMin;
    float zMax;
  };

  struct funnelState_t
  {
    uint8_t  entered  = 0;
    uint8_t  exited   = 0;
    uint16_t frames   = 0;
    uint16_t exitStep = 0; ///< Step at which the funnel released this line (0 = not yet exited)
  };

  __INLINE__ void registerGameProperties() override
  {
    // Getting emulator's low memory pointer
    _lowMem = _emulator->getProperty("LRAM").pointer;

    // Registering native game properties
    registerGameProperty("Game Timer", &_lowMem[0x000A], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Game Cycle", &_lowMem[0x07DD], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Win Flag", &_lowMem[0x008B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble State", &_lowMem[0x0019], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Flags", &_lowMem[0x0408], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos X1", &_lowMem[0x0398], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos X2", &_lowMem[0x0390], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos X3", &_lowMem[0x0388], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos Y1", &_lowMem[0x03B0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos Y2", &_lowMem[0x03A8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos Y3", &_lowMem[0x03A0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Pos Z1", &_lowMem[0x03C0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Airtime", &_lowMem[0x03C8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Vel X", &_lowMem[0x03D0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Vel Y", &_lowMem[0x03E0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Velocity is 16-bit sign-magnitude: high bytes + Y direction flag (X direction = "Marble Motion 1").
    // Hash these too, or states differing only in the high byte / Y direction dedup wrongly.
    registerGameProperty("Marble Vel X Hi", &_lowMem[0x03D8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Vel Y Hi", &_lowMem[0x03E8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Motion 2", &_lowMem[0x0418], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Airborne physics state: Z velocity, fall counter and airborne flag are causal during hops/falls
    // (pit-corner cuts). States identical in X/Y but differing here diverge — they must hash apart.
    registerGameProperty("Marble Vel Z", &_lowMem[0x03F0], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Vel Z Hi", &_lowMem[0x03F8], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Fall Counter", &_lowMem[0x0082], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Airborne Flag", &_lowMem[0x007B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Funnel transit countdown (decrements once per frame while the marble rides a funnel; release at 0).
    // Must be hashed: riders pinned at the funnel point are byte-identical in position/velocity, and this
    // timer is what distinguishes an early entrant from a late one.
    registerGameProperty("Funnel Timer", &_lowMem[0x00DE], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Clock-coupled causal state: the camera scroll ($30/$31) anchors the platform schedule (its freeze
    // at $0343 starts the phase machine) and the phase counter ($9B) drives every blink window. Hashing
    // them separates same-position states with different schedules -- the correct, principled form of
    // "clock in the hash": two states are interchangeable only if position AND schedule state agree.
    registerGameProperty("Camera Scroll Lo", &_lowMem[0x0030], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Camera Scroll Hi", &_lowMem[0x0031], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Platform Phase", &_lowMem[0x009B], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    // Pause machinery: the phase stepper ($49; 7 = hold/accepting phase where unpause Start is honored)
    // and the pause-in countdown ($D2). Both are clock-coupled pause state: paused lines with different
    // pause-in times differ ONLY here and must not merge.
    registerGameProperty("Pause Phase", &_lowMem[0x0049], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Pause Countdown", &_lowMem[0x00D2], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Dead Flag", &_lowMem[0x0400], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Motion 1", &_lowMem[0x0410], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Void Fall Frames", &_voidFallFrames, Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble Surface Angle", &_lowMem[0x0428], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Pause State", &_lowMem[0x05CA], Property::datatype_t::dt_uint8, Property::endianness_t::little);
    registerGameProperty("Marble In Catapult", &_lowMem[0x0420], Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Marble Pos X", &_marblePosX, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Marble Pos Y", &_marblePosY, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Marble Pos Z", &_marblePosZ, Property::datatype_t::dt_float32, Property::endianness_t::little);
    registerGameProperty("Try New Inputs", &_tryNewInputs, Property::datatype_t::dt_bool, Property::endianness_t::little);
    registerGameProperty("Prev Pause State", &_prevPauseState, Property::datatype_t::dt_uint8, Property::endianness_t::little);

    registerGameProperty("Current Step", &_currentStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    registerGameProperty("Last Input Step", &_lastInputStep, Property::datatype_t::dt_uint16, Property::endianness_t::little);
    _gameTimer          = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Game Timer")]->getPointer();
    _gameCycle          = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Game Cycle")]->getPointer();
    _winFlag            = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Win Flag")]->getPointer();
    _marbleState        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble State")]->getPointer();
    _marbleFlags        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Flags")]->getPointer();
    _marblePosX1        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos X1")]->getPointer();
    _marblePosX2        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos X2")]->getPointer();
    _marblePosX3        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos X3")]->getPointer();
    _marblePosY1        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos Y1")]->getPointer();
    _marblePosY2        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos Y2")]->getPointer();
    _marblePosY3        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos Y3")]->getPointer();
    _marblePosZ1        = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Pos Z1")]->getPointer();
    _marbleAirtime      = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Airtime")]->getPointer();
    _marbleVelX         = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Vel X")]->getPointer();
    _marbleVelY         = (int8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Vel Y")]->getPointer();
    _marbleDeadFlag     = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Dead Flag")]->getPointer();
    _marbleSurfaceAngle = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Surface Angle")]->getPointer();
    _pauseState         = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Pause State")]->getPointer();
    _marbleVelXHi       = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Vel X Hi")]->getPointer();
    _marbleVelYHi       = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Vel Y Hi")]->getPointer();
    _marbleMotion1      = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Motion 1")]->getPointer();
    _marbleMotion2      = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Motion 2")]->getPointer();
    _marbleAirborneFlag = (uint8_t*)_propertyMap[jaffarCommon::hash::hashString("Marble Airborne Flag")]->getPointer();

    // Getting index for a non input
    _nullInputIdx = _emulator->registerInput("|..|........|");

    // Initializing prev input values
    _prevInputIdx           = _nullInputIdx;
    _prevInputRepeatedTimes = 0;
    _tryNewInputs           = true;
    _prevPauseState         = *_pauseState;
  }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override
  {
    // Storing previous pause state
    _prevPauseState = *_pauseState;

    // Increasing counter if input is null
    if (input != _nullInputIdx) _lastInputStep = _currentStep;

    // Funnel bookkeeping (advance-only: exactly one update per emulated frame): entry/exit latches and
    // the frames-inside counter, driven by the game's scripted-motion state after this frame's advance.
    // $7B==1 alone is NOT funnel-specific: it also blips for single frames on ordinary tile seams
    // (farmable). The genuine funnel hold is $7B==1 WITH the transit countdown $DE active -- a fixed
    // 49-frame window unique in the whole stage.
    const bool insideNow = (*_marbleAirborneFlag == 1) && (_lowMem[0x00DE] != 0);

    // Void-fall persistence: $7B==1 with no countdown marks game-scripted motion outside the funnel.
    // Sustained, it is a committed bottomless fall (death latches at 24 frames); on the reference it
    // never persists more than 4 frames (seam blips, funnel-transition frames). A single-frame test
    // cannot work -- the airborne counter $82 stays large across the funnel's transition frames -- so
    // the fail rule keys on this consecutive-frame counter instead.
    if (*_marbleAirborneFlag == 1 && _lowMem[0x00DE] == 0)
    {
      if (_voidFallFrames < 255) _voidFallFrames++;
    }
    else
      _voidFallFrames = 0;
    if (insideNow)
    {
      if (_funnelState.entered == 0) _funnelState.entered = 1;
      _funnelState.frames++;
    }
    else if (_funnelState.entered == 1 && _funnelState.exited == 0)
    {
      _funnelState.exited   = 1;
      _funnelState.exitStep = _currentStep;
    }

    // Checking input repetition
    if (_prevInputIdx == input)
      _prevInputRepeatedTimes++;
    else
      _prevInputRepeatedTimes = 0;

    // Checking if next time we need to try new inputs
    if (_prevInputRepeatedTimes >= _repeatPrevInputCount)
      _tryNewInputs = true;
    else
      _tryNewInputs = false;

    // Remembering input
    _prevInputIdx = input;

    // Running emulator
    _emulator->advanceState(input);

    // Advancing current step
    _currentStep++;

    // Waypoint chain progress (advance-only, like the funnel latches): collect the next box if the
    // marble is inside it after this frame's advance. A while loop tolerates overlapping boxes.
    if (_useWaypointMagnet)
    {
      const float   wx     = (float)_lowMem[0x0398] * 256.0f + (float)_lowMem[0x0390] + (float)_lowMem[0x0388] / 256.0f;
      const float   wy     = (float)_lowMem[0x03B0] * 256.0f + (float)_lowMem[0x03A8] + (float)_lowMem[0x03A0] / 256.0f;
      const uint8_t wpPrev = _wpNext;
      const float   wz     = (float)_lowMem[0x03C0];
      while (_wpNext < _wpBoxes.size() && wx >= _wpBoxes[_wpNext].xMin && wx <= _wpBoxes[_wpNext].xMax && wy >= _wpBoxes[_wpNext].yMin && wy <= _wpBoxes[_wpNext].yMax &&
             wz >= _wpBoxes[_wpNext].zMin && wz <= _wpBoxes[_wpNext].zMax)
        _wpNext++;
      // Ratchet: the distance term is the closest this line has EVER been to its current target, so
      // scripted swirls, schedule waits and small retreats hold the reward flat instead of dipping.
      // A collection resets the ratchet to the live distance toward the new target.
      if (_wpNext < _wpBoxes.size())
      {
        const float dx = wx - _wpBoxes[_wpNext].cx;
        const float dy = wy - _wpBoxes[_wpNext].cy;
        const float d  = sqrtf(dx * dx + dy * dy);
        _wpMinD        = (_wpNext != wpPrev) ? d : std::min(_wpMinD, d);
      }
      else
        _wpMinD = 0.0f;
    }
  }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    // Hash the step counter whenever the marble is frozen in place (paused, or in the pre-roll
    // intro where marbleState==0 and input is ignored). Without this, the inert first frame(s)
    // produce successors identical to the base state, which all dedup away -> "ran out of states".
    if (*_pauseState == 0 || *_marbleState == 0 || _insideFunnel) hashEngine.Update(_currentStep);
    // Platform-cycle position: $9B gives the phase, but the time to the NEXT phase tick is set by the
    // global counter's position on the 32-frame grid ($0A & 0x1F) -- the actual platform-cycle clock.
    // Two waiting states with identical phase but different cycle positions face different windows and
    // must never merge; this also eliminates cross-step shadowing between wait states in general.
    hashEngine.Update((uint8_t)(_lowMem[0x000A] & 0x1F));

    // Quantized dedup terms: 16-bit velocity magnitudes at reduced resolution (the accel dither
    // injects +-3 low-bit noise per frame); the raw velocity bytes and position subpixels are
    // excluded via the config's Hash Properties, and this block supplies their quantized stand-ins.
    if (_useHashQuantization)
    {
      if (_inExactRegion)
      {
        // Endgame region: full-precision dedup (position with subpixels, Z low byte, full velocities).
        // The blink-bridge choreography is decided at subpixel scale; cell representatives cannot
        // thread it (proven: a full coarse-hash run reached the endgame with big leads and zero wins).
        hashEngine.Update(&_lowMem[0x0398], 1);
        hashEngine.Update(&_lowMem[0x0390], 1);
        hashEngine.Update(&_lowMem[0x0388], 1);
        hashEngine.Update(&_lowMem[0x03B0], 1);
        hashEngine.Update(&_lowMem[0x03A8], 1);
        hashEngine.Update(&_lowMem[0x03A0], 1);
        hashEngine.Update(&_lowMem[0x03C8], 1);
        hashEngine.Update(&_lowMem[0x03D0], 1);
        hashEngine.Update(&_lowMem[0x03D8], 1);
        hashEngine.Update(&_lowMem[0x03E0], 1);
        hashEngine.Update(&_lowMem[0x03E8], 1);
        hashEngine.Update(&_lowMem[0x03F0], 1);
        hashEngine.Update(&_lowMem[0x03F8], 1);
      }
      else
      {
        // Open course: cell-resolution dedup (position >> shift, velocity >> shift) merges the
        // micro-variant crowd so the frontier spends its capacity on genuinely distinct lines.
        const uint16_t pxq = (uint16_t)(((uint16_t)_lowMem[0x0398] * 256 + _lowMem[0x0390]) >> _hqPositionShift);
        const uint16_t pyq = (uint16_t)(((uint16_t)_lowMem[0x03B0] * 256 + _lowMem[0x03A8]) >> _hqPositionShift);
        hashEngine.Update(pxq);
        hashEngine.Update(pyq);
        const uint16_t vxq = (uint16_t)(((uint16_t)_lowMem[0x03D8] * 256 + _lowMem[0x03D0]) >> _hqVelocityShift);
        const uint16_t vyq = (uint16_t)(((uint16_t)_lowMem[0x03E8] * 256 + _lowMem[0x03E0]) >> _hqVelocityShift);
        const uint16_t vzq = (uint16_t)(((uint16_t)_lowMem[0x03F8] * 256 + _lowMem[0x03F0]) >> _hqVelocityShift);
        hashEngine.Update(vxq);
        hashEngine.Update(vyq);
        hashEngine.Update(vzq);
      }
    }

    // Waypoint chain progress is causal history (which pivots this line has already cleared); two
    // states at the same position with different chain progress must never merge.
    if (_useWaypointMagnet) hashEngine.Update(_wpNext);
    // The void-fall persistence counter drives a fail rule but is derived from RAM HISTORY -- two
    // RAM-identical states can carry different counts, so it must be hashed or shadows leak through
    // (or into) the fence.
    hashEngine.Update(_voidFallFrames);

    // Funnel-release cohorts: riders released at different times play byte-identical trajectories
    // offset in absolute time -- and the global clock is causal (platform phases, camera). Hashing the
    // release step separates cohorts permanently while keeping within-cohort dedup intact.
    hashEngine.Update(_funnelState.exitStep);
    // Full rest: a marble at complete rest differs from its successor only by the clock. Hash the step
    // so waiting chains stay distinct (waiting is meaningful in a clock-driven game).
    const uint16_t velXMag = (uint16_t)((uint16_t)*_marbleVelXHi * 256 + (uint8_t)*_marbleVelX);
    const uint16_t velYMag = (uint16_t)((uint16_t)*_marbleVelYHi * 256 + (uint8_t)*_marbleVelY);
    // At (quantized) rest the successive states hash identically -- include the step so waiting and
    // crawling chains stay distinct. Under quantization the threshold is the velocity CELL (a crawl
    // inside one position cell with velocity bucket 0 is a wait for dedup purposes).
    const int rq = _useHashQuantization ? _hqVelocityShift : 0;
    if ((velXMag >> rq) == 0 && (velYMag >> rq) == 0 && *_marbleAirborneFlag == 0) hashEngine.Update(_currentStep);
    // hashEngine.Update(&_lowMem[0x0001], 0x0018);
    // hashEngine.Update(&_lowMem[0x001C], 0x0050);
  }

  // Updating derivative values after updating the internal state
  __INLINE__ void stateUpdatePostHook() override
  {
    _marblePosX = (float)*_marblePosX1 * 256.0f + (float)*_marblePosX2 + (float)*_marblePosX3 / 256.0f;
    _marblePosY = (float)*_marblePosY1 * 256.0f + (float)*_marblePosY2 + (float)*_marblePosY3 / 256.0f;
    _marblePosZ = (float)*_marblePosZ1;

    _surfaceAngleX = 1.0;
    _surfaceAngleY = 1.0;
    if (*_marbleSurfaceAngle == 3)
    {
      _surfaceAngleX = 1.0;
      _surfaceAngleY = 1.0;
    }
    if (*_marbleSurfaceAngle == 0)
    {
      _surfaceAngleX = 0.5;
      _surfaceAngleY = 0.5;
    }
    if (*_marbleSurfaceAngle == 1)
    {
      _surfaceAngleX = 0.5;
      _surfaceAngleY = 1.0;
    }
    if (*_marbleSurfaceAngle == 4)
    {
      _surfaceAngleX = 0.75;
      _surfaceAngleY = 1.0;
    }
    if (*_marbleSurfaceAngle == 2)
    {
      _surfaceAngleX = 1.0;
      _surfaceAngleY = 0.5;
    }
    if (*_marbleSurfaceAngle == 255)
    {
      _surfaceAngleX = -5.0;
      _surfaceAngleY = -5.0;
    }
    if (*_marbleSurfaceAngle == 128)
    {
      _surfaceAngleX = -5.0;
      _surfaceAngleY = -5.0;
    }
    // Funnel presence flag (derived, idempotent): $7B == 1 during a funnel traversal
    // (255 = ordinary airborne, 0 = grounded). The evolving counters/latches are updated in
    // advanceStateImpl -- this hook also runs on deserialize and must not mutate counters.
    _insideFunnel = (*_marbleAirborneFlag == 1) && (_lowMem[0x00DE] != 0);

    // Region-scoped hashing: inside the endgame region (the frame-precise bridge/goal choreography)
    // the dedup hash stays EXACT; outside it the quantized cells apply.
    _inExactRegion = _hqUseExactRegion && _marblePosX >= _hqExactXMin && _marblePosY <= _hqExactYMax;
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    // Resetting magnets ahead of rule re-evaluation
    _pointMagnet.intensity = 0.0;
    _pointMagnet.x         = 0.0;
    _pointMagnet.y         = 0.0;
    _stopProcessingReward  = false;

    _traceMagnet.intensity = 0.0;
    _traceMagnet.offset    = 0;

    _lastInputMagnet = 0.0;
  }

  __INLINE__ void ruleUpdatePostHook() override
  {
    // Updating distance to user-defined point
    _marbleDistanceToPointX = std::abs(_pointMagnet.x - _marblePosX);
    _marbleDistanceToPointY = std::abs(_pointMagnet.y - _marblePosY);
    _marbleDistanceToPoint  = sqrtf(_marbleDistanceToPointX * _marbleDistanceToPointX + _marbleDistanceToPointY * _marbleDistanceToPointY);

    // Updating trace stuff
    if (_useTrace == true)
    {
      _traceStep    = (uint16_t)std::max(std::min((int)_currentStep + _traceMagnet.offset, (int)_trace.size() - 1), 0);
      _traceTargetX = _trace[_traceStep].x;
      _traceTargetY = _trace[_traceStep].y;

      // Updating distance to trace point
      _traceDistanceX = std::abs(_traceTargetX - _marblePosX);
      _traceDistanceY = std::abs(_traceTargetY - _marblePosY);
      _traceDistance  = sqrtf(_traceDistanceX * _traceDistanceX + _traceDistanceY * _traceDistanceY);
    }
  }

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_lastInputStep, sizeof(_lastInputStep));
    serializer.push(&_currentStep, sizeof(_currentStep));
    serializer.push(&_prevInputIdx, sizeof(_prevInputIdx));
    serializer.push(&_prevInputRepeatedTimes, sizeof(_prevInputRepeatedTimes));
    serializer.push(&_tryNewInputs, sizeof(_tryNewInputs));
    serializer.push(&_funnelState, sizeof(_funnelState));
    serializer.push(&_wpNext, sizeof(_wpNext));
    serializer.push(&_wpMinD, sizeof(_wpMinD));
    serializer.push(&_voidFallFrames, sizeof(_voidFallFrames));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer)
  {
    deserializer.pop(&_lastInputStep, sizeof(_lastInputStep));
    deserializer.pop(&_currentStep, sizeof(_currentStep));
    deserializer.pop(&_prevInputIdx, sizeof(_prevInputIdx));
    deserializer.pop(&_prevInputRepeatedTimes, sizeof(_prevInputRepeatedTimes));
    deserializer.pop(&_tryNewInputs, sizeof(_tryNewInputs));
    deserializer.pop(&_funnelState, sizeof(_funnelState));
    deserializer.pop(&_wpNext, sizeof(_wpNext));
    deserializer.pop(&_wpMinD, sizeof(_wpMinD));
    deserializer.pop(&_voidFallFrames, sizeof(_voidFallFrames));
  }

  __INLINE__ float calculateGameSpecificReward() const
  {
    // Getting rewards from rules
    float reward = 0.0;

    // Subtracting reward for having made an input recently (for early termination)
    reward += _lastInputMagnet * _lastInputStep;

    // Continuous last-input pressure (config "Last Input Step Reward", typically negative): rewards
    // coasting lines at every step, not just at the win. Without it, a line that stops steering early
    // to glide into the goal scores worse on the instantaneous trace metric than greedy still-steering
    // lines and gets evicted from a full state DB before its win can pay out.
    reward += _lastInputStepReward * _lastInputStep;

    // If this is a win state, then evaluate only w.r.t. how long since the last input
    if (_stopProcessingReward) return reward;

    // Distance to point magnet
    reward += -1.0 * _pointMagnet.intensity * _marbleDistanceToPoint;

    // If trace is used, compute its magnet's effect
    if (_useTrace == true) reward += -1.0 * _traceMagnet.intensity * _traceDistance;

    // Funnel rewards: entry/exit latches persist; the per-frame term counts ONLY during traversal
    // (after exit the PM resumes and carries the entry-time gradient through position).
    if (_useFunnelReward)
    {
      reward += _funnelEnterReward * (float)_funnelState.entered + _funnelExitReward * (float)_funnelState.exited;
      // Latched accumulator (monotone): scripted-transit frames pay permanently. Gating this on being
      // currently inside made the whole accumulated sum re-fire as a one-frame spike at every later
      // single-frame scripted event ($7B==1 also blips at ledge drops), breaking monotonicity.
      reward += _funnelFrameReward * (float)_funnelState.frames;
    }

    // Waypoint magnet: discrete pay per collected box plus attraction toward the next box's center.
    // The distance term is optionally quantized into plateaus so schedule waits (position oscillating
    // by a few pixels) hold reward flat instead of dipping. Box Reward must exceed the retarget jump
    // (intensity x longest leg) so every collection is a net reward INCREASE along a good line.
    if (_useWaypointMagnet)
    {
      reward += _wpPrefix[_wpNext];
      if (_wpNext < _wpBoxes.size())
      {
        const auto& b  = _wpBoxes[_wpNext];
        const float dx = _marblePosX - b.cx;
        const float dy = _marblePosY - b.cy;
        // Include the live distance so the initial state (ratchet not yet primed) reads correctly;
        // min() keeps the term the ratchet (monotone) everywhere else.
        const float dLive = sqrtf(dx * dx + dy * dy);
        float       d     = _wpUseRatchet ? std::min(_wpMinD, dLive) : dLive;
        if (_wpDistanceQuantum > 0.0f) d = std::floor(d / _wpDistanceQuantum) * _wpDistanceQuantum;
        reward += -_wpIntensity * d;
      }
    }

    // Progress magnet: alpha * (frames ahead on the corridor) - beta * (phase-space deviation from it).
    // Lead credit is GATED on actually being on the corridor: beyond the deviation gate the state gets no
    // lead term and a sinking penalty instead. Without the gate, geometry exploits the matcher -- the course
    // is a tight serpentine, so an off-route state (a pit, an adjacent lane) can sit near a reference index
    // far ahead and collect a huge false lead at modest deviation cost.
    // Fully suppressed inside a funnel: position is scripted there, so the matcher is degenerate (all
    // transit indices tie at deviation 0 and the tie-break grants a spurious full-window lead).
    if (_usePMagnet == true && _insideFunnel == false)
    {
      int   jBest = -1;
      float cBest = 0.0f;
      progressMatch(jBest, cBest);
      if (jBest >= 0)
      {
        if (cBest <= _pmDeviationGate)
          reward += _pmIntensity * (float)(jBest - (int)_currentStep) - _pmDeviationWeight * cBest;
        else
          reward += -_pmOffCorridorPenalty - _pmDeviationWeight * cBest;
      }
    }

    // Returning reward
    return reward;
  }

  /// @brief Finds the reference index (within the step window) best matching the marble's current
  ///        phase-space state (position + true 16-bit signed velocity + height + airborne flag).
  ///        Ties in cost break toward the LARGEST index, so waiting plateaus credit full progress.
  __INLINE__ void progressMatch(int& jBest, float& cBest) const
  {
    // Velocity is sign-magnitude: unsigned 16-bit magnitude (hi:lo), direction flag 1 = negative.
    const float   vx  = (float)((int)*_marbleVelXHi * 256 + (int)(uint8_t)*_marbleVelX) * (*_marbleMotion1 != 0 ? -1.0f : 1.0f);
    const float   vy  = (float)((int)*_marbleVelYHi * 256 + (int)(uint8_t)*_marbleVelY) * (*_marbleMotion2 != 0 ? -1.0f : 1.0f);
    const float   z   = (float)*_marblePosZ1;
    const uint8_t air = (*_marbleAirborneFlag != 0) ? 1 : 0;

    const int jLo = std::max((int)_currentStep - _pmWindowBehind, 0);
    const int jHi = std::min((int)_currentStep + _pmWindowAhead, (int)_ptrace.size() - 1);
    jBest         = -1;
    cBest         = 0.0f;
    for (int j = jLo; j <= jHi; j++)
    {
      const auto& e   = _ptrace[j];
      const float dx  = _marblePosX - e.x;
      const float dy  = _marblePosY - e.y;
      const float dvx = vx - e.vx;
      const float dvy = vy - e.vy;
      float       c   = sqrtf(dx * dx + dy * dy) / _pmPositionScale;
      // Inside a funnel the motion is scripted and the velocity bytes are meaningless -- match on
      // position (+Z) only, or riders would be spuriously flagged off-corridor.
      if (_insideFunnel == false)
      {
        c += sqrtf(dvx * dvx + dvy * dvy) / _pmVelocityScale;
        if (air != e.air) c += _pmAirbornePenalty;
      }
      c += _pmZWeight * std::abs(z - e.z);
      if (jBest < 0 || c <= cBest)
      {
        jBest = j;
        cBest = c;
      } // <= : ties go to the largest j
    }
  }

  void printInfoImpl() const override
  {
    jaffarCommon::logger::log("[Jaffar]  + Game Timer:             %02u\n", *_gameTimer);
    jaffarCommon::logger::log("[Jaffar]  + Game Cycle:             %02u\n", *_gameCycle);
    jaffarCommon::logger::log("[Jaffar]  + Marble State:           %02u\n", *_marbleState);
    jaffarCommon::logger::log("[Jaffar]  + Marble Flags:           %02u\n", *_marbleFlags);
    jaffarCommon::logger::log("[Jaffar]  + Marble Pos X:           %f, (%02u, %02u, %02u)\n", _marblePosX, *_marblePosX1, *_marblePosX2, *_marblePosX3);
    jaffarCommon::logger::log("[Jaffar]  + Marble Pos Y:           %f, (%02u, %02u, %02u)\n", _marblePosY, *_marblePosY1, *_marblePosY2, *_marblePosY3);
    jaffarCommon::logger::log("[Jaffar]  + Marble Pos Z:           %f\n", _marblePosZ);
    jaffarCommon::logger::log("[Jaffar]  + Marble Vel X:           %02d\n", *_marbleVelX);
    jaffarCommon::logger::log("[Jaffar]  + Marble Vel Y:           %02d\n", *_marbleVelY);
    jaffarCommon::logger::log("[Jaffar]  + Marble Airtime:         %02u\n", *_marbleAirtime);
    jaffarCommon::logger::log("[Jaffar]  + Marble Surface Angle:   %02u\n", *_marbleSurfaceAngle);

    if (_usePMagnet == true)
    {
      int   jBest = -1;
      float cBest = 0.0f;
      progressMatch(jBest, cBest);
      jaffarCommon::logger::log("[Jaffar]  + Progress Magnet:        lead %+d frames (ref idx %d), deviation %.3f\n", jBest - (int)_currentStep, jBest, cBest);
    }
    if (_useWaypointMagnet)
    {
      if (_wpNext < _wpBoxes.size())
      {
        const auto& b  = _wpBoxes[_wpNext];
        const float dx = _marblePosX - b.cx;
        const float dy = _marblePosY - b.cy;
        jaffarCommon::logger::log("[Jaffar]  + Waypoint Magnet:        box %u / %zu collected, next center (%.0f,%.0f), distance %.2f\n", _wpNext, _wpBoxes.size(), b.cx, b.cy,
                                  sqrtf(dx * dx + dy * dy));
      }
      else
        jaffarCommon::logger::log("[Jaffar]  + Waypoint Magnet:        all %zu boxes collected\n", _wpBoxes.size());
    }
    jaffarCommon::logger::log("[Jaffar]  + Prev Input:             %02u\n", _prevInputIdx);
    jaffarCommon::logger::log("[Jaffar]  + Prev Input Repetitions: %02u / %02u (Try new: %s)\n", _prevInputRepeatedTimes, _repeatPrevInputCount, _tryNewInputs ? "Yes" : "No");

    if (std::abs(_pointMagnet.intensity) > 0.0f)
    {
      jaffarCommon::logger::log("[J+]  + Point Magnet                             Intensity: %.5f, X: %3.3f, Y: %3.3f\n", _pointMagnet.intensity, _pointMagnet.x, _pointMagnet.y);
      jaffarCommon::logger::log("[J+]    + Distance X                             %3.3f\n", _marbleDistanceToPointX);
      jaffarCommon::logger::log("[J+]    + Distance Y                             %3.3f\n", _marbleDistanceToPointY);
      jaffarCommon::logger::log("[J+]    + Total Distance                         %3.3f\n", _marbleDistanceToPoint);
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

    if (std::abs(_lastInputMagnet) > 0.0f) jaffarCommon::logger::log("[J+]  + Last Input Magnet                      Intensity: %.5f\n", _lastInputMagnet);
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

    if (actionType == "Set Trace Magnet")
    {
      if (_useTrace == false) JAFFAR_THROW_LOGIC("Specified Trace Magnet, but no trace file was provided.");
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto offset    = jaffarCommon::json::getNumber<int>(actionJs, "Offset");
      rule.addAction([=, this]() { this->_traceMagnet = traceMagnet_t{.intensity = intensity, .offset = offset}; });
      recognizedActionType = true;
    }

    if (actionType == "Set Last Input Magnet")
    {
      auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      auto action    = [=, this]() { this->_lastInputMagnet = intensity; };
      rule.addAction(action);
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
  struct traceMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    int   offset    = 0;   // Which entry (step) to look at wrt the current emulation step
  };

  traceMagnet_t _traceMagnet;

  // Datatype to describe a point magnet
  struct pointMagnet_t
  {
    float intensity = 0.0; // How strong the magnet is
    float x         = 0.0; // What is the x point of attraction
    float y         = 0.0; // What is the y point of attraction
  };

  bool        _isDumpingTrace = false;
  std::string _traceDumpString;

  __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) override
  {
    if (_tryNewInputs == false) allowedInputSet.push_back(_prevInputIdx);
  }

  __INLINE__ void playerPrintCommands() const override { jaffarCommon::logger::log("[J+] t: start/stop trace dumping (%s)\n", _isDumpingTrace ? "On" : "Off"); };

  __INLINE__ bool playerParseCommand(const int command)
  {
    // If storing a trace, do it here
    if (_isDumpingTrace == true) _traceDumpString += std::to_string(_marblePosX) + std::string(" ") + std::to_string(_marblePosY) + std::string("\n");

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

  // Magnets (used to determine state reward and have Jaffar favor a direction or action)
  pointMagnet_t _pointMagnet;
  float         _lastInputMagnet = 0.0;

  // Container for game-specific values
  uint8_t* _gameTimer;
  uint8_t* _gameCycle;
  uint8_t* _winFlag;
  uint8_t* _marbleState;
  uint8_t* _marbleFlags;
  uint8_t* _marblePosX1;
  uint8_t* _marblePosX2;
  uint8_t* _marblePosX3;
  uint8_t* _marblePosY1;
  uint8_t* _marblePosY2;
  uint8_t* _marblePosY3;
  uint8_t* _marblePosZ1;
  uint8_t* _marbleAirtime;
  int8_t*  _marbleVelX;
  int8_t*  _marbleVelY;
  uint8_t* _marbleDeadFlag;
  uint8_t* _marbleSurfaceAngle;
  uint8_t* _pauseState;
  uint8_t* _marbleVelXHi;
  uint8_t* _marbleVelYHi;
  uint8_t* _marbleMotion1;
  uint8_t* _marbleMotion2;
  uint8_t* _marbleAirborneFlag;

  // Progress Magnet configuration/state
  bool  _usePMagnet = false;
  float _pmIntensity;
  float _pmDeviationWeight;
  int   _pmWindowBehind;
  int   _pmWindowAhead;
  float _pmPositionScale;
  float _pmVelocityScale;
  float _pmZWeight;
  float _pmAirbornePenalty;
  float _pmDeviationGate;
  float _pmOffCorridorPenalty;
  bool  _useHashQuantization = false;
  int   _hqVelocityShift     = 0;
  int   _hqPositionShift     = 0;
  bool  _hqUseExactRegion    = false;
  float _hqExactXMin         = 0.0f;
  float _hqExactYMax         = 0.0f;
  bool  _inExactRegion       = false;
  bool  _hqHashSubpixels     = true;

  // Waypoint Magnet configuration/state
  bool                 _useWaypointMagnet = false;
  bool                 _wpUseRatchet      = true;
  float                _wpIntensity       = 0.0f;
  float                _wpBoxReward       = 0.0f;
  float                _wpDistanceQuantum = 0.0f;
  std::vector<wpBox_t> _wpBoxes;
  std::vector<float>   _wpPrefix; ///< Cumulative box reward for having collected boxes 0..i-1
  uint8_t              _wpNext = 0;
  float                _wpMinD = std::numeric_limits<float>::max(); ///< Ratchet: closest approach to the current target so far

  /// Consecutive frames of scripted motion ($7B==1) without the funnel countdown -- a committed void fall when sustained
  uint8_t _voidFallFrames = 0;

  bool                       _useFunnelReward = false;
  float                      _funnelEnterReward;
  float                      _funnelFrameReward;
  float                      _funnelExitReward;
  funnelState_t              _funnelState;
  bool                       _insideFunnel = false;
  std::vector<ptraceEntry_t> _ptrace;

  float   _marbleDistanceToPointX;
  float   _marbleDistanceToPointY;
  float   _marbleDistanceToPoint;
  uint8_t _prevPauseState;

  float _marblePosX;
  float _marblePosY;
  float _marblePosZ;
  float _surfaceAngleX;
  float _surfaceAngleY;

  // Pointer to emulator's low memory storage
  uint8_t* _lowMem;

  // Reward for the last time an input was made (for early termination)
  float _lastInputStepReward;

  // Specifies whether the reward should continue to be processed (for early termination)
  bool _stopProcessingReward;

  // Null input index to remember the last valid input
  InputSet::inputIndex_t _nullInputIdx;

  // Additions to make the last input as soon as possible
  uint16_t _lastInputStep = 0;
  uint16_t _currentStep   = 0;

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

  // What was the prev input
  InputSet::inputIndex_t _prevInputIdx;

  // How many times the prev input is repeated
  uint16_t _prevInputRepeatedTimes;

  // How many times to repeat the prev input for
  uint16_t _repeatPrevInputCount;

  // Flag that indicates whether the engine should try all possible inputs
  bool _tryNewInputs;
};

} // namespace nes

} // namespace games

} // namespace jaffarPlus
