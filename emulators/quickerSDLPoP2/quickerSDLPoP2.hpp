#pragma once

#include <SDL.h>
#include <emulator.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <memory>
#include <quickerSDLPoP2/quickerInstance.hpp>
#include <sstream>
#include <vector>

// The drawing: SDLPoP2 itself (linked whole, one game in globals), which draws a savestate of its own format, the same
// bytes as quickerSDLPoP2's (source/shell.h, render.h)
extern "C"
{
  int            shell_init(const char* dir, int argc, const char** argv);
  void           shell_draw_state(const void* state);
  extern uint8_t screen_buf[320 * 200];
  extern uint8_t render_palette[256 * 3];
}

namespace jaffarPlus
{

namespace emulator
{

/**
 * Prince of Persia 2: The Shadow and the Flame (DOS 1.0) through quickerSDLPoP2, the game logic of SDLPoP2 as one
 * thread-safe class. The search draws nothing; the player draws each step with SDLPoP2's own renderer.
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
    _gamePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Game Path");

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

  // The player's window: each step's savestate drawn by SDLPoP2 (a room drawn afresh, as its quickload shows it)
  void initializeVideoOutput() override
  {
    SDL_SetMainReady();
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) JAFFAR_THROW_LOGIC("Failed to initialize video: %s", SDL_GetError());
    _window = SDL_CreateWindow("JaffarPlus - Prince of Persia 2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 3, 200 * 3, SDL_WINDOW_RESIZABLE);
    if (_window == nullptr) JAFFAR_THROW_LOGIC("Could not open SDL window: %s", SDL_GetError());
    _renderer = SDL_CreateRenderer(_window, -1, 0);
    if (_renderer == nullptr) JAFFAR_THROW_LOGIC("Could not create SDL renderer: %s", SDL_GetError());
    SDL_RenderSetLogicalSize(_renderer, 320, 240); // the DOS 320x200 picture on a 4:3 screen
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
    if (_texture == nullptr) JAFFAR_THROW_LOGIC("Could not create SDL texture: %s", SDL_GetError());
    initializeDrawing();
  }

  // Screenshots without a window (the player's --screenshotDir)
  void enableHeadlessRendering() override { initializeDrawing(); }

  void saveScreenshot(const std::string& path) override
  {
    std::vector<uint8_t>                 state(getStateSize());
    jaffarCommon::serializer::Contiguous serializer(state.data(), state.size());
    serializeState(serializer);
    drawFrame(state.data());
    auto surface = SDL_CreateRGBSurfaceWithFormatFrom(_pixels, 320, 200, 32, 320 * sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr || SDL_SaveBMP(surface, path.c_str()) != 0) JAFFAR_THROW_LOGIC("Could not save the screenshot %s: %s\n", path.c_str(), SDL_GetError());
    SDL_FreeSurface(surface);
  }

  void finalizeVideoOutput() override
  {
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_window);
  }

  __INLINE__ void enableRendering() override {}
  __INLINE__ void disableRendering() override {}

  // The renderer's state is the step's savestate
  __INLINE__ void updateRendererState(const size_t stepIdx, const std::string input) override {}
  __INLINE__ void serializeRendererState(jaffarCommon::serializer::Base& serializer) const override { serializeState(serializer); }
  __INLINE__ void deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) override
  {
    _renderState.resize(getStateSize());
    deserializer.pop(_renderState.data(), _renderState.size());
  }
  __INLINE__ size_t getRendererStateSize() const override { return getStateSize(); }

  void showRender() override
  {
    if (_renderState.empty()) return;
    drawFrame(_renderState.data());
    SDL_UpdateTexture(_texture, nullptr, _pixels, 320 * sizeof(uint32_t));
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
    SDL_PumpEvents();
  }

private:
  // SDLPoP2 set up once (the game's files read; its images are loaded per level as the states need them)
  void initializeDrawing()
  {
    if (_drawingInitialized) return;
    if (shell_init(_gamePath.c_str(), 0, nullptr) == 0) JAFFAR_THROW_LOGIC("SDLPoP2 could not read the game's files in: %s\n", _gamePath.c_str());
    _drawingInitialized = true;
  }

  // A savestate drawn by SDLPoP2 into _pixels (ARGB, the VGA's 6-bit colours scaled up)
  void drawFrame(const uint8_t* state)
  {
    shell_draw_state(state);
    uint32_t palette[256];
    for (size_t i = 0; i < 256; i++)
      palette[i] = 0xFF000000u | ((uint32_t)(render_palette[3 * i] << 2) << 16) | ((uint32_t)(render_palette[3 * i + 1] << 2) << 8) | (uint32_t)(render_palette[3 * i + 2] << 2);
    for (size_t i = 0; i < 320 * 200; i++) _pixels[i] = palette[screen_buf[i]];
  }

  std::unique_ptr<PoP2Instance> _instance;

  std::string          _gamePath;
  SDL_Window*          _window   = nullptr;
  SDL_Renderer*        _renderer = nullptr;
  SDL_Texture*         _texture  = nullptr;
  std::vector<uint8_t> _renderState;
  bool                 _drawingInitialized = false;
  uint32_t             _pixels[320 * 200];

  int         _startLevel;
  uint32_t    _seed;
  std::string _stateFilePath;
  std::string _initialSequenceFilePath;
  bool        _overrideRNGEnabled;
  uint32_t    _overrideRNGValue;
};

} // namespace emulator

} // namespace jaffarPlus
