#pragma once

#include <SDL.h>
#include <emulator.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <machine.h>
#include <sstream>
#include <vector>

namespace jaffarPlus
{

namespace emulator
{

/**
 * 4D Sports Boxing (DOS, 1991) through quickerOpen4DBoxing: Open4DBoxing's game (its MCGA build) as a machine that is
 * stepped, saved and loaded, one machine a thread. The inputs are Chimera's core's movie rows (quickerOpen4DBoxing's
 * inputParser.hpp), so a solution is Chimera input.
 *
 * Emulator Configuration:
 *   "Game Path"                  the folder with the two floppies' images (disk1.img, disk2.img)
 *   "Sound"                      (optional, "mt32") the sound card the game drives (its driver's timing is part of the
 *                                game's): "mt32", "sb", "adlib", "speaker", "tandy", as the Chimera project's setting
 *   "Clock Start"                (optional, "1991-07-30 14:47:00") the PC's clock at the start, the project's setting
 *   "CPU Speed"                  (optional, 22000) the PC's speed, DOSBox's cycles (instructions a millisecond), the
 *                                project's setting: at 220000 every frame of the game fits in a tick of its timer and
 *                                its fight goes the same drawn or not ("Fast Fight")
 *   "Fast Fight"                 (optional, false) from the initial sequence's end, the fight's logic alone (no scene
 *                                drawn, the frames with nothing to do skipped; quickerOpen4DBoxing's o4db_set_fast):
 *                                the game's steps at a CPU speed of 220000, many times faster
 *   "Initial State File Path"    (optional) a machine's state (quickerOpen4DBoxing's o4db_save, tests/o4dbBench's save:
 *                                the whole game's after a movie's rows, at the CPU speed) loaded instead of playing the
 *                                initial sequence (the rows then only counted): a fight's later rounds without their
 *                                movie played again in every thread
 *   "Initial Speed"              (optional, experiments) the CPU speed of the initial sequence, the machine switched to
 *                                "CPU Speed" at its end (a sequence recorded at another speed; a search from it is no
 *                                movie)
 *   "Initial Sequence File Path" (optional) a Chimera movie's rows played from the start, a row a frame (its first row
 *                                before the game starts, as Chimera's first step); the search starts where it ends
 *   "Step Function"              (optional, "25BB0202") where the game reads the boxers' controls (its timer interrupt's
 *                                routine): a step ends at the first stop after the game enters it, once the step has
 *                                passed a frame end; "" steps are frames
 *   "Max Frames Per Step"        (optional, 8) a step ends after so many frames if the game reads no controls (between
 *                                rounds, the panels, the menus)
 *   "Max Delta Pages"            (optional, 1024) the states are the memory's 256-byte pages that differ from the
 *                                search's start (the same in every thread) and the rest of the machine: at most so many
 *                                pages (a state with more stops the search)
 *   "Unsaved Memory"             (optional) [["START", "END"], ...]: memory the states do not hold (linear addresses,
 *                                hexadecimal, whole pages): what the game only writes before it reads it (its pictures:
 *                                the screen A0000-AFFFF, its drawing buffers); the pictures of a state loaded are wrong
 *   "Initial Pokes"              (optional, experiments) [[frame, "OFFSET", "VALUE"], ...]: a word of the data segment
 *                                changed at that frame end of the initial sequence (before its input), e.g. a boxer's
 *                                fight parameters for another set of statistics; a search with pokes is no movie
 */
class QuickerOpen4DBoxing final : public Emulator
{
public:
  static std::string getName() { return "QuickerOpen4DBoxing"; }

  // Constructor must only do configuration parsing
  QuickerOpen4DBoxing(const nlohmann::json& config) : Emulator(config)
  {
    _gamePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Game Path");
    if (_emulatorConfigRemaining.contains("Sound")) _sound = jaffarCommon::json::popString(_emulatorConfigRemaining, "Sound");
    if (_emulatorConfigRemaining.contains("Clock Start")) _clockStart = jaffarCommon::json::popString(_emulatorConfigRemaining, "Clock Start");
    if (_emulatorConfigRemaining.contains("CPU Speed")) _cpuSpeed = jaffarCommon::json::popNumber<uint32_t>(_emulatorConfigRemaining, "CPU Speed");
    _initialSpeed = _cpuSpeed;
    if (_emulatorConfigRemaining.contains("Initial Speed")) _initialSpeed = jaffarCommon::json::popNumber<uint32_t>(_emulatorConfigRemaining, "Initial Speed");
    if (_emulatorConfigRemaining.contains("Fast Fight")) _fastFight = jaffarCommon::json::popBoolean(_emulatorConfigRemaining, "Fast Fight");
    if (_emulatorConfigRemaining.contains("Initial Sequence File Path"))
      _initialSequenceFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial Sequence File Path");
    if (_emulatorConfigRemaining.contains("Initial State File Path"))
      _initialStateFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial State File Path");
    if (_emulatorConfigRemaining.contains("Step Function"))
    {
      const auto f = jaffarCommon::json::popString(_emulatorConfigRemaining, "Step Function");
      _stepFunction = f.empty() ? 0 : (uint32_t)std::stoul(f, nullptr, 16);
    }
    if (_emulatorConfigRemaining.contains("Max Frames Per Step")) _maxFramesPerStep = jaffarCommon::json::popNumber<unsigned>(_emulatorConfigRemaining, "Max Frames Per Step");
    if (_emulatorConfigRemaining.contains("Max Delta Pages")) _maxPages = jaffarCommon::json::popNumber<unsigned>(_emulatorConfigRemaining, "Max Delta Pages");
    if (_emulatorConfigRemaining.contains("Unsaved Memory"))
      for (const auto& r : jaffarCommon::json::popArray<nlohmann::json>(_emulatorConfigRemaining, "Unsaved Memory"))
        _unsaved.push_back({(uint32_t)std::stoul(r[0].get<std::string>(), nullptr, 16), (uint32_t)std::stoul(r[1].get<std::string>(), nullptr, 16)});
    if (_emulatorConfigRemaining.contains("Initial Pokes"))
      for (const auto& p : jaffarCommon::json::popArray<nlohmann::json>(_emulatorConfigRemaining, "Initial Pokes"))
        _pokes.push_back({p[0].get<uint64_t>(), (uint16_t)std::stoul(p[1].get<std::string>(), nullptr, 16), (uint16_t)std::stoul(p[2].get<std::string>(), nullptr, 16)});
    _inputParser = std::make_unique<jaffar::InputParser>(config);
  };

  void initializeImpl() override
  {
    o4db_set_speed(_initialSpeed);
    if (!o4db_setup(_gamePath.c_str(), _sound.c_str(), _clockStart.c_str(), nullptr)) JAFFAR_THROW_LOGIC("quickerOpen4DBoxing: %s\n", o4db_error());

    // The initial sequence: a Chimera movie's rows, a row a frame end (its first before the game starts)
    std::vector<O4dbInput> rows;
    if (_initialSequenceFilePath != "")
    {
      std::string data;
      if (jaffarCommon::file::loadStringFromFile(data, _initialSequenceFilePath) == false)
        JAFFAR_THROW_LOGIC("Could not load initial sequence file: %s\n", _initialSequenceFilePath.c_str());
      std::istringstream iss(data);
      std::string        line;
      while (std::getline(iss, line))
      {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] != '|') continue;
        const auto in = _inputParser->parseInputString(line);
        rows.push_back({in.buttons, in.mouseX, in.mouseY});
      }
    }
    const O4dbInput neutral = {0, O4DB_MOUSE_X_NEUTRAL, O4DB_MOUSE_Y_NEUTRAL};
    o4db_new(rows.empty() ? &neutral : &rows[0]);
    if (_initialStateFilePath != "")
    {
      // a saved machine instead of the rows played (the same machine: the whole game's, after them)
      std::string state;
      if (jaffarCommon::file::loadStringFromFile(state, _initialStateFilePath) == false)
        JAFFAR_THROW_LOGIC("Could not load initial state file: %s\n", _initialStateFilePath.c_str());
      if (state.size() != o4db_state_size())
        JAFFAR_THROW_LOGIC("quickerOpen4DBoxing: %s holds %lu bytes, a state is %lu\n", _initialStateFilePath.c_str(), state.size(), o4db_state_size());
      o4db_load(state.data());
      _initialFrames = o4db_frames();
    }
    else
    {
      if (!_pokes.empty()) o4db_set_frame_hook(pokeHook, this);
      _initialFrames = rows.size() > 1 ? o4db_run(rows.data() + 1, rows.size() - 1) : 0;
      o4db_set_frame_hook(nullptr, nullptr);
    }
    _initialRows = rows.size();

    // From here on, steps (the fight's logic alone if asked, at the CPU speed asked): and the states as the pages that
    // differ from here (every thread's machine must be here)
    if (_initialSpeed != _cpuSpeed) o4db_test_set_cycles(_cpuSpeed);
    o4db_set_step_function(_stepFunction, _maxFramesPerStep);
    o4db_set_fast(_fastFight ? 1 : 0);
    for (const auto& r : _unsaved) o4db_set_unsaved(r.first, r.second);
    const int differ = o4db_set_reference();
    if (differ != 0) JAFFAR_THROW_LOGIC("quickerOpen4DBoxing: this thread's machine differs from the first one's after the initial sequence (%d pages)\n", differ);
    _deltaSize = o4db_delta_size(_maxPages);
    _stateBuffer.resize(_deltaSize);
  }

  jaffar::InputParser* getInputParser() const override { return _inputParser.get(); }

  void advanceStateImpl(const jaffar::input_t& input) override
  {
    const O4dbInput in = {input.buttons, input.mouseX, input.mouseY};
    _lastStepFrames = o4db_step(&in);
  }

  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const override
  {
    if (_isInitialized == false || _stateBuffer.empty())
    {
      // (the size only, before the machine runs: the configured pages)
      std::vector<uint8_t> dummy(o4db_delta_size(_maxPages));
      serializer.push(dummy.data(), dummy.size());
      return;
    }
    if (o4db_save_delta(_stateBuffer.data(), _maxPages) < 0)
      JAFFAR_THROW_LOGIC("quickerOpen4DBoxing: a state differs from the search's start in more than %u pages of memory: raise \"Max Delta Pages\"\n", _maxPages);
    serializer.push(_stateBuffer.data(), _stateBuffer.size());
  };

  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(_stateBuffer.data(), _stateBuffer.size());
    o4db_load_delta(_stateBuffer.data(), _maxPages);
  };

  __INLINE__ void printInfo() const override
  {
    jaffarCommon::logger::log("[J+] quickerOpen4DBoxing: %lu initial rows (%lu frames), %lu-byte states (%u pages at most), step function %08X, %u frames at most\n",
                              _initialRows, _initialFrames, _deltaSize, _maxPages, _stepFunction, _maxFramesPerStep);
    jaffarCommon::logger::log("[J+] quickerOpen4DBoxing: CPU speed %u (the initial sequence's %u), %s; %lu replays of steps (a missed punch's garbage)\n", _cpuSpeed, _initialSpeed,
                              _fastFight ? "the fight's logic alone" : "the whole game", (unsigned long)o4db_fast_replays());
  }

  // "Memory": the megabyte; "Data Segment": the program's (DS 3173h, 64 KB): the boxers, the fight, the career
  __INLINE__ property_t getProperty(const std::string& propertyName) const override
  {
    if (propertyName == "Memory") return property_t(o4db_memory(), 0x110000);
    if (propertyName == "Data Segment") return property_t(o4db_memory() + ((uint32_t)o4db_data_segment() << 4), 0x10000);
    JAFFAR_THROW_LOGIC("Property name: '%s' not found in emulator '%s'", propertyName.c_str(), getName().c_str());
  }

  // The frames the last step passed (a step is as many movie rows), the frames from the start
  uint64_t getLastStepFrames() const { return _lastStepFrames; }
  uint64_t getFrames() const { return o4db_frames(); }

  // The player's window: the screen as the state left it (320x200 through the DAC)
  void initializeVideoOutput() override
  {
    SDL_SetMainReady();
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) JAFFAR_THROW_LOGIC("Failed to initialize video: %s", SDL_GetError());
    _window = SDL_CreateWindow("JaffarPlus - 4D Sports Boxing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 3, 200 * 3, SDL_WINDOW_RESIZABLE);
    if (_window == nullptr) JAFFAR_THROW_LOGIC("Could not open SDL window: %s", SDL_GetError());
    _renderer = SDL_CreateRenderer(_window, -1, 0);
    if (_renderer == nullptr) JAFFAR_THROW_LOGIC("Could not create SDL renderer: %s", SDL_GetError());
    SDL_RenderSetLogicalSize(_renderer, 320, 240); // the 320x200 picture on a 4:3 screen
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
    if (_texture == nullptr) JAFFAR_THROW_LOGIC("Could not create SDL texture: %s", SDL_GetError());
  }

  void enableHeadlessRendering() override {}

  void saveScreenshot(const std::string& path) override
  {
    drawFrame();
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

  // The renderer's state is the step's state
  __INLINE__ void updateRendererState(const size_t stepIdx, const std::string input) override {}
  __INLINE__ void serializeRendererState(jaffarCommon::serializer::Base& serializer) const override { serializeState(serializer); }
  __INLINE__ void deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) override { deserializeState(deserializer); }
  __INLINE__ size_t getRendererStateSize() const override { return _deltaSize; }

  void showRender() override
  {
    drawFrame();
    SDL_UpdateTexture(_texture, nullptr, _pixels, 320 * sizeof(uint32_t));
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
    SDL_PumpEvents();
  }

private:
  struct poke_t
  {
    uint64_t frame;
    uint16_t offset, value;
  };
  static void pokeHook(uint64_t frame, void* ctx)
  {
    auto*    self = (QuickerOpen4DBoxing*)ctx;
    uint8_t* ds   = o4db_memory() + ((uint32_t)o4db_data_segment() << 4);
    for (const auto& p : self->_pokes)
      if (p.frame == frame)
      {
        ds[p.offset]     = (uint8_t)p.value;
        ds[p.offset + 1] = (uint8_t)(p.value >> 8);
      }
  }
  std::vector<poke_t> _pokes;
  std::vector<std::pair<uint32_t, uint32_t>> _unsaved;  // "Unsaved Memory"

  void drawFrame()
  {
    const uint8_t* screen = o4db_screen();
    const uint8_t(*dac)[3] = o4db_palette();
    for (size_t i = 0; i < 320 * 200; i++)
    {
      const uint8_t* c = dac[screen[i]];
      _pixels[i]       = 0xFF000000u | ((uint32_t)((c[0] & 63) << 2 | (c[0] & 63) >> 4) << 16) | ((uint32_t)((c[1] & 63) << 2 | (c[1] & 63) >> 4) << 8) |
                   (uint32_t)((c[2] & 63) << 2 | (c[2] & 63) >> 4);
    }
  }

  std::unique_ptr<jaffar::InputParser> _inputParser;
  std::string                          _gamePath;
  std::string                          _sound      = "mt32";
  std::string                          _clockStart = "1991-07-30 14:47:00";
  std::string                          _initialSequenceFilePath;
  std::string                          _initialStateFilePath;
  uint32_t                             _stepFunction     = 0x25BB0202;
  unsigned                             _maxFramesPerStep = 8;
  unsigned                             _maxPages         = 1024;
  uint32_t                             _cpuSpeed         = 22000;
  uint32_t                             _initialSpeed     = 22000;
  bool                                 _fastFight        = false;
  size_t                               _deltaSize        = 0;
  size_t                               _initialRows      = 0;
  uint64_t                             _initialFrames    = 0;
  uint64_t                             _lastStepFrames   = 0;
  mutable std::vector<uint8_t>         _stateBuffer;

  SDL_Window*   _window   = nullptr;
  SDL_Renderer* _renderer = nullptr;
  SDL_Texture*  _texture  = nullptr;
  uint32_t      _pixels[320 * 200];
};

} // namespace emulator

} // namespace jaffarPlus
