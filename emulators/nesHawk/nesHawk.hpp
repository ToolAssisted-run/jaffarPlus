#pragma once

/**
 * @file nesHawk.hpp
 * @brief JaffarPlus emulator wrapper around the C++ translation of BizHawk's NesHawk core.
 *
 * NesHawk is the cycle-accurate ground-truth NES core (BizHawk, C#, MIT), translated 1:1 to C++
 * under extern/quickerNesHawk/source/ and validated byte-exact against the genuine C# implementation
 * (per-frame 2KB RAM over a 55,412-frame full-game movie plus both level-1 solutions; see
 * extern/quickerNesHawk/harness for the golden oracle). Use it when search results must hold on real
 * hardware timing: QuickerNES is instruction-granular and diverges from NesHawk on
 * interrupt-timing-sensitive glitches (e.g. the Prince of Persia U+D action glitch family).
 *
 * ~10-30x slower per frame than QuickerNES -- cycle accuracy is the price of ground truth.
 *
 * The translated core is a single system configuration matching the Prince of Persia (NES) cart:
 * UNROM (mapper 2), 128KB PRG, 8KB CHR-RAM, vertical mirroring, NTSC, one joypad. Other carts
 * need their board translated first (extern/quickerNesHawk/source/nesBoards.hpp).
 *
 * Config keys (subset of QuickerNES's, same meanings): "Rom File Path", "Rom File SHA1",
 * "Initial State File Path", "Initial Sequence File Path", "Initial RAM Data File Path",
 * "Controller 1 Type", "Controller 2 Type".
 */

#include "../../extern/quickerNesHawk/source/nes.hpp"
#include "../../extern/quickerNesHawk/source/nesSerialization.hpp"
#include "inputParser.hpp"
#include <emulator.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/string.hpp>
#include <memory>
#include <string>
#include <vector>

namespace jaffarPlus
{

namespace emulator
{

class NesHawk final : public Emulator
{
public:
  static std::string getName() { return "NesHawk"; }

  // Constructor must only do configuration parsing (no heavy init), so dry runs work.
  NesHawk(const nlohmann::json& config) : Emulator(config)
  {
    _romFilePath             = jaffarCommon::json::popString(_emulatorConfigRemaining, "Rom File Path");
    _romFileSHA1             = jaffarCommon::json::popString(_emulatorConfigRemaining, "Rom File SHA1");
    _initialStateFilePath    = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial State File Path");
    _initialSequenceFilePath = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial Sequence File Path");
    _initialRAMDataFilePath  = jaffarCommon::json::popString(_emulatorConfigRemaining, "Initial RAM Data File Path");

    // The NES joypad input parser ("|..|UDLRSsBA|" -> port byte A=0x01..R=0x80, which is exactly
    // the core's ButtonBit order). It reads the controller-type keys without popping, so consume
    // them here for the strict leftover-key check.
    _inputParser = std::make_unique<jaffar::InputParser>(config);
    jaffarCommon::json::popString(_emulatorConfigRemaining, "Controller 1 Type");
    jaffarCommon::json::popString(_emulatorConfigRemaining, "Controller 2 Type");
  }

  ~NesHawk() = default;

  void initializeImpl() override
  {
    // Reading from ROM file
    std::string romFileData;
    if (jaffarCommon::file::loadStringFromFile(romFileData, _romFilePath.c_str()) == false) JAFFAR_THROW_LOGIC("Could not find/read from ROM file: %s\n", _romFilePath.c_str());

    // Getting SHA1 of ROM for checksum
    auto actualRomSHA1 = jaffarCommon::hash::getSHA1String(romFileData);
    if (_romFileSHA1 != actualRomSHA1)
      JAFFAR_THROW_LOGIC("ROM file: '%s' expected SHA1 ('%s') does not coincide with the one read ('%s')\n", _romFilePath.c_str(), _romFileSHA1.c_str(), actualRomSHA1.c_str());

    // Power-on (parses the iNES header, boots the hardcoded NES-UNROM board)
    _nes = std::make_unique<nesHawk::NES>(reinterpret_cast<const uint8_t*>(romFileData.data()), romFileData.size());

    // Plug a second standard joypad into port 2 when the config asks for one (default: unplugged,
    // which is byte-exact with the P1-only translation). Rockman's game-end glitch reads controller 2.
    _nes->controllerDeck._port2Connected = (_inputParser->_controller2Type != jaffar::InputParser::controller_t::none);

    _stateBuffer.resize(nesHawk::nesStateSize(*_nes));

    // If initial state file defined, load it
    if (_initialStateFilePath.empty() == false)
    {
      std::string initialState;
      if (jaffarCommon::file::loadStringFromFile(initialState, _initialStateFilePath) == false)
        JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from initial state file: %s\n", _initialStateFilePath.c_str());

      jaffarCommon::deserializer::Contiguous d(initialState.data(), initialState.size());
      deserializeState(d);
    }

    // Advancing the state using the initial sequence, if provided
    if (_initialSequenceFilePath != "")
    {
      std::string initialSequenceFileString;
      if (jaffarCommon::file::loadStringFromFile(initialSequenceFileString, _initialSequenceFilePath) == false)
        JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from initial sequence file: %s\n", _initialSequenceFilePath.c_str());

      const auto initialSequence = jaffarCommon::string::split(initialSequenceFileString, '\0');
      for (const auto& inputString : initialSequence) advanceStateImpl(_inputParser->parseInputString(inputString));
    }

    // Pushing initial RAM data
    if (_initialRAMDataFilePath != "")
    {
      std::string initialRAMDataString;
      if (jaffarCommon::file::loadStringFromFile(initialRAMDataString, _initialRAMDataFilePath) == false)
        JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from RAM Data file: %s\n", _initialRAMDataFilePath.c_str());

      memcpy(_nes->ram, initialRAMDataString.data(), 0x800);
    }
  }

  jaffar::InputParser* getInputParser() const override { return _inputParser.get(); }

  void advanceStateImpl(const jaffar::input_t& input) override
  {
    // Lag-frame sibling cache: when a frame is lag (the game never latched the controller,
    // nes->islag), its outcome is provably input-independent -- every sibling input expanded from
    // the same parent produces the byte-identical child. The search engine's expansion pattern is
    // deserialize(parent) -> advance(input_k) for each candidate input, so on a lag frame the
    // first sibling's result can be served to the rest as a state load (~100x cheaper than a
    // frame). Exact: keyed on the full parent state bytes, valid only in the
    // deserialize-then-advance pattern, and the cached child is loaded into the core so every
    // later observer (serialize, memory properties) sees the real state.
    if (_parentFresh && _lagChildValid && _stateBuffer == _lagParent)
    {
      nesHawk::StateLoader l(_lagChild.data());
      nesHawk::syncNesState(*_nes, l);
      updateHaltLatch();
      _parentFresh = false;
      return;
    }

    const bool parentWasFresh = _parentFresh;
    _parentFresh              = false;

    _nes->FrameAdvance((uint8_t)input.port1, (uint8_t)input.port2);
    updateHaltLatch();

    if (parentWasFresh && _nes->islag)
    {
      // _stateBuffer still holds the parent bytes (advance does not touch it)
      _lagParent = _stateBuffer;
      _lagChild.resize(_stateBuffer.size());
      nesHawk::StateSaver s(_lagChild.data());
      nesHawk::syncNesState(*_nes, s);
      _lagChildValid = true;
    }
  }

  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const override
  {
    nesHawk::StateSaver s(_stateBuffer.data());
    nesHawk::syncNesState(*_nes, s);
    serializer.push(_stateBuffer.data(), _stateBuffer.size());
  }

  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer) override
  {
    deserializer.pop(_stateBuffer.data(), _stateBuffer.size());
    nesHawk::StateLoader l(_stateBuffer.data());
    nesHawk::syncNesState(*_nes, l);
    updateHaltLatch();
    _parentFresh = true; // enables the lag-frame sibling cache for the next advance
  }

  property_t getProperty(const std::string& propertyName) const override
  {
    // Same names as the QuickerNES wrapper, so game headers work unchanged. All pointers are
    // stable after initialize (the core never reallocates its state objects mid-run).
    if (propertyName == "LRAM") return property_t(_nes->ram, sizeof(_nes->ram));
    if (propertyName == "NTAB") return property_t(_nes->CIRAM, sizeof(_nes->CIRAM));
    if (propertyName == "CHRR") return property_t(_nes->board->Vram.data(), _nes->board->Vram.size());
    if (propertyName == "SPRT") return property_t(_nes->ppu->OAM, sizeof(_nes->ppu->OAM));
    if (propertyName == "PALR") return property_t(_nes->ppu->PALRAM, sizeof(_nes->ppu->PALRAM));
    // CPU halt detector: 1 when the CPU is executing a KIL/JAM opcode. NesHawk's Jam micro-op
    // freezes the microcode step forever (NMI is never serviced), so a KIL opcode observed at a
    // frame boundary means the CPU is jammed for good -- games should register this and FAIL the
    // state, matching real-hardware freeze semantics. Derived from cpu.opcode (serialized), so it
    // is recomputed after every advance/deserialize and needs no storage of its own.
    if (propertyName == "CPU Halt Latch") return property_t(const_cast<uint8_t*>(&_cpuHaltLatch), 1);
#ifdef _NESHAWK_DETECT_BAD_ACCESS
    // Per-frame ground-truth bad-access flag (unofficial opcode / RAM execution this frame). The
    // glitch discriminator for run 3: a REAL derail sets this; a normal off-path room transition
    // does not. Points directly at the CPU's latch (stable after initialize).
    if (propertyName == "CPU Bad Access") return property_t(&_nes->cpu->badAccessLatch, 1);
#endif

    JAFFAR_THROW_LOGIC("Property name: '%s' not found in emulator '%s'", propertyName.c_str(), getName().c_str());
  }

  __INLINE__ void printInfo() const override
  {
    jaffarCommon::logger::log("[J+]  + Initial Sequence File Path:          '%s'\n", _initialSequenceFilePath.c_str());
    jaffarCommon::logger::log("[J+]  + Frame / Lag Count:                   %d / %d\n", _nes->_frame, _nes->_lagcount);
    jaffarCommon::logger::log("[J+]  + State Size:                          %zu bytes\n", _stateBuffer.size());
    jaffarCommon::logger::log("[J+]  + CPU Halted:                          %u\n", _cpuHaltLatch);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Video output. The NesHawk PPU always renders into its framebuffer ppu->xbuf[256*240] -- each
  // entry is a 9-bit value (6-bit NES color index in bits 0-5 | color-emphasis in bits 6-8). We map
  // the base color index through a standard 64-entry NES palette into a 0x00RRGGBB blit buffer, which
  // both feeds the SDL window (showRender) and BMP screenshots (saveScreenshot). Mirrors the
  // QuickerNES wrapper so jaffar-player behaves identically across cores. Emphasis bits are ignored
  // (they only slightly tint the picture), which is more than enough to see the game.
  static constexpr int NESHAWK_WIDTH  = 256;
  static constexpr int NESHAWK_HEIGHT = 240;
  static constexpr int NESHAWK_BLIT   = 256 * 256; // texture is 256x256; only the top 256x240 is used

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

    m_window = SDL_CreateWindow("JaffarPlus", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, NESHAWK_WIDTH, NESHAWK_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (m_window == nullptr)
    {
      jaffarCommon::logger::log("[J+] WARNING: no SDL window; headless (screenshots only).\n");
      return;
    }

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
      JAFFAR_THROW_RUNTIME("Could not create SDL texture in NesHawk emulator");
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    m_nesDest = {0, 0, NESHAWK_WIDTH, NESHAWK_HEIGHT};
  }

  void finalizeVideoOutput() override
  {
    if (m_tex) SDL_DestroyTexture(m_tex);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    m_tex      = nullptr;
    m_renderer = nullptr;
    m_window   = nullptr;
  }

  // The PPU renders unconditionally, so there is nothing to toggle; kept for interface parity.
  __INLINE__ void enableRendering() override {}
  __INLINE__ void disableRendering() override {}

  __INLINE__ void updateRendererState(const size_t /*stepIdx*/, const std::string /*input*/) override
  {
    const int16_t* xbuf = _nes->ppu->xbuf;
    for (int i = 0; i < NESHAWK_WIDTH * NESHAWK_HEIGHT; i++) _curBlit[i] = _nesPalette[xbuf[i] & 0x3F];
  }

  // Write the current rendered frame (_curBlit) to a 24-bit BMP. No SDL/display needed.
  // Refreshes _curBlit from the PPU framebuffer first: the player's headless screenshot pass
  // calls saveScreenshot without updateRendererState (QuickerNES self-refreshes; so do we).
  void saveScreenshot(const std::string& path) override
  {
    updateRendererState(0, "");
    const int W            = NESHAWK_WIDTH;
    const int H            = NESHAWK_HEIGHT;
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

  __INLINE__ void showRender() override
  {
    if (m_renderer == nullptr) return; // headless (screenshot-only) mode

    void* nesPixels = nullptr;
    int   pitch     = 0;
    if (SDL_LockTexture(m_tex, nullptr, &nesPixels, &pitch) < 0) JAFFAR_THROW_RUNTIME("Could not lock texture in NesHawk emulator");
    memcpy(nesPixels, _curBlit, sizeof(int32_t) * NESHAWK_BLIT);
    SDL_UnlockTexture(m_tex);

    SDL_RenderClear(m_renderer);
    const SDL_Rect blitRect = {0, 0, NESHAWK_WIDTH, NESHAWK_HEIGHT};
    SDL_RenderCopy(m_renderer, m_tex, &blitRect, &m_nesDest);
    SDL_RenderPresent(m_renderer);
  }

  // The renderer state is the pixel buffer itself (populated from the live PPU framebuffer by
  // updateRendererState during the player's caching pass), matching the QuickerNES contract: the
  // player caches _curBlit per step and replays it back for display/screenshots.
  __INLINE__ void   serializeRendererState(jaffarCommon::serializer::Base& serializer) const override { serializer.pushContiguous(_curBlit, sizeof(int32_t) * NESHAWK_BLIT); }
  __INLINE__ void   deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) override { deserializer.popContiguous(_curBlit, sizeof(int32_t) * NESHAWK_BLIT); }
  __INLINE__ size_t getRendererStateSize() const override
  {
    jaffarCommon::serializer::Contiguous s;
    serializeRendererState(s);
    return s.getOutputSize();
  }

private:
  // Standard 64-color NES (2C02) palette as 0x00RRGGBB; index = ppu->xbuf & 0x3F.
  static constexpr int32_t _nesPalette[64] = {0x545454, 0x001E74, 0x081090, 0x300088, 0x440064, 0x5C0030, 0x540400, 0x3C1800, 0x202A00, 0x083A00, 0x004000, 0x003C00, 0x00323C,
                                              0x000000, 0x000000, 0x000000, 0x989698, 0x084CC4, 0x3032EC, 0x5C1EE4, 0x8814B0, 0xA01464, 0x982220, 0x783C00, 0x545A00, 0x287200,
                                              0x087C00, 0x007628, 0x006678, 0x000000, 0x000000, 0x000000, 0xECEEEC, 0x4C9AEC, 0x787CEC, 0xB062EC, 0xE454EC, 0xEC58B4, 0xEC6A64,
                                              0xD48820, 0xA0AA00, 0x74C400, 0x4CD020, 0x38CC6C, 0x38B4CC, 0x3C3C3C, 0x000000, 0x000000, 0xECEEEC, 0xA8CCEC, 0xBCBCEC, 0xD4B2EC,
                                              0xECAEEC, 0xECAED4, 0xECB4B0, 0xE4C490, 0xCCD278, 0xB4DE78, 0xA8E290, 0x98E2B4, 0xA0D6E4, 0xA0A2A0, 0x000000, 0x000000};

  // SDL video state (mirrors the QuickerNES wrapper)
  SDL_Window*   m_window               = nullptr;
  SDL_Renderer* m_renderer             = nullptr;
  SDL_Texture*  m_tex                  = nullptr;
  SDL_Rect      m_nesDest              = {0, 0, NESHAWK_WIDTH, NESHAWK_HEIGHT};
  int32_t       _curBlit[NESHAWK_BLIT] = {0}; // 0x00RRGGBB, top 256x240 populated each rendered frame

  __INLINE__ void updateHaltLatch()
  {
    // KIL/JAM opcodes: x2 for x in {0,1,2,3,4,5,6,7,9,B,D,F}. A jammed CPU never leaves the
    // opcode, so this is exact (and inherently sticky) at frame granularity.
    switch (_nes->cpu->opcode)
    {
      case 0x02:
      case 0x12:
      case 0x22:
      case 0x32:
      case 0x42:
      case 0x52:
      case 0x62:
      case 0x72:
      case 0x92:
      case 0xB2:
      case 0xD2:
      case 0xF2: _cpuHaltLatch = 1; break;
      default: _cpuHaltLatch = 0; break;
    }
  }

  std::unique_ptr<nesHawk::NES>        _nes;
  std::unique_ptr<jaffar::InputParser> _inputParser;
  mutable std::vector<uint8_t>         _stateBuffer;
  uint8_t                              _cpuHaltLatch = 0;

  // lag-frame sibling cache (see advanceStateImpl)
  bool                 _parentFresh   = false;
  bool                 _lagChildValid = false;
  std::vector<uint8_t> _lagParent;
  std::vector<uint8_t> _lagChild;

  std::string _romFilePath;
  std::string _romFileSHA1;
  std::string _initialStateFilePath;
  std::string _initialSequenceFilePath;
  std::string _initialRAMDataFilePath;
};

} // namespace emulator

} // namespace jaffarPlus
