#pragma once

#include <emulator.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <memory>
#include <nesInstance.hpp>

// Relevant defines for rendering
#define BLIT_SIZE 65536
#define DEFAULT_WIDTH 256
#define DEFAULT_HEIGHT 240

namespace jaffarPlus
{

namespace emulator
{

class QuickerNES final : public Emulator
{
public:
#ifdef __JAFFAR_USE_QUICKERNES_ARKANOID
  static std::string getName() { return "QuickerNESArkanoid"; }
#else
  static std::string getName() { return "QuickerNES"; }
#endif

  // Constructor must only do configuration parsing
  QuickerNES(const nlohmann::json& config) : Emulator(config)
  {
    // Specifies how to map code ROM
    // Flat: The game program memory is placed raw in a contiguous array. This is faster for instruction decoding but every page switch triggers a full copy
    // Paged: The game program memory is properly distributed in pages whose address is determined by pointers. The indirection adds cost to instruction decoding but does not
    // require a copy on switch Use flat for games or sections that do not perform frequent bank switching; paged, otherwise
    _useFlatCodeMap = jaffarCommon::json::popBoolean(_emulatorConfigRemaining, "Use Flat Code Map");

    // Getting initial state file from the configuration
    _initialStateFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial State File Path");

    // For testing purposes, the initial state file can be overriden by environment variables
    if (auto* value = std::getenv("JAFFAR_QUICKERNES_OVERRIDE_INITIAL_STATE_FILE_PATH")) _initialStateFilePath = std::string(value);

    // Parsing rom file path
    _romFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Rom File Path");

    // For testing purposes, the rom file path can be overriden by environment variables
    if (auto* value = std::getenv("JAFFAR_QUICKERNES_OVERRIDE_ROM_FILE_PATH")) _romFilePath = std::string(value);

    // Getting initial sequence file path
    _initialSequenceFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial Sequence File Path");

    // For testing purposes, the sequence file path can be overriden by environment variables
    if (auto* value = std::getenv("JAFFAR_QUICKERNES_OVERRIDE_INITIAL_SEQUENCE_FILE_PATH")) _initialSequenceFilePath = std::string(value);

    // Parsing rom file SHA1
    _romFileSHA1 = jaffarCommon::json::popString(_emulatorConfigRemaining, "Rom File SHA1");

    // For testing purposes, the rom file SHA1 can be overriden by environment variables
    if (auto* value = std::getenv("JAFFAR_QUICKERNES_OVERRIDE_ROM_FILE_SHA1")) _romFileSHA1 = std::string(value);

    // Parsing initial RAM Data file
    _initialRAMDataFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial RAM Data File Path");

    // Getting Nametable size to use
    _NTABBlockSize = jaffarCommon::json::popNumber<size_t>(_emulatorConfigRemaining, "Nametable Block Size");
    _SRAMBlockSize = jaffarCommon::json::popNumber<size_t>(_emulatorConfigRemaining, "SRAM Block Size");

    // Optional (default off): serialize the full CPU/PPU/APU timing state so a deserialized replay
    // matches a live run even in glitch/derailed-execution regions. Only glitch-hunting games (e.g. PoP)
    // need it; leaving it off keeps the lighter, ~850 B-smaller original serialization.
    if (_emulatorConfigRemaining.contains("Precise State Timing")) _preciseTiming = jaffarCommon::json::popBoolean(_emulatorConfigRemaining, "Precise State Timing");

    // Getting disabled state properties
    const auto disabledStateProperties = jaffarCommon::json::popArray<std::string>(_emulatorConfigRemaining, "Disabled State Properties");
    for (const auto& property : disabledStateProperties) _disabledStateProperties.push_back(property);

    // Keys consumed by the underlying NES instance's input parser (jaffar::InputParser): popped here
    // too so the strict-key check below accounts for them. The instance is constructed from the
    // original config; we pop from the remaining copy only for accounting.
    jaffarCommon::json::popString(_emulatorConfigRemaining, "Controller 1 Type");
    jaffarCommon::json::popString(_emulatorConfigRemaining, "Controller 2 Type");

    // Creating internal emulator instance
    _quickerNES = std::make_unique<NESInstance>(config);

    // All recognized emulator-configuration keys have now been consumed; reject any leftover (unrecognized) key.
  };

  // Function to get a reference to the input parser from the base emulator
  jaffar::InputParser* getInputParser() const override { return _quickerNES->getInputParser(); }

  // Raw pointer to the internal quickerNES::Emu, for auxiliary tools that need core-level hooks
  // (e.g. the per-instruction trace callback used to audit glitch inputs at the CPU level).
  void* getInternalEmulatorPointer() const { return _quickerNES->getInternalEmulatorPointer(); }

  void initializeImpl() override
  {
    // Setting game's internal video buffer
    ((emulator_t*)_quickerNES->getInternalEmulatorPointer())->set_pixels(_videoBuffer, DEFAULT_WIDTH + 8);

    // Reading from ROM file
    std::string romFileData;
    bool        status = jaffarCommon::file::loadStringFromFile(romFileData, _romFilePath.c_str());
    if (status == false) JAFFAR_THROW_LOGIC("Could not find/read from ROM file: %s\n", _romFilePath.c_str());

    // Getting SHA1 of ROM for checksum
    auto actualRomSHA1 = jaffarCommon::hash::getSHA1String(romFileData);
    if (_romFileSHA1 != actualRomSHA1)
      JAFFAR_THROW_LOGIC("ROM file: '%s' expected SHA1 ('%s') does not concide with the one read ('%s')\n", _romFilePath.c_str(), _romFileSHA1.c_str(), actualRomSHA1.c_str());

    // Loading rom into emulator
    _quickerNES->loadROM((uint8_t*)romFileData.data(), romFileData.size());

    // Set the NTAB/SRAM serialize block sizes BEFORE any Initial State deserialization, so a state saved
    // with these (config) block sizes is read back with the same layout. (Previously these were set only
    // after the Initial State load, which deserialized with default sizes and corrupted/overflowed.)
    _quickerNES->setNTABBlockSize(_NTABBlockSize);
    _quickerNES->setSRAMBlockSize(_SRAMBlockSize);

    // Set precise-timing BEFORE any Initial State deserialization so the load uses the matching format.
    _quickerNES->setPreciseTiming(_preciseTiming);

    // If initial state file defined, load it
    if (_initialStateFilePath.empty() == false)
    {
      // Reading from initial state file
      std::string initialState;
      bool        success = jaffarCommon::file::loadStringFromFile(initialState, _initialStateFilePath);
      if (success == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from initial state file: %s\n", _initialStateFilePath.c_str());

      // Deserializing initial state into the emulator
      enableStateProperties();
      jaffarCommon::deserializer::Contiguous d(initialState.data(), initialState.size());
      deserializeState(d);
    }

    // Now disabling state properties, as requested
    disableStateProperties();

    // Getting input parser from the internal emulator
    const auto inputParser = _quickerNES->getInputParser();

    // Advancing the state using the initial sequence, if provided
    if (_initialSequenceFilePath != "")
    {
      // Load initial sequence
      std::string initialSequenceFileString;
      if (jaffarCommon::file::loadStringFromFile(initialSequenceFileString, _initialSequenceFilePath) == false)
        JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from initial sequence file: %s\n", _initialSequenceFilePath.c_str());

      // Getting input sequence
      const auto initialSequence = jaffarCommon::string::split(initialSequenceFileString, '\0');

      // Running inputs in the initial sequence
      for (const auto& inputString : initialSequence) advanceStateImpl(inputParser->parseInputString(inputString));
    }

    // Pushing initial RAM data
    if (_initialRAMDataFilePath != "")
    {
      // Load initial RAM Data
      std::string initialRAMDataString;
      if (jaffarCommon::file::loadStringFromFile(initialRAMDataString, _initialRAMDataFilePath) == false)
        JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from RAM Data file: %s\n", _initialRAMDataFilePath.c_str());

      // Pushing data into RAM
      memcpy(_quickerNES->getLowMem(), initialRAMDataString.data(), 0x800);
    }

    // Flattening code map, if required
    if (_useFlatCodeMap == true) _quickerNES->useFlatCodeMap();
  }

  // State advancing function
  void advanceStateImpl(const jaffar::input_t& input) override { _quickerNES->advanceState(input); }

  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const override { _quickerNES->serializeState(serializer); };
  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer) override { _quickerNES->deserializeState(deserializer); };

  __INLINE__ void printInfo() const override
  {
    jaffarCommon::logger::log("[J+]  + Initial Sequence File Path:          '%s'\n", _initialSequenceFilePath.c_str());
    jaffarCommon::logger::log("[J+]  + Uses Flat Code Map:                  %s\n", _useFlatCodeMap ? "True" : "False");
  }

  property_t getProperty(const std::string& propertyName) const override
  {
    if (propertyName == "LRAM") return property_t(_quickerNES->getLowMem(), _quickerNES->getLowMemSize());
    if (propertyName == "SRAM") return property_t(_quickerNES->getWorkMem(), _quickerNES->getWorkMemSize());
    if (propertyName == "NTAB") return property_t(_quickerNES->getNametableMem(), _quickerNES->getNametableMemSize());
    if (propertyName == "CHRR") return property_t(_quickerNES->getCHRMem(), _quickerNES->getCHRMemSize());
    if (propertyName == "SPRT") return property_t(_quickerNES->getSpriteMem(), _quickerNES->getSpriteMemSize());
    if (propertyName == "PALR") return property_t(_quickerNES->getPALMem(), _quickerNES->getPALMemSize());
    // CPU sticky halt latch (1 after a KIL/JAM executed; only console reset clears it). Games should
    // register it and FAIL any state that jammed the CPU -- real hardware freezes there, and letting
    // the emulator's NMI revive the game would make such states emulator artifacts, not valid play.
    if (propertyName == "CPU Halt Latch") return property_t(_quickerNES->getHaltLatchPtr(), 1);
    // Per-frame joypad ($4016/$4017) read count as a raw int: 0 right after an advance means that
    // frame polled no input (a lag frame). Not serialized -- only meaningful after a LIVE advance,
    // never after a state restore. Used to pinpoint the exact polled frame before each level's
    // loading lag group (the true stage-start frame).
    if (propertyName == "Joypad Read Count")
    {
      auto* p = _quickerNES->getJoypadReadCountPtr();
      if (p == nullptr) JAFFAR_THROW_LOGIC("Property 'Joypad Read Count' requires a -D_QUICKERNES_DETECT_JOYPAD_READS build.");
      return property_t((uint8_t*)p, sizeof(int));
    }
    // Glitch-investigation detector (only present in -D_QUICKERNES_DETECT_BAD_ACCESS builds): per-frame
    // flag, 1 iff this frame executed an unofficial opcode or fetched code from RAM (data-as-code derail).
    if (propertyName == "CPU Bad Access")
    {
      auto* p = _quickerNES->getBadAccessPtr();
      if (p == nullptr) JAFFAR_THROW_LOGIC("Property 'CPU Bad Access' requires a -D_QUICKERNES_DETECT_BAD_ACCESS build.");
      return property_t(p, 1);
    }
    // Per-frame count of instructions executed from work RAM (data-as-code "walk" detector). Normal
    // play touches RAM code only a few times per frame (jump-notes); a large value flags a derail such
    // as the NES Pinball score-warp. uint16, always available.
    if (propertyName == "CPU RAM Exec Count")
    {
      auto* p = _quickerNES->getRamExecCountPtr();
      if (p == nullptr) JAFFAR_THROW_LOGIC("Property 'CPU RAM Exec Count' requires a -DquickerNESStudyTracers=true build.");
      return property_t((uint8_t*)p, 2);
    }
    // Packed frame-timing accumulators (burst_phase, frame_length_extra) that drive NMI alignment. Add
    // this to a config's "Hash Properties" to distinguish timing-different states (glitch/walk hunting).
    if (propertyName == "CPU Timing State")
    {
      auto* p = _quickerNES->getTimingHashPtr();
      if (p == nullptr) JAFFAR_THROW_LOGIC("Property 'CPU Timing State' requires a -DquickerNESStudyTracers=true build.");
      return property_t(p, 8);
    }

    JAFFAR_THROW_LOGIC("Property name: '%s' not found in emulator '%s'", propertyName.c_str(), getName().c_str());
  }

  __INLINE__ void enableStateProperty(const std::string& property) { _quickerNES->enableStateBlock(property); }

  // Full-fidelity snapshot support (player reload): momentarily lift the configured block
  // exclusions so NTAB/CHRR/etc. serialize too, then re-apply them.
  void enableAllStateProperties() override { enableStateProperties(); }
  void reapplyDisabledStateProperties() override { disableStateProperties(); }

  __INLINE__ void disableStateProperty(const std::string& property) { _quickerNES->disableStateBlock(property); }

  ////////// Rendering functions (some of these taken from https://github.com/Bindernews/HeadlessQuickNes / MIT License)

  // Function to initalize the video palette
  int32_t* _initF_VideoPalette()
  {
    static int32_t           VideoPalette[512];
    const emulator_t::rgb_t* palette = emulator_t::nes_colors;
    for (int i = 0; i < 512; i++) { VideoPalette[i] = palette[i].red << 16 | palette[i].green << 8 | palette[i].blue | 0xff000000; }
    return VideoPalette;
  }

  // Initialize the video palette
  const int32_t* NES_VIDEO_PALETTE = _initF_VideoPalette();

  // Storage for nes renderer state
  int32_t _curBlit[BLIT_SIZE];
  uint8_t _videoBuffer[BLIT_SIZE];

  // Copied from bizinterface.cpp in BizHawk/quicknes
  __INLINE__ void saveBlit(const void* ePtr, int32_t* dest, const int32_t* colors, int cropleft, int croptop, int cropright, int cropbottom)
  {
    const emulator_t*    e         = (emulator_t*)ePtr;
    const unsigned char* in_pixels = e->frame().pixels;
    if (in_pixels == NULL) return;
    int32_t* out_pixels = dest;

    for (unsigned h = 0; h < emulator_t::image_height; h++, in_pixels += e->frame().pitch, out_pixels += emulator_t::image_width)
      for (unsigned w = 0; w < emulator_t::image_width; w++)
      {
        unsigned                 col = e->frame().palette[in_pixels[w]];
        const emulator_t::rgb_t& rgb = e->nes_colors[col];
        unsigned                 r   = rgb.red;
        unsigned                 g   = rgb.green;
        unsigned                 b   = rgb.blue;
        out_pixels[w]                = (r << 16) | (g << 8) | (b << 0);
      }
  }

  // This function opens the video output (e.g., window). In a headless/display-less environment the
  // window/renderer can't be created; we degrade gracefully to a "screenshot-only" mode (m_renderer
  // stays null, showRender becomes a no-op) so that --screenshotDir still works (it only needs
  // _curBlit, populated by updateRendererState -- no SDL).
  void initializeVideoOutput() override
  {
    m_window   = nullptr;
    m_renderer = nullptr;
    m_tex      = nullptr;

    SDL_SetMainReady();
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
      {
        jaffarCommon::logger::log("[J+] WARNING: no SDL video (%s); running headless (screenshots only).\n", SDL_GetError());
        return;
      }

    m_window = SDL_CreateWindow("JaffarPlus", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (m_window == nullptr)
    {
      jaffarCommon::logger::log("[J+] WARNING: no SDL window; headless (screenshots only).\n");
      return;
    }

    // Prefer accelerated, fall back to software, then to headless (screenshot-only).
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (m_renderer == nullptr) m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (m_renderer == nullptr)
    {
      jaffarCommon::logger::log("[J+] WARNING: no SDL renderer; headless (screenshots only).\n");
      SDL_DestroyWindow(m_window);
      m_window = nullptr;
      return;
    }
    if (!(m_tex = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 256)))
      JAFFAR_THROW_RUNTIME("Coult not create SDL texture in NES emulator");
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    if (!setScale(1)) JAFFAR_THROW_RUNTIME("Coult not set SDL scale in NES emulator");
  }

  // This function closes the video output (e.g., window)
  void finalizeVideoOutput() override
  {
    if (m_tex) SDL_DestroyTexture(m_tex);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
  }

  __INLINE__ void enableRendering() override { _quickerNES->enableRendering(); }

  // Headless (no-SDL) screenshots: just enable core video rendering; saveScreenshot refreshes the
  // blit itself, so no SDL window/renderer/texture is needed.
  __INLINE__ void enableHeadlessRendering() override { _quickerNES->enableRendering(); }

  __INLINE__ void disableRendering() override { _quickerNES->disableRendering(); }

  __INLINE__ void updateRendererState(const size_t stepIdx, const std::string input) override
  {
    saveBlit(_quickerNES->getInternalEmulatorPointer(), _curBlit, NES_VIDEO_PALETTE, 0, 0, 0, 0);
  }

  // Write the current rendered frame (_curBlit, populated by updateRendererState) to a 24-bit BMP.
  // No SDL/display needed -- _curBlit holds 0x00RRGGBB pixels at image_width x image_height.
  void saveScreenshot(const std::string& path) override
  {
    // Refresh the blit from the core's last rendered frame (headless path never calls updateRendererState)
    saveBlit(_quickerNES->getInternalEmulatorPointer(), _curBlit, NES_VIDEO_PALETTE, 0, 0, 0, 0);
    const int W            = (int)emulator_t::image_width;
    const int H            = (int)emulator_t::image_height;
    const int rowSize      = ((W * 3 + 3) / 4) * 4;
    const int dataSize     = rowSize * H;
    const int fileSize     = 54 + dataSize;
    uint8_t   hdr[54]      = {0};
    hdr[0]                 = 'B';
    hdr[1]                 = 'M';
    *(uint32_t*)(hdr + 2)  = (uint32_t)fileSize;
    *(uint32_t*)(hdr + 10) = 54;
    *(uint32_t*)(hdr + 14) = 40;
    *(int32_t*)(hdr + 18)  = W;
    *(int32_t*)(hdr + 22)  = H; // positive height => bottom-up
    *(uint16_t*)(hdr + 26) = 1;
    *(uint16_t*)(hdr + 28) = 24;
    *(uint32_t*)(hdr + 34) = (uint32_t)dataSize;
    FILE* f                = fopen(path.c_str(), "wb");
    if (f == nullptr) return;
    fwrite(hdr, 1, 54, f);
    std::vector<uint8_t> row(rowSize, 0);
    for (int y = H - 1; y >= 0; y--) // BMP rows are bottom-up
    {
      for (int x = 0; x < W; x++)
      {
        uint32_t px    = (uint32_t)_curBlit[y * W + x];
        row[x * 3 + 0] = (uint8_t)(px & 0xFF);         // B
        row[x * 3 + 1] = (uint8_t)((px >> 8) & 0xFF);  // G
        row[x * 3 + 2] = (uint8_t)((px >> 16) & 0xFF); // R
      }
      fwrite(row.data(), 1, (size_t)rowSize, f);
    }
    fclose(f);
  }
  __INLINE__ void   serializeRendererState(jaffarCommon::serializer::Base& serializer) const override { serializer.pushContiguous(_curBlit, sizeof(int32_t) * BLIT_SIZE); }
  __INLINE__ void   deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) override { deserializer.popContiguous(_curBlit, sizeof(int32_t) * BLIT_SIZE); }
  __INLINE__ size_t getRendererStateSize() const override
  {
    jaffarCommon::serializer::Contiguous s;
    serializeRendererState(s);
    return s.getOutputSize();
  }

  // Window pointer
  SDL_Window* m_window;

  // Renderer
  SDL_Renderer* m_renderer;

  // SDL Textures
  SDL_Texture* m_tex;

  // Destination rect for the texture
  SDL_Rect m_nesDest;

  const SDL_Rect NES_BLIT_RECT = {0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT};

  bool setScale(int scale)
  {
    int winW = DEFAULT_WIDTH * scale;
    int winH = DEFAULT_HEIGHT * scale;

    // Change the window size
    SDL_SetWindowSize(m_window, winW, winH);

    // update the overlay destination
    m_nesDest = {0, 0, winW, winH};

    return true;
  }

  __INLINE__ void showRender() override
  {
    if (m_renderer == nullptr) return; // headless (screenshot-only) mode

    void* nesPixels = nullptr;
    int   pitch     = 0;

    if (SDL_LockTexture(m_tex, nullptr, &nesPixels, &pitch) < 0) JAFFAR_THROW_RUNTIME("Coult not lock texture in NES emulator");

    memcpy(nesPixels, _curBlit, sizeof(int32_t) * BLIT_SIZE);
    SDL_UnlockTexture(m_tex);

    // render to screen
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_tex, &NES_BLIT_RECT, &m_nesDest);

    SDL_RenderPresent(m_renderer);
  }

private:
  __INLINE__ void disableStateProperties()
  {
    for (const auto& property : _disabledStateProperties) disableStateProperty(property);
  }
  __INLINE__ void enableStateProperties()
  {
    for (const auto& property : _disabledStateProperties) enableStateProperty(property);
  }

  void printMemoryBlockHash(const std::string& blockName) const
  {
    auto p    = getProperty(blockName);
    auto hash = jaffarCommon::hash::hashToString(jaffarCommon::hash::calculateMetroHash(p.pointer, p.size));
    jaffarCommon::logger::log("[J+] %s Hash:        %s\n", blockName.c_str(), hash.c_str());
  }

  std::unique_ptr<NESInstance> _quickerNES;

  bool        _useFlatCodeMap;
  size_t      _NTABBlockSize;
  size_t      _SRAMBlockSize;
  bool        _preciseTiming = false;
  std::string _romFilePath;
  std::string _romFileSHA1;

  std::string _initialStateFilePath;
  std::string _initialSequenceFilePath;
  std::string _initialRAMDataFilePath;

  // Collection of state blocks to disable during engine run
  std::vector<std::string> _disabledStateProperties;
};

} // namespace emulator

} // namespace jaffarPlus