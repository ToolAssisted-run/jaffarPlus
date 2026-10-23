#pragma once

#include <cmath>
#include <emulator.hpp>
#include <game.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <machine.h>
#include <string>

namespace jaffarPlus
{

namespace games
{

namespace dos
{

/**
 * 4D Sports Boxing (Distinctive Software, DOS 1991), its MCGA build, through the QuickerOpen4DBoxing emulator: a fight
 * of the Main Event, the player's boxer the blue corner's (human), the opponent the red corner's (the computer).
 *
 * The fight's data are the program's data segment's (DS 3173h; Open4DBoxing's docs/FINDINGS.md): the boxers' structures
 * at DS:BA12 (blue) and DS:BB06 (red), 0xF4 bytes each (their place and heading, the stamina, health and endurance,
 * 0x4000 fresh, the knockdown, the moves and punches in progress); their round statistics at DS:A488 and DS:A4FC (0x74
 * bytes: punches thrown and landed in the round and the fight, knockdowns in the round, the fight lost by
 * disqualification, TKO or KO); the round's clock (DS:CF02, milliseconds), the referee's count (DS:CF12), the round
 * (DS:9302); the controls' bytes as the game read them (DS:8A40 blue, 8A41 red); the C library's random number
 * (DS:8472).
 *
 * The game's rules (the program's code): a punch costs its thrower stamina when it resolves (lands, is blocked or
 * absorbed by the guard; one that misses altogether costs nothing and is not counted thrown); a landed punch's damage
 * grows with the thrower's stamina; a boxer whose health falls below 1 is floored; the computer gets up at the count of
 * 9 (it presses its buttons then); getting up from the third knockdown of a round is a TKO; the count of 10 a KO; the
 * standing boxer is warned, docked and disqualified if he does not back away during the count.
 *
 * Game Configuration (all optional):
 *   "Hash Stack"   (true) the program's stack from its stack pointer up (its locals where it stopped) and its call chain
 *                  in the state hash
 */
class SportsBoxing4D final : public jaffarPlus::Game
{
public:
  static __INLINE__ std::string getName() { return "DOS / 4D Sports Boxing"; }

  SportsBoxing4D(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : jaffarPlus::Game(std::move(emulator), config)
  {
    if (_gameConfigRemaining.contains("Hash Stack")) _hashStack = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Hash Stack");
  }

private:
  static constexpr uint16_t BLUE = 0xBA12, RED = 0xBB06, BOXER_SIZE = 0xF4;
  static constexpr uint16_t BLUE_STATS = 0xA488, RED_STATS = 0xA4FC, STATS_SIZE = 0x74;
  static constexpr uint16_t STACK_TOP = 0xE5A8; // (the program's stack: SS = DS, 3672:1388 in the EXE's terms)

  template <typename T> __INLINE__ T* ds(uint16_t off) const { return (T*)(_ds + off); }

  void registerBoxer(const std::string& name, uint16_t b, uint16_t st, uint16_t input)
  {
    using dt      = Property::datatype_t;
    const auto le = Property::endianness_t::little;
    registerGameProperty(name + " Heading", ds<uint16_t>(b + 0x04), dt::dt_uint16, le);
    registerGameProperty(name + " Pos X", ds<int16_t>(b + 0x06), dt::dt_int16, le);
    registerGameProperty(name + " Pos Y", ds<int16_t>(b + 0x08), dt::dt_int16, le);
    registerGameProperty(name + " Pos Z", ds<int16_t>(b + 0x0A), dt::dt_int16, le);
    registerGameProperty(name + " Down", ds<uint16_t>(b + 0x0C), dt::dt_uint16, le);
    registerGameProperty(name + " Stamina", ds<int16_t>(b + 0x0E), dt::dt_int16, le);
    registerGameProperty(name + " Health", ds<int16_t>(b + 0x10), dt::dt_int16, le);
    registerGameProperty(name + " Endurance", ds<int16_t>(b + 0x12), dt::dt_int16, le);
    // the posture (1eb6:2ee2: 1 standing; 3, 4, 5 floored; 7, 8 getting up) and the endurance's ceiling (its fight
    // block's: the health comes back to 0x4000 less half the endurance lost, the stamina to 0x4000 less a quarter; a
    // boxer below his ceiling's health loses endurance every tick, (ceiling - health) / 256, / 64 below 0)
    registerGameProperty(name + " Posture", ds<uint16_t>(b + 0x16), dt::dt_uint16, le);
    registerGameProperty(name + " State", ds<uint16_t>(b + 0x2C), dt::dt_uint16, le);
    // a punch reaching out (1eb6:298c: its button let go before phase 8 pulls it back; no new punch meanwhile), the
    // punch's definition (DS:5D16 + 16 k) and its animation's record (its phase at +0x10)
    registerGameProperty(name + " Punching", ds<uint16_t>(b + 0x28), dt::dt_uint16, le);
    registerGameProperty(name + " Punch Definition", ds<uint16_t>(b + 0x30), dt::dt_uint16, le);
    registerGameProperty(name + " Guard", ds<uint16_t>(b + 0x40), dt::dt_uint16, le);
    // the lean (1eb6:2864: -0x2000 and up): a punch at a boxer is stopped by 0x1800 (0x1E00 guarded) times 0.5 plus
    // its lean's size (1eb6:19fa), and lands only when its own force is more
    registerGameProperty(name + " Lean", ds<int16_t>(b + 0x48), dt::dt_int16, le);
    registerGameProperty(name + " Input", ds<uint8_t>(input), dt::dt_uint8, le);        // (1 up 2 down 4 right 8 left, 0x20 0x10 the buttons)
    registerGameProperty(name + " Round Thrown", ds<uint16_t>(st + 0x00), dt::dt_uint16, le);
    registerGameProperty(name + " Round Landed", ds<uint16_t>(st + 0x02), dt::dt_uint16, le);
    registerGameProperty(name + " Thrown", ds<uint16_t>(st + 0x06), dt::dt_uint16, le);
    registerGameProperty(name + " Landed", ds<uint16_t>(st + 0x08), dt::dt_uint16, le);
    registerGameProperty(name + " Round Knockdowns", ds<uint16_t>(st + 0x0A), dt::dt_uint16, le);
    registerGameProperty(name + " Lost By DQ", ds<uint16_t>(st + 0x6E), dt::dt_uint16, le);
    registerGameProperty(name + " Lost By TKO", ds<uint16_t>(st + 0x70), dt::dt_uint16, le);
    registerGameProperty(name + " Lost By KO", ds<uint16_t>(st + 0x72), dt::dt_uint16, le);
  }

  __INLINE__ void registerGameProperties() override
  {
    using dt      = Property::datatype_t;
    const auto le = Property::endianness_t::little;
    _ds           = _emulator->getProperty("Data Segment").pointer;
    _memory       = _emulator->getProperty("Memory").pointer;

    registerBoxer("Player", BLUE, BLUE_STATS, 0x8A40);
    registerBoxer("Opponent", RED, RED_STATS, 0x8A41);
    registerGameProperty("Round", ds<uint16_t>(0x9302), dt::dt_uint16, le);
    registerGameProperty("Rounds", ds<uint16_t>(0x5EDC), dt::dt_uint16, le);           // (15 - 12 x the opponent's rank / 50)
    registerGameProperty("Round Length", ds<uint32_t>(0x5ED8), dt::dt_uint32, le);     // (milliseconds)
    // the judges' cards (1eb6:0fd4 at each round's end: the round's landed punches, each with a judge's noise of 25%,
    // their share 10-6 down to 6-10, 0-0 when neither landed; less a point a knockdown and a deduction)
    for (int judge = 0; judge < 3; judge++)
    {
      registerGameProperty("Player Judge " + std::to_string(judge + 1), ds<int16_t>(0xA4EE + 2 * judge), dt::dt_int16, le);
      registerGameProperty("Opponent Judge " + std::to_string(judge + 1), ds<int16_t>(0xA562 + 2 * judge), dt::dt_int16, le);
    }
    registerGameProperty("Round Clock", ds<uint32_t>(0xCF02), dt::dt_uint32, le);
    registerGameProperty("Round Over", ds<uint16_t>(0xCF0A), dt::dt_uint16, le);
    registerGameProperty("Fight Over", ds<uint16_t>(0xCF0C), dt::dt_uint16, le);
    registerGameProperty("Count", ds<uint16_t>(0xCF12), dt::dt_uint16, le);
    registerGameProperty("Count Warnings", ds<uint16_t>(0xCF14), dt::dt_uint16, le);
    registerGameProperty("Boxer Distance", ds<int16_t>(0xCF54), dt::dt_int16, le);
    registerGameProperty("Result", ds<int16_t>(0xB0D0), dt::dt_int16, le);
    registerGameProperty("Winner", ds<int16_t>(0xD208), dt::dt_int16, le);
    registerGameProperty("Random", ds<uint32_t>(0x8472), dt::dt_uint32, le);
    // (the game class's own, stateUpdatePostHook: the opponent's health taken since the search began, the player's)
    registerGameProperty("Opponent Damage", &_opponentDamage, dt::dt_int32, le);
    registerGameProperty("Player Damage", &_playerDamage, dt::dt_int32, le);
    registerGameProperty("Player Wasted Punches", &_wastedPunches, dt::dt_int32, le); // (resolved but not landed)
    registerGameProperty("Step Frames", &_stepFrames, dt::dt_uint32, le);

    _playerHealth   = ds<int16_t>(BLUE + 0x10);
    _opponentHealth = ds<int16_t>(RED + 0x10);
    _playerThrown   = ds<uint16_t>(BLUE_STATS + 0x06);
    _playerLanded   = ds<uint16_t>(BLUE_STATS + 0x08);

    _nullInputIdx = _emulator->registerInput("|  160,  100,.........................................................|");
  }

  __INLINE__ InputSet::inputIndex_t getNullInputIndex() const override { return _nullInputIdx; }

  __INLINE__ void advanceStateImpl(const InputSet::inputIndex_t input) override { _emulator->advanceState(input); }

  __INLINE__ void computeAdditionalHashing(MetroHash128& hashEngine) const override
  {
    hashEngine.Update(_ds + BLUE, BOXER_SIZE);
    hashEngine.Update(_ds + RED, BOXER_SIZE);
    hashEngine.Update(_ds + BLUE_STATS, STATS_SIZE);
    hashEngine.Update(_ds + RED_STATS, STATS_SIZE);
    // the rest of what decides the fight (the influence test, tests/o4dbInfluence: a region of a later frame put into a
    // fight's state changes the boxers' future): the random number, the computer's controls (234a:0012's memory), the
    // controls as read, their replay pointers and counts and their buffers' ends, the controls' sampling accumulator,
    // the moves requested (1eb6:01c4: B136 blue, BD10 red), the boxers' pose blocks, the round and the referee
    hashEngine.Update(_ds + 0x5A1C, 4);
    hashEngine.Update(_ds + 0x8472, 4);
    hashEngine.Update(_ds + 0x89F6, 10);
    hashEngine.Update(_ds + 0x8A40, 2);
    hashEngine.Update(_ds + 0x8CC8, 16);
    hashEngine.Update(_ds + 0x8E3E, 8);
    hashEngine.Update(_ds + 0x9302, 2);
    hashEngine.Update(_ds + 0xA6B0, 16);
    hashEngine.Update(_ds + 0xAEC0, 2);
    hashEngine.Update(_ds + 0xB130, 16);
    hashEngine.Update(_ds + 0xB6B8, 0x48);
    hashEngine.Update(_ds + 0xBD08, 12);
    hashEngine.Update(_ds + 0xCC58, 4);
    hashEngine.Update(_ds + 0xCF02, 0x60);
    // the controls recorded but not yet read (the fight's replay: the timer's reads write a boxer's byte, far pointer
    // DS:8E3E blue, 8E42 red; his steps read it later, DS:8CC8, 8CD0): a step's input is only that until the next
    // step reads it (the step ends where the controls are read)
    for (int boxer = 0; boxer < 2; boxer++)
    {
      const uint16_t readOff = *ds<uint16_t>(0x8CC8 + 8 * boxer), readSeg = *ds<uint16_t>(0x8CCA + 8 * boxer);
      const uint16_t writeOff = *ds<uint16_t>(0x8E3E + 4 * boxer), writeSeg = *ds<uint16_t>(0x8E40 + 4 * boxer);
      if (readSeg != writeSeg || writeOff <= readOff || writeOff - readOff > 16) continue;
      hashEngine.Update(_memory + ((uint32_t)readSeg << 4) + readOff, writeOff - readOff);
    }
    if (_hashStack)
    {
      int        n     = 0;
      const auto chain = o4db_chain(&n);
      hashEngine.Update((const uint8_t*)chain, (size_t)n * 8);
      const uint16_t sp = o4db_stack_pointer();
      if (sp < STACK_TOP) hashEngine.Update(_ds + sp, STACK_TOP - sp);
    }
  }

  __INLINE__ void stateUpdatePostHook() override
  {
    // the health taken since the last step (its regeneration not counted back)
    const int16_t oh = *_opponentHealth, ph = *_playerHealth;
    if (_started && oh < _lastOpponentHealth) _opponentDamage += _lastOpponentHealth - oh;
    if (_started && ph < _lastPlayerHealth) _playerDamage += _lastPlayerHealth - ph;
    _lastOpponentHealth = oh;
    _lastPlayerHealth   = ph;
    _wastedPunches      = (int32_t)*_playerThrown - (int32_t)*_playerLanded;
    _stepFrames         = (uint32_t)((emulator::QuickerOpen4DBoxing*)_emulator.get())->getLastStepFrames();
    _started            = 1;
  }

  __INLINE__ void ruleUpdatePreHook() override
  {
    _opponentDamageMagnet = 0.0f;
    _opponentHealthMagnet = 0.0f;
    _playerStaminaMagnet  = 0.0f;
    _distanceMagnet       = {0.0f, 0.0f};
    _fightMagnet          = fightMagnet_t{};
    _survivalMagnet       = survivalMagnet_t{};
  }

  // The fight's reward (the "Set Fight Magnet" action), from the state as it is now (rules latch, this does not): the
  // opponent's knockdowns in this round (a TKO is the third); while he stands, his health taken (it comes back, so
  // what counts is what he lacks now) and the distance to him beyond the punching range; while he is down, the
  // distance to the count's range (the referee warns, docks and disqualifies a boxer within 500 from count 3 on), so
  // that the player waits there to meet him when he gets up; the player's stamina (a punch's damage grows with it);
  // the opponent's endurance lost (it lowers his health's ceiling and its return, and comes back by 2 a tick only);
  // and, as costs, the health the player lost and his wasted punches (blocked or absorbed: they cost stamina)
  __INLINE__ float fightReward() const
  {
    const auto& m = _fightMagnet;
    if (m.knockdownWeight == 0.0f && m.healthWeight == 0.0f && m.rangeWeight == 0.0f && m.staminaWeight == 0.0f && m.enduranceWeight == 0.0f &&
        m.damageWeight == 0.0f && m.wastedWeight == 0.0f)
      return 0.0f;
    const float knockdowns = (float)*ds<uint16_t>(RED_STATS + 0x0A);
    const bool  down       = *ds<uint16_t>(RED + 0x0C) != 0;
    const float distance   = (float)*ds<int16_t>(0xCF54);
    float       r          = m.knockdownWeight * knockdowns;
    if (down)
      r -= m.rangeWeight * std::abs(distance - m.countRange);
    else
    {
      r += m.healthWeight * (float)(0x4000 - std::max<int>(0, *ds<int16_t>(RED + 0x10)));
      r -= m.rangeWeight * std::max(0.0f, distance - m.range);
    }
    r += m.staminaWeight * (float)*ds<int16_t>(BLUE + 0x0E);
    r += m.enduranceWeight * (float)(*(const int16_t*)(_ds + *ds<uint16_t>(RED) + 0x14) - *ds<int16_t>(RED + 0x12));
    r -= m.damageWeight * (float)_playerDamage;
    r -= m.wastedWeight * (float)_wastedPunches;
    return r;
  }

  // The survival's reward (the "Set Survival Magnet" action), from the state as it is now: the distance to the
  // opponent (up to a cap: out of his reach), the player's distance from the ring's middle (room to step away, as a
  // cost), the punches landed in the round (up to a cap: one wins the judges' round 10-6 while he lands none), and
  // the player's lean (+0x48, the diagonals' 1eb6:2864, up to 0x2000 either way) while he is nearer than a range:
  // his punches miss a head leaned away, and the lean must be there before them (2 to 6 steps from a punch's start
  // to its end, the lean's command only at the controls' next reading); and a bonus at a safe distance or more (his
  // punches only nearer than 420, 234a:0012: there he does not punch, and the player's land from up to about 450)
  __INLINE__ float survivalReward() const
  {
    const auto& m = _survivalMagnet;
    if (m.distanceWeight == 0.0f && m.centerWeight == 0.0f && m.landedWeight == 0.0f && m.leanWeight == 0.0f && m.safeWeight == 0.0f) return 0.0f;
    const float distance = (float)*ds<int16_t>(0xCF54);
    const float x = (float)*ds<int16_t>(BLUE + 0x06), z = (float)*ds<int16_t>(BLUE + 0x0A);
    const float landed = (float)*ds<uint16_t>(BLUE_STATS + 0x02);
    float r = m.distanceWeight * std::min(distance, m.distanceCap);
    r -= m.centerWeight * std::sqrt(x * x + z * z);
    r += m.landedWeight * std::min(landed, m.landedCap);
    if (m.leanWeight != 0.0f && distance < m.leanRange)
      r += m.leanWeight * std::min(std::fabs((float)*ds<int16_t>(BLUE + 0x48)), 8192.0f) / 8192.0f;
    if (m.safeWeight != 0.0f && distance >= m.safeDistance) r += m.safeWeight;
    return r;
  }

  __INLINE__ void ruleUpdatePostHook() override {}

  __INLINE__ void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const override
  {
    serializer.push(&_opponentDamage, sizeof(_opponentDamage));
    serializer.push(&_playerDamage, sizeof(_playerDamage));
    serializer.push(&_lastOpponentHealth, sizeof(_lastOpponentHealth));
    serializer.push(&_lastPlayerHealth, sizeof(_lastPlayerHealth));
    serializer.push(&_started, sizeof(_started));
  }

  __INLINE__ void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(&_opponentDamage, sizeof(_opponentDamage));
    deserializer.pop(&_playerDamage, sizeof(_playerDamage));
    deserializer.pop(&_lastOpponentHealth, sizeof(_lastOpponentHealth));
    deserializer.pop(&_lastPlayerHealth, sizeof(_lastPlayerHealth));
    deserializer.pop(&_started, sizeof(_started));
  }

  __INLINE__ float calculateGameSpecificReward() const override
  {
    float reward = 0.0f;
    reward += _opponentDamageMagnet * (float)_opponentDamage;
    reward += _opponentHealthMagnet * (float)(0x4000 - *_opponentHealth);
    reward += _playerStaminaMagnet * (float)*ds<int16_t>(BLUE + 0x0E);
    if (_distanceMagnet.intensity != 0.0f) reward += _distanceMagnet.intensity * -std::abs((float)*ds<int16_t>(0xCF54) - _distanceMagnet.center);
    reward += fightReward();
    reward += survivalReward();
    return reward;
  }

  void printInfoImpl() const override
  {
    auto boxer = [&](const char* name, uint16_t b, uint16_t st)
    {
      jaffarCommon::logger::log("[J+]  + %-8s pos (%5d, %5d) heading %4u, stamina %5d, health %5d, endurance %5d, down %u state %u, thrown %u landed %u, knockdowns %u\n", name,
                                *ds<int16_t>(b + 6), *ds<int16_t>(b + 0xA), *ds<uint16_t>(b + 4), *ds<int16_t>(b + 0xE), *ds<int16_t>(b + 0x10), *ds<int16_t>(b + 0x12),
                                *ds<uint16_t>(b + 0xC), *ds<uint16_t>(b + 0x2C), *ds<uint16_t>(st + 6), *ds<uint16_t>(st + 8), *ds<uint16_t>(st + 0xA));
    };
    boxer("Player", BLUE, BLUE_STATS);
    boxer("Opponent", RED, RED_STATS);
    jaffarCommon::logger::log("[J+]  + Round %u, clock %u ms, count %u (warnings %u), distance %d; opponent damage %d, player damage %d, wasted punches %d; step frames %u\n",
                              *ds<uint16_t>(0x9302), *ds<uint32_t>(0xCF02), *ds<uint16_t>(0xCF12), *ds<uint16_t>(0xCF14), *ds<int16_t>(0xCF54), _opponentDamage, _playerDamage,
                              _wastedPunches, _stepFrames);
    if (_opponentDamageMagnet != 0.0f) jaffarCommon::logger::log("[J+]  + Opponent Damage Magnet: %f\n", _opponentDamageMagnet);
    if (_opponentHealthMagnet != 0.0f) jaffarCommon::logger::log("[J+]  + Opponent Health Magnet: %f\n", _opponentHealthMagnet);
    if (_playerStaminaMagnet != 0.0f) jaffarCommon::logger::log("[J+]  + Player Stamina Magnet: %f\n", _playerStaminaMagnet);
    if (_distanceMagnet.intensity != 0.0f) jaffarCommon::logger::log("[J+]  + Distance Magnet: intensity %f, center %f\n", _distanceMagnet.intensity, _distanceMagnet.center);
    if (survivalReward() != 0.0f)
      jaffarCommon::logger::log("[J+]  + Survival Magnet: %f (distance x %g up to %g, from the middle x %g, landed x %g up to %g, lean x %g nearer than %g, %g from %g)\n",
                                survivalReward(), _survivalMagnet.distanceWeight, _survivalMagnet.distanceCap, _survivalMagnet.centerWeight, _survivalMagnet.landedWeight,
                                _survivalMagnet.landedCap, _survivalMagnet.leanWeight, _survivalMagnet.leanRange, _survivalMagnet.safeWeight, _survivalMagnet.safeDistance);
    if (fightReward() != 0.0f)
      jaffarCommon::logger::log("[J+]  + Fight Magnet: %f (knockdowns x %g, health x %g, range %g x %g, count range %g, stamina x %g, endurance x %g, damage x %g, wasted x %g)\n",
                                fightReward(), _fightMagnet.knockdownWeight, _fightMagnet.healthWeight, _fightMagnet.range, _fightMagnet.rangeWeight, _fightMagnet.countRange,
                                _fightMagnet.staminaWeight, _fightMagnet.enduranceWeight, _fightMagnet.damageWeight, _fightMagnet.wastedWeight);
  }

  bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) override
  {
    bool recognizedActionType = false;
    if (actionType == "Set Opponent Damage Magnet")
    {
      const auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_opponentDamageMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Opponent Health Magnet")
    {
      const auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_opponentHealthMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Player Stamina Magnet")
    {
      const auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      rule.addAction([=, this]() { this->_playerStaminaMagnet = intensity; });
      recognizedActionType = true;
    }
    if (actionType == "Set Fight Magnet")
    {
      fightMagnet_t m;
      m.knockdownWeight = jaffarCommon::json::getNumber<float>(actionJs, "Knockdown Weight");
      m.healthWeight    = jaffarCommon::json::getNumber<float>(actionJs, "Health Weight");
      m.range           = jaffarCommon::json::getNumber<float>(actionJs, "Range");
      m.rangeWeight     = jaffarCommon::json::getNumber<float>(actionJs, "Range Weight");
      m.countRange      = jaffarCommon::json::getNumber<float>(actionJs, "Count Range");
      m.staminaWeight   = jaffarCommon::json::getNumber<float>(actionJs, "Stamina Weight");
      if (actionJs.contains("Endurance Weight")) m.enduranceWeight = jaffarCommon::json::getNumber<float>(actionJs, "Endurance Weight");
      if (actionJs.contains("Damage Weight")) m.damageWeight = jaffarCommon::json::getNumber<float>(actionJs, "Damage Weight");
      if (actionJs.contains("Wasted Weight")) m.wastedWeight = jaffarCommon::json::getNumber<float>(actionJs, "Wasted Weight");
      rule.addAction([=, this]() { this->_fightMagnet = m; });
      recognizedActionType = true;
    }
    if (actionType == "Set Survival Magnet")
    {
      survivalMagnet_t m;
      m.distanceWeight = jaffarCommon::json::getNumber<float>(actionJs, "Distance Weight");
      m.distanceCap    = jaffarCommon::json::getNumber<float>(actionJs, "Distance Cap");
      m.centerWeight   = jaffarCommon::json::getNumber<float>(actionJs, "Center Weight");
      m.landedWeight   = jaffarCommon::json::getNumber<float>(actionJs, "Landed Weight");
      m.landedCap      = jaffarCommon::json::getNumber<float>(actionJs, "Landed Cap");
      if (actionJs.contains("Lean Weight")) m.leanWeight = jaffarCommon::json::getNumber<float>(actionJs, "Lean Weight");
      if (actionJs.contains("Lean Range")) m.leanRange = jaffarCommon::json::getNumber<float>(actionJs, "Lean Range");
      if (actionJs.contains("Safe Weight")) m.safeWeight = jaffarCommon::json::getNumber<float>(actionJs, "Safe Weight");
      if (actionJs.contains("Safe Distance")) m.safeDistance = jaffarCommon::json::getNumber<float>(actionJs, "Safe Distance");
      rule.addAction([=, this]() { this->_survivalMagnet = m; });
      recognizedActionType = true;
    }
    if (actionType == "Set Distance Magnet")
    {
      const auto intensity = jaffarCommon::json::getNumber<float>(actionJs, "Intensity");
      const auto center    = jaffarCommon::json::getNumber<float>(actionJs, "Center");
      rule.addAction([=, this]() { this->_distanceMagnet = {intensity, center}; });
      recognizedActionType = true;
    }
    return recognizedActionType;
  }

  uint8_t*              _ds = nullptr;
  uint8_t*              _memory = nullptr;
  bool                  _hashStack = true;
  InputSet::inputIndex_t _nullInputIdx;
  const int16_t*        _playerHealth;
  const int16_t*        _opponentHealth;
  const uint16_t*       _playerThrown;
  const uint16_t*       _playerLanded;

  // (state)
  int32_t _opponentDamage     = 0;
  int32_t _playerDamage       = 0;
  int16_t _lastOpponentHealth = 0;
  int16_t _lastPlayerHealth   = 0;
  uint8_t _started            = 0;
  // (derived after each step)
  int32_t  _wastedPunches = 0;
  uint32_t _stepFrames    = 0;

  // (magnets)
  float _opponentDamageMagnet = 0.0f;
  float _opponentHealthMagnet = 0.0f;
  float _playerStaminaMagnet  = 0.0f;
  struct distanceMagnet_t
  {
    float intensity;
    float center;
  } _distanceMagnet = {0.0f, 0.0f};
  struct fightMagnet_t
  {
    float knockdownWeight = 0.0f;
    float healthWeight    = 0.0f;
    float range           = 0.0f;
    float rangeWeight     = 0.0f;
    float countRange      = 0.0f;
    float staminaWeight   = 0.0f;
    float enduranceWeight = 0.0f;
    float damageWeight    = 0.0f;
    float wastedWeight    = 0.0f;
  } _fightMagnet;
  struct survivalMagnet_t
  {
    float distanceWeight = 0.0f;
    float distanceCap    = 0.0f;
    float centerWeight   = 0.0f;
    float landedWeight   = 0.0f;
    float landedCap      = 0.0f;
    float leanWeight     = 0.0f;
    float leanRange      = 0.0f;
    float safeWeight     = 0.0f;
    float safeDistance   = 0.0f;
  } _survivalMagnet;
};

} // namespace dos

} // namespace games

} // namespace jaffarPlus
