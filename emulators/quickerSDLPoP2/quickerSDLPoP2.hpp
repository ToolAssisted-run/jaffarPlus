#pragma once

#include <emulator.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <memory>
#include <quickerSDLPoP2/quickerInstance.hpp>
#include <sstream>

namespace jaffarPlus
{

namespace emulator
{

/**
 * Prince of Persia 2: The Shadow and the Flame (DOS 1.0) through quickerSDLPoP2, the game logic of SDLPoP2 as one
 * thread-safe class. No drawing: the search needs none.
 *
 * Emulator Configuration:
 *   "Game Path"                   the folder with the game's files (PRINCE.EXE, *.DAT)
 *   "Start Level", "Seed"          a new game at that level (1..14) with that random seed
 *   "Initial State File"           (optional, "" none) a savestate to start from instead (quickerSDLPoP2 / SDLPoP2 format)
 *   "Initial Sequence File Path"   (optional) inputs played after that, one per line
 *   "Override RNG Enabled", "Override RNG Value"   (optional) the random seed set after all that
 */
class QuickerSDLPoP2 final : public Emulator
{
public:
  static std::string getName() { return "QuickerSDLPoP2"; }

  // Constructor must only do configuration parsing
  QuickerSDLPoP2(const nlohmann::json& config) : Emulator(config)
  {
    // The instance reads "Game Path" from the configuration itself
    _instance = std::make_unique<PoP2Instance>(config);
    jaffarCommon::json::popString(_emulatorConfigRemaining, "Game Path");

    _startLevel = jaffarCommon::json::popNumber<int>(_emulatorConfigRemaining, "Start Level");
    _seed       = jaffarCommon::json::popNumber<uint32_t>(_emulatorConfigRemaining, "Seed");

    _stateFilePath = _emulatorConfigRemaining.contains("Initial State File") ? jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial State File") : std::string("");
    _initialSequenceFilePath =
        _emulatorConfigRemaining.contains("Initial Sequence File Path") ? jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial Sequence File Path") : std::string("");

    _overrideRNGEnabled = _emulatorConfigRemaining.contains("Override RNG Enabled") ? jaffarCommon::json::popBoolean(_emulatorConfigRemaining, "Override RNG Enabled") : false;
    _overrideRNGValue   = _emulatorConfigRemaining.contains("Override RNG Value") ? jaffarCommon::json::popNumber<uint32_t>(_emulatorConfigRemaining, "Override RNG Value") : 0;
  };

  void initializeImpl() override
  {
    _instance->initialize();
    _instance->newGame(_startLevel, _seed);

    // A starting savestate, if given
    if (_stateFilePath != "")
    {
      std::string stateFileData;
      if (jaffarCommon::file::loadStringFromFile(stateFileData, _stateFilePath) == false) JAFFAR_THROW_LOGIC("Could not read the initial state file: %s\n", _stateFilePath.c_str());
      if (stateFileData.size() != _instance->getStateSize())
        JAFFAR_THROW_LOGIC("The initial state file %s has %lu bytes, not %lu\n", _stateFilePath.c_str(), stateFileData.size(), _instance->getStateSize());
      jaffarCommon::deserializer::Contiguous deserializer(stateFileData.data(), stateFileData.size());
      _instance->deserializeState(deserializer);
    }

    // A fixed input sequence, if given
    if (_initialSequenceFilePath != "")
    {
      std::string seqData;
      if (jaffarCommon::file::loadStringFromFile(seqData, _initialSequenceFilePath) == false)
        JAFFAR_THROW_LOGIC("Could not load initial sequence file: %s\n", _initialSequenceFilePath.c_str());
      std::istringstream iss(seqData);
      std::string        line;
      while (std::getline(iss, line))
      {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        _instance->advanceState(getInputParser()->parseInputString(line));
      }
    }

    if (_overrideRNGEnabled) _instance->setRNGValue(_overrideRNGValue);
  }

  jaffar::InputParser* getInputParser() const override { return _instance->getInputParser(); }

  void advanceStateImpl(const jaffar::input_t& input) override { _instance->advanceState(input); }

  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const override { _instance->serializeState(serializer); };

  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer) override { _instance->deserializeState(deserializer); };

  __INLINE__ void printInfo() const override {}

  // "Game State": the savestate part of the core (quicker::QuickerSDLPoP2State), for the game's properties
  __INLINE__ property_t getProperty(const std::string& propertyName) const override
  {
    if (propertyName == "Game State") return property_t((uint8_t*)static_cast<quicker::QuickerSDLPoP2State*>(_instance->getCore()), _instance->getStateSize());
    JAFFAR_THROW_LOGIC("Property name: '%s' not found in emulator '%s'", propertyName.c_str(), getName().c_str());
  }

  // No drawing (the search needs none)
  void initializeVideoOutput() override {}
  void finalizeVideoOutput() override {}
  __INLINE__ void enableRendering() override {}
  __INLINE__ void disableRendering() override {}
  __INLINE__ void updateRendererState(const size_t stepIdx, const std::string input) override {}
  __INLINE__ void serializeRendererState(jaffarCommon::serializer::Base& serializer) const override { serializeState(serializer); }
  __INLINE__ void deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) override { deserializeState(deserializer); }
  __INLINE__ size_t getRendererStateSize() const override { return getStateSize(); }
  __INLINE__ void showRender() override {}

private:
  std::unique_ptr<PoP2Instance> _instance;

  int         _startLevel;
  uint32_t    _seed;
  std::string _stateFilePath;
  std::string _initialSequenceFilePath;
  bool        _overrideRNGEnabled;
  uint32_t    _overrideRNGValue;
};

} // namespace emulator

} // namespace jaffarPlus
