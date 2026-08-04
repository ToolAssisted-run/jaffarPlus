/**
 * @file player.cpp
 * @brief The jaffar-player (jaffar-tester) executable: loads a config and a solution sequence and
 *        plays it back through a @ref jaffarPlus::Runner.
 *
 * @details Supports interactive stepping/playback in a terminal, headless/unattended replay,
 * per-frame BMP screenshot capture, single-command runs (@c --runCommand), and a machine-readable
 * final-state summary (@c --printFinalState) used as an oracle for headless reproduction tests
 * (determinism / golden-hash comparisons). Behavior is driven by the command-line flags parsed in
 * @ref main and the file-scope switches below.
 */

#include "emulator.hpp"
#include "game.hpp"
#include "playback.hpp"
#include "runner.hpp"
#include <argparse/argparse.hpp>
#include <chrono>
#include <emulatorList.hpp>
#include <gameList.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/string.hpp>
#include <map>
#include <set>

bool isUnattended; ///< Prevents the interactive player from stalling for a keystroke.

bool isExitOnEnd; ///< Determines that the reproduction must end on reaching the last step.

bool isReload; ///< Switch to toggle whether to reload the movie on reaching the end of the sequence.

bool isReproduce; ///< Switch to toggle whether to reproduce (auto-advance) the movie.

size_t frameskip; ///< Number of frames to skip between renderings.

std::string runCommand; ///< Command to run initially and then exit.

/// @brief When set, prints a stable, machine-readable summary of the final state on exit (for
///        headless verification of a reproduction: determinism checks and golden-hash comparisons).
bool printFinalState;

/// @brief When non-empty, writes the per-step game-state hash for every step to this file (one
///        "step\thashHi\thashLo" line per step). Used to cross-validate two emulators replaying the
///        same solution: diffing the two dumps pinpoints the exact first frame at which they diverge.
std::string dumpHashesPath;

/// @brief When non-empty, writes the full low work-RAM ("LRAM") segment for every step to this file
///        as a flat binary blob (size-of-LRAM bytes per step). Diffing two emulators' RAM dumps
///        byte-by-byte identifies the exact RAM addresses (hence game variables) that diverge.
std::string dumpRamPath;
/// @brief When non-empty, writes the full VRAM for every step to this file as a flat binary blob.
std::string dumpVramPath;

/// @brief When non-empty (--dumpPolls), writes the per-step joypad read count ("step\tcount" per line)
///        for the replayed solution to this file. A count of 0 marks a lag frame (input not polled);
///        the polled frame right before a lag group is a level's true start frame. The counter is not
///        part of the serialized state, so this pass replays the solution live instead of restoring
///        per-step states.
std::string dumpPollsPath;

/// @brief When non-empty (--dumpReward), writes the per-step game reward (one value per line) for the
///        replayed solution to this file. Suitable as a driver "Reference Reward Floor" trace.
std::string dumpRewardPath;

/// @brief When non-empty (--dumpTrace), writes the game's per-step trace line (Game::getTraceLine, one line per
///        step) for the replayed solution to this file. Suitable as a game "Trace File Path" (trace-magnet ref).
std::string dumpTracePath;
/// @brief When non-empty (--dumpRacer), dumps frame,gx,gy,spd + the full 234-byte racer struct per frame.
std::string dumpRacerPath;

/// @brief When set (--saveStateStep), the step at which to capture a full emulator savestate (paired
///        with --saveStateFile). Parsed as an unsigned step index; empty unless saving is requested.
std::string saveStateStepStr;

/// @brief When non-empty (--saveStateFile), the path to write the emulator savestate captured at
///        --saveStateStep (e.g. to seed another search cleanly from a chosen frame).
std::string saveStateFilePath;

/// @brief Directory to write per-frame screenshots (BMP) into; empty disables screenshotting.
std::string screenshotDir;
/// @brief Steps to capture as screenshots; empty captures all rendered steps when a dir is given.
std::set<size_t> screenshotSteps;

/// @brief Work-RAM bytes to poke into the post-initial-sequence state before replaying (for mechanic
///        experiments, e.g. perturbing an RNG/counter). Parsed from --pokeRAM "0xADDR=VAL,0xADDR=VAL".
std::vector<std::pair<uint32_t, uint8_t>> pokeRam;
/// @brief When set (--terrainSweep "out.csv:x0,x1,y0,y1,step"), probe the terrain the game computes at a grid
///        of player positions: reload the post-init state, poke player pos (0x80A6/0x80A8), step, read the
///        surface byte (0xA649), attribute (0xA648) and fall state (0xA66C). Writes CSV and exits.
std::string terrainSweepSpec;
/// @brief When set (--terrainDrive out.csv), branch off the replayed solution every few frames and steer a REAL
///        driving car into neighboring cells, logging the true physics effect (min speed = slowdown, fall = hole,
///        max Z = jump) per grid cell. Correct effects (consistent state) for the drivable band. Writes CSV & exits.
std::string terrainDrivePath;

/**
 * @brief Parses a non-negative integer from a CLI argument value.
 * @details Reports a clear logic error (instead of a raw std::stoul exception) when the value is
 * malformed or contains trailing characters.
 * @param value The string value to parse.
 * @param flag  The name of the CLI flag the value came from, used in the error message.
 * @return The parsed non-negative integer.
 */
static size_t parseUInt(const std::string& value, const std::string& flag)
{
  try
  {
    size_t       consumed = 0;
    const size_t result   = std::stoul(value, &consumed);
    if (consumed == value.size()) return result;
  }
  catch (const std::exception&)
  {
  }
  JAFFAR_THROW_LOGIC("Invalid value '%s' for %s (expected a non-negative integer)\n", value.c_str(), flag.c_str());
  return 0;
}

/**
 * @brief Runs one full pass over a solution sequence, optionally interactive, and reports state info.
 *
 * @details Loads the solution sequence file, builds a @ref jaffarPlus::Playback over the runner, and
 * enters a loop that (unless rendering is disabled) renders the current frame, optionally saves a BMP
 * screenshot for the current step, prints per-step information (step index, input, hashes, allowed
 * inputs, game/emulator names, checkpoint and save-solution status, plus game-specific info), and
 * handles navigation/playback commands. Commands move the current step (n/m +/-1, h/j +/-10, y/u
 * +/-100, k/i +/-1000), quicksave the emulator state to "quicksave.state" (s), toggle playback (p),
 * toggle auto-reload (r), quit (q), or are forwarded to the game's player command parser. In
 * unattended or reproduce mode, key input is non-blocking; @c runCommand forces a single command then
 * finalizes. On reaching the end it reloads, exits, or stops depending on the reload/exit-on-end
 * flags. When @c printFinalState is set, prints a stable end-of-sequence summary (final step, state
 * type, first win/fail steps, final state hash, not-allowed-input count, repeated-state count).
 *
 * @param r             The runner to play the solution back on.
 * @param solutionFile  Path to the solution sequence file to load and reproduce.
 * @param disableRender Whether to skip frame rendering (and screenshot capture).
 * @return false when the cycle finalized (quit requested), true to keep looping (e.g. for reload).
 */
bool mainCycle(jaffarPlus::Runner& r, const std::string& solutionFile, bool disableRender)
{
  // If sequence file defined, load it and play it
  std::string solutionFileString;
  if (jaffarCommon::file::loadStringFromFile(solutionFileString, solutionFile) == false)
    JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from solution sequence file: %s\n", solutionFile.c_str());

  // Getting input sequence
  const auto solutionSequence = jaffarCommon::string::split(solutionFileString, '\0');

  // Optional RAM poke(s) into the post-initial-sequence state, before any replay (mechanic experiments).
  if (pokeRam.empty() == false)
  {
    auto* ram = r.getGame()->getEmulator()->getProperty("LRAM").pointer;
    for (const auto& [addr, val] : pokeRam)
    {
      ram[addr] = val;
      jaffarCommon::logger::log("[J+] Poked RAM 0x%04X = 0x%02X (%u)\n", addr, val, val);
    }
  }

  // Per-frame trace: replay the solution and dump frame,gx,gy,spd + the whole racer struct (234 bytes) so we can
  // empirically find the byte that flags "on honey" (true on the real slowdown crossing, false in corners).
  if (dumpRacerPath.empty() == false)
  {
    auto* emu  = r.getGame()->getEmulator();
    auto* ram  = emu->getProperty("LRAM").pointer;
    auto  rd16 = [&](int a) { return ram[a] | (ram[a + 1] << 8); };
    auto  s16  = [&](int a)
    {
      int v = rd16(a);
      return v >= 0x8000 ? v - 0x10000 : v;
    };
    auto spd = [&]()
    {
      double vx = s16(0xA614) + rd16(0xA616) / 65536.0, vy = s16(0xA618) + rd16(0xA61A) / 65536.0;
      return std::sqrt(vx * vx + vy * vy);
    };
    std::string out = "frame,gx,gy,spd,posX,posY"; // posX/posY = Player 1 Pos X/Y (0x80A6/0x80A8), the magnet/reference coords
    for (int i = 0; i < 234; i++)
    {
      char h[8];
      snprintf(h, sizeof h, ",b%02X", i);
      out += h;
    }
    out += "\n";
    char buf[80];
    for (size_t f = 0; f < solutionSequence.size(); f++)
    {
      r.advanceState(emu->registerInput(solutionSequence[f]));
      snprintf(buf, sizeof buf, "%zu,%d,%d,%.3f,%d,%d", f, rd16(0xA658) & 31, rd16(0xA656) & 31, spd(), rd16(0x80A6), rd16(0x80A8));
      out += buf;
      for (int i = 0; i < 234; i++)
      {
        snprintf(buf, sizeof buf, ",%d", ram[0xA60C + i]);
        out += buf;
      }
      out += "\n";
    }
    jaffarCommon::file::saveStringToFile(out, dumpRacerPath.c_str());
    return false;
  }

  // Coverage drive: branch off the replayed solution and steer a REAL car into neighbors, logging true effects
  // (min speed = slowdown, fall state = hole, max Z = jump) per grid cell. Consistent physics state throughout.
  if (terrainDrivePath.empty() == false)
  {
    auto* emu  = r.getGame()->getEmulator();
    auto* ram  = emu->getProperty("LRAM").pointer;
    auto  rd16 = [&](int a) { return ram[a] | (ram[a + 1] << 8); };
    auto  s16  = [&](int a)
    {
      int v = rd16(a);
      return v >= 0x8000 ? v - 0x10000 : v;
    };
    auto spd = [&]()
    {
      double vx = s16(0xA614) + rd16(0xA616) / 65536.0, vy = s16(0xA618) + rd16(0xA61A) / 65536.0;
      return std::sqrt(vx * vx + vy * vy);
    };
    // Fan the car widely into adjacent cells (accelerate straight / soft & hard L/R) so the probe reaches
    // off-racing-line cells from their normal neighbours — the "drive through it from an adjacent block" method.
    // Faithful terrain map = the REAL driven racing line (stays on-track by construction). For each cell the
    // car actually drives through we record the true effect: attr (0xA648, honey sinks it), surf category
    // (0xA649), speed band, fall (0xA66C, hole), Z (0xA64C, jump/ramp). No teleport, no off-road excursions.
    struct Agg
    {
      double minSpd = 99, maxSpd = 0, maxDrop = 0;
      int    maxFall = 0, maxZ = -999, minAttr = 99, maxAttr = 0, n = 0, surf = 0;
    };
    std::map<std::pair<int, int>, Agg> cells;
    double                             prev = spd();
    for (size_t f = 0; f < solutionSequence.size(); f++)
    {
      r.advanceState(emu->registerInput(solutionSequence[f]));
      double s = spd(), drop = prev - s;
      prev    = s;
      auto& a = cells[{rd16(0xA658), rd16(0xA656)}];
      if (s < a.minSpd) a.minSpd = s;
      if (s > a.maxSpd) a.maxSpd = s;
      if (drop > a.maxDrop) a.maxDrop = drop;
      if (ram[0xA66C] > a.maxFall) a.maxFall = ram[0xA66C];
      int z = s16(0xA64C);
      if (z > a.maxZ) a.maxZ = z;
      int at = ram[0xA648];
      if (at < a.minAttr) a.minAttr = at;
      if (at > a.maxAttr) a.maxAttr = at;
      a.surf = ram[0xA649];
      a.n++;
    }
    std::string csv = "gx,gy,surf,minAttr,maxAttr,minSpd,maxSpd,maxDrop,maxFall,maxZ,n\n";
    for (auto& kv : cells)
    {
      char buf[160];
      snprintf(buf, sizeof buf, "%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%d,%d,%d\n", kv.first.first, kv.first.second, kv.second.surf, kv.second.minAttr, kv.second.maxAttr, kv.second.minSpd,
               kv.second.maxSpd, kv.second.maxDrop, kv.second.maxFall, kv.second.maxZ, kv.second.n);
      csv += buf;
    }
    jaffarCommon::file::saveStringToFile(csv, terrainDrivePath.c_str());
    jaffarCommon::logger::log("[J+] Terrain drive: %lu cells logged to %s\n", cells.size(), terrainDrivePath.c_str());
    return false;
  }

  // Synthetic terrain sweep: probe the surface the game computes at a grid of player positions.
  if (terrainSweepSpec.empty() == false)
  {
    // parse "out.csv:x0,x1,y0,y1,step"
    auto         colon   = terrainSweepSpec.find(':');
    std::string  outPath = terrainSweepSpec.substr(0, colon);
    auto         nums    = jaffarCommon::string::split(terrainSweepSpec.substr(colon + 1), ',');
    int          x0 = std::stoi(nums[0]), x1 = std::stoi(nums[1]), y0 = std::stoi(nums[2]), y1 = std::stoi(nums[3]), st = std::stoi(nums[4]);
    auto*        emu       = r.getGame()->getEmulator();
    auto*        ram       = emu->getProperty("LRAM").pointer;
    const size_t stateSize = emu->getStateSize();
    // capture the base (post-init) state
    std::string base;
    base.resize(stateSize);
    {
      jaffarCommon::serializer::Contiguous s(base.data(), stateSize);
      emu->serializeState(s);
    }
    const auto accelInput = emu->registerInput("|..|.....B..|"); // hold accelerate (B) so the car actually DRIVES
    // Force the whole player position block to the target world (x,y) every frame so no velocity update can drift
    // it, then read the surface (0xA649), attribute (0xA648), checkpoint (0xA6DB) and fall (0xA66C) the game computes.
    // DRIVE-THROUGH PROBE: for each grid cell, place a MOVING car one cell before it (on a road approach) and drive
    // it INTO the cell, measuring the true physics response: slowdown, slip, fall (hole), bump (solid), jump (ramp).
    // grid gy <-> racer[0x00](0xA60C) (worldX), grid gx <-> racer[0x04](0xA610) (worldY); vel = racer[0x08]/[0x0C].
    auto rd16 = [&](int adr) { return ram[adr] | (ram[adr + 1] << 8); };
    auto s16  = [&](int adr)
    {
      int v = rd16(adr);
      return v >= 0x8000 ? v - 0x10000 : v;
    };
    auto setPos = [&](int a, int b)
    {
      ram[0xA60C] = a & 0xFF;
      ram[0xA60D] = (a >> 8) & 0xFF;
      ram[0xA60E] = 0;
      ram[0xA60F] = 0;
      ram[0xA610] = b & 0xFF;
      ram[0xA611] = (b >> 8) & 0xFF;
      ram[0xA612] = 0;
      ram[0xA613] = 0;
    };
    auto setVel = [&](int vx, int vy)
    {
      ram[0xA614] = vx & 0xFF;
      ram[0xA615] = (vx >> 8) & 0xFF;
      ram[0xA616] = 0;
      ram[0xA617] = 0;
      ram[0xA618] = vy & 0xFF;
      ram[0xA619] = (vy >> 8) & 0xFF;
      ram[0xA61A] = 0;
      ram[0xA61B] = 0;
    };
    auto speed = [&]()
    {
      double vx = rd16(0xA614) + rd16(0xA616) / 65536.0, vy = rd16(0xA618) + rd16(0xA61A) / 65536.0;
      return std::sqrt(vx * vx + vy * vy);
    };
    auto gyOf = [&]() { return ((rd16(0xA60C) >> 3) / 12) & 31; };
    auto gxOf = [&]() { return ((rd16(0xA610) >> 3) / 12) & 31; };
    // Drive the car INTO target cell (gx,gy) from one adjacent cell, along one of the 4 axes. The approach cell
    // and the whole path must stay ON-track: if recovery (0xA66C) fires BEFORE we reach the target, this approach
    // started/passed through void -> discard it. A hole = 0xA66C fires only AFTER we cleanly enter the target.
    // Returns a per-approach result. dir: 0=+X(from gy-1) 1=-X(gy+1) 2=+Y(gx-1) 3=-Y(gx+1).
    struct Res
    {
      int    reached = 0, valid = 0, blocked = 0, hole = 0, maxZ = -32768, minAttr = 99, surf = -1;
      double minSpd = 99;
    };
    auto probe = [&](int gx, int gy, int dir) -> Res
    {
      Res R;
      {
        jaffarCommon::deserializer::Contiguous d(base.data(), stateSize);
        emu->deserializeState(d);
      }
      const int V = 6; // approach speed (px/frame): ~16 frames to cross a 96u cell
      if (dir == 0)
      {
        setPos((gy - 1) * 96 + 48, gx * 96 + 48);
        setVel(+V, 0);
      }
      else if (dir == 1)
      {
        setPos((gy + 1) * 96 + 48, gx * 96 + 48);
        setVel(-V, 0);
      }
      else if (dir == 2)
      {
        setPos(gy * 96 + 48, (gx - 1) * 96 + 48);
        setVel(0, +V);
      }
      else
      {
        setPos(gy * 96 + 48, (gx + 1) * 96 + 48);
        setVel(0, -V);
      }
      ram[0xA66C]  = 0;
      int lastMove = rd16(dir < 2 ? 0xA60C : 0xA610), stuck = 0, entered = 0;
      for (int k = 0; k < 32; k++)
      {
        r.advanceState(accelInput);
        bool inTarget = (gyOf() == gy && gxOf() == gx);
        if (!entered && ram[0xA66C] != 0)
        {
          R.valid = 0;
          return R;
        } // recovery before entry -> void approach, discard
        if (inTarget)
        {
          entered   = 1;
          R.reached = 1;
          R.valid   = 1;
          double s  = speed();
          if (s < R.minSpd) R.minSpd = s;
          int at = rd16(0xA648) & 0xFF;
          if (at < R.minAttr) R.minAttr = at;
          R.surf = rd16(0xA649) & 0xFF;
          int z  = s16(0xA64C);
          if (z > R.maxZ) R.maxZ = z;
          if (ram[0xA66C] != 0) R.hole = 1; // fell AT the target on a clean approach = real hole
        }
        int mv = rd16(dir < 2 ? 0xA60C : 0xA610);
        if (std::abs(mv - lastMove) < 2)
          stuck++;
        else
          stuck = 0;
        lastMove = mv;
        if (stuck >= 6)
        {
          if (!entered) R.blocked = 1;
          R.valid = 1;
          break;
        } // stopped before entering = solid wall
      }
      return R;
    };
    // DIAGNOSTIC: single-cell range -> poke car onto the tile, hold accelerate, print attr/surf/spd/fall per frame
    if (x0 == x1 && y0 == y1)
    {
      {
        jaffarCommon::deserializer::Contiguous d(base.data(), stateSize);
        emu->deserializeState(d);
      }
      setPos(x0 * 96 + 48, y0 * 96 + 48);
      setVel(2, 0);
      ram[0xA66C]    = 0;
      std::string dg = "f,gy,gx,attr,surf,spd,fall,Z\n";
      char        b[128];
      for (int k = 0; k < 90; k++)
      {
        r.advanceState(accelInput);
        snprintf(b, sizeof b, "%d,%d,%d,%d,%d,%.2f,%d,%d\n", k, gyOf(), gxOf(), rd16(0xA648) & 0xFF, rd16(0xA649) & 0xFF, speed(), ram[0xA66C], s16(0xA64C));
        dg += b;
      }
      jaffarCommon::file::saveStringToFile(dg, outPath.c_str());
      return false;
    }
    // interpret range as GRID cells: gy in [x0,x1], gx in [y0,y1]
    std::string csv = "gy,gx,reached,surf,minAttr,minSpd,hole,maxZ,solid,nApproach\n";
    for (int gx = y0; gx <= y1; gx += st)
      for (int gy = x0; gy <= x1; gy += st)
      {
        Res best;
        int nApproach = 0, solidVotes = 0;
        for (int dir = 0; dir < 4; dir++)
        {
          Res R = probe(gx, gy, dir);
          if (!R.valid) continue; // approach came through void; ignore
          nApproach++;
          if (R.blocked)
          {
            solidVotes++;
            continue;
          }
          if (R.reached)
          { // merge the effect from any clean reaching approach
            best.reached = 1;
            if (R.minAttr < best.minAttr) best.minAttr = R.minAttr;
            if (R.minSpd < best.minSpd) best.minSpd = R.minSpd;
            if (R.maxZ > best.maxZ) best.maxZ = R.maxZ;
            if (R.hole) best.hole = 1;
            best.surf = R.surf;
          }
        }
        if (best.minSpd > 50) best.minSpd = 0;
        // solid = a clean road approach reached the adjacent cell but was walled out, and NO approach entered
        int  solid = (best.reached == 0 && solidVotes > 0) ? 1 : 0;
        char buf[160];
        snprintf(buf, sizeof buf, "%d,%d,%d,%d,%d,%.2f,%d,%d,%d,%d\n", gy, gx, best.reached, best.surf, best.reached ? best.minAttr : -1, best.minSpd, best.hole, best.maxZ, solid,
                 nApproach);
        csv += buf;
      }
    jaffarCommon::file::saveStringToFile(csv, outPath.c_str());
    jaffarCommon::logger::log("[J+] Terrain sweep written to %s\n", outPath.c_str());
    return false;
  }

  // Variable for current step in view
  ssize_t currentStep = 0;

  // Getting sequence length
  const ssize_t sequenceLength = solutionSequence.size();

  // Getting inverse frame rate from game
  const auto     frameRate        = r.getGame()->getFrameRate();
  const uint32_t inverseFrameRate = std::round((1.0 / frameRate) * 1.0e+6);

  // Getting game state size
  const auto stateSize = r.getStateSize();

  // Printing information
  jaffarCommon::logger::refreshTerminal();

  // Instantiating playback instance
  jaffarPlus::Playback p(r);

  // Per-step joypad-poll dump: the read counter is reset and updated inside each frame advance and is
  // NOT part of the serialized state, so it cannot be recovered from restored per-step states like the
  // RAM/hash dumps below. Instead, replay the solution LIVE from the emulator's fresh step-0 state and
  // sample the counter right after each advance. One-shot terminal pass (returns false) so the caller's
  // playback loop does not re-run it.
  if (dumpPollsPath.empty() == false)
  {
    auto*       emu   = r.getGame()->getEmulator();
    const auto  polls = emu->getProperty("Joypad Read Count");
    std::string dump;
    char        line[64];
    for (ssize_t s = 0; s < sequenceLength; s++)
    {
      r.advanceState(emu->registerInput(solutionSequence[s]));
      snprintf(line, sizeof(line), "%ld\t%d\n", s + 1, *(const int*)polls.pointer);
      dump += line;
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpPollsPath.c_str()) == false)
      JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step poll dump to: %s\n", dumpPollsPath.c_str());
    return false;
  }

  // Headless screenshot pass: capture each requested step to BMP. A per-step state RESTORE does NOT correctly
  // repaint the framebuffer (GPGX's tile caches are not rebuilt by a state-load), so we replay the solution
  // linearly from the emulator's FRESH step-0 state (as left by the runner's init sequence, before the playback
  // build advances it) -- each advanceState() paints its frame -- and screenshot along the way, then finalize.
  // This is a one-shot terminal pass: we MUST return false (finalize/quit) so the caller's playback loop does
  // not re-run us. A repeat pass would restore the initial state via deserialize (which cannot repaint GPGX),
  // then overwrite every good frame with a black one -- exactly the black-frame regression this avoids.
  if (screenshotDir.empty() == false)
  {
    auto* emu = r.getGame()->getEmulator();
    emu->enableHeadlessRendering();
    for (ssize_t s = 0; s < sequenceLength; s++)
    {
      const auto inputIndex = emu->registerInput(solutionSequence[s]);
      r.advanceState(inputIndex);
      if (screenshotSteps.empty() || screenshotSteps.count((size_t)(s + 1)) > 0)
      {
        char path[1024];
        snprintf(path, sizeof(path), "%s/step_%06ld.bmp", screenshotDir.c_str(), s + 1);
        emu->saveScreenshot(path);
        // Also dump the LIVE video RAM (correct here, since we advanced via advanceState rather than a
        // state-load) so it can be lifted into a transplanted state whose vram is from a different track.
        // Not all emulators expose a VRAM property (e.g. QuickerNES) -- skip quietly if absent.
        try
        {
          const auto vram = emu->getProperty("VRAM");
          char       vpath[1024];
          snprintf(vpath, sizeof(vpath), "%s/vram_%06ld.bin", screenshotDir.c_str(), s + 1);
          std::string vdump((const char*)vram.pointer, vram.size);
          jaffarCommon::file::saveStringToFile(vdump, vpath);
        }
        catch (const std::exception&)
        {
        }
      }
    }
    return false;
  }

  // Initializing playback instance. In headless mode (--disableRender) renderFrame is never called,
  // so skip caching the per-step renderer framebuffer (~256KB/step) -- critical for long movies.
  p.initialize(solutionSequence, !disableRender);

  // Flag to display frame information
  bool showFrameInfo = true;

  // Finalization flag
  bool isFinalize = false;

  // Checking for repeated state hashes
  std::vector<ssize_t> repeatedHashStates;
  for (ssize_t i = 0; i < sequenceLength; i++)
  {
    const auto repeatedHashSteps = p.getStateRepeatedHashSteps(i);
    if (repeatedHashSteps.size() > 0) repeatedHashStates.push_back(i);
  }

  // Checking for not-allowed inputs
  std::vector<ssize_t> notAllowedInputStates;
  for (ssize_t i = 0; i < sequenceLength; i++)
  {
    const auto isInputAllowed = p.isInputAllowed(i);
    if (isInputAllowed == false) notAllowedInputStates.push_back(i);
  }

  // If requested, dump the per-step game-state hash for every step (including the end-of-sequence
  // step) to a file. Diffing the dumps of two emulators replaying the same solution pinpoints the
  // exact first frame at which their hashed game RAM diverges.
  if (dumpHashesPath.empty() == false)
  {
    std::string dump;
    char        line[64];
    for (ssize_t i = 0; i <= sequenceLength; i++)
    {
      const auto hash = p.getStateHash(i);
      snprintf(line, sizeof(line), "%ld\t%016lX%016lX\n", i, hash.first, hash.second);
      dump += line;
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpHashesPath.c_str()) == false)
      JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step hash dump to: %s\n", dumpHashesPath.c_str());
  }

  // If requested, dump the full low work-RAM for every step as a flat binary blob. Reading the RAM
  // requires restoring each step's state into the live emulator first (loadStepData), so this mutates
  // the live state -- harmless here since the interactive loop re-loads per step regardless.
  if (dumpRamPath.empty() == false)
  {
    const auto  lram = r.getGame()->getEmulator()->getProperty("LRAM");
    std::string dump;
    dump.reserve((size_t)(sequenceLength + 1) * lram.size);
    for (ssize_t i = 0; i <= sequenceLength; i++)
    {
      p.loadStepData(i);
      dump.append((const char*)lram.pointer, lram.size);
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpRamPath.c_str()) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step RAM dump to: %s\n", dumpRamPath.c_str());
  }

  // If requested, dump the full video RAM (VRAM) for every step as a flat binary blob (for finding the
  // VRAM region inside a foreign emulator savestate by cross-correlation).
  if (dumpVramPath.empty() == false)
  {
    const auto  vram = r.getGame()->getEmulator()->getProperty("VRAM");
    std::string dump;
    dump.reserve((size_t)(sequenceLength + 1) * vram.size);
    for (ssize_t i = 0; i <= sequenceLength; i++)
    {
      p.loadStepData(i);
      dump.append((const char*)vram.pointer, vram.size);
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpVramPath.c_str()) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step VRAM dump to: %s\n", dumpVramPath.c_str());
  }

  // If requested, write the per-step game reward (one value per line) to a file. The reward is part of the
  // serialized game state, so loadStepData restores each step's reward directly -- this is exactly the value
  // the search compares against, suitable as a "Reference Reward Floor" trace.
  if (dumpRewardPath.empty() == false)
  {
    std::string dump;
    for (ssize_t i = 0; i <= sequenceLength; i++)
    {
      p.loadStepData(i);
      // Full precision (NOT std::to_string, which truncates to 6 decimals): the reward is on the 1/256
      // sub-pixel grid, so 6 decimals (e.g. 0.246094) does not round-trip the exact value (0.24609375),
      // which makes a "Reference Reward Floor" with tolerance 0 false-cancel on an EXACT match. %.17g
      // preserves the value so a tol=0 floor only cancels when the search is genuinely behind.
      char rbuf[64];
      snprintf(rbuf, sizeof(rbuf), "%.17g", (double)r.getGame()->getReward());
      dump += std::string(rbuf) + "\n";
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpRewardPath.c_str()) == false)
      JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step reward dump to: %s\n", dumpRewardPath.c_str());
  }

  // If requested, write the game's per-step trace line (Game::getTraceLine) to a file, one line per step. Like the
  // reward dump, loadStepData restores each step's full state first, so the coordinates are exact. Suitable as a
  // game "Trace File Path" for the trace magnet.
  if (dumpTracePath.empty() == false)
  {
    std::string dump;
    for (ssize_t i = 0; i <= sequenceLength; i++)
    {
      p.loadStepData(i);
      dump += r.getGame()->getTraceLine() + "\n";
    }
    if (jaffarCommon::file::saveStringToFile(dump, dumpTracePath.c_str()) == false)
      JAFFAR_THROW_LOGIC("[ERROR] Could not write per-step trace dump to: %s\n", dumpTracePath.c_str());
  }

  // If requested, restore the state at a single step and save the emulator's FULL state to a file (for use
  // as the Emulator "Initial State File Path" -- a mid-run seed). Prints the bike posX so the caller can set
  // the game's "Initial Block Transitions" to make _bikePosX absolute. Then exits.
  if (saveStateFilePath.empty() == false)
  {
    const auto step = (ssize_t)parseUInt(saveStateStepStr, "--saveStateStep");
    p.loadStepData(step);
    // Full-fidelity save: enable ALL state properties (incl. the search-disabled SPRT/NTAB/CHRR/
    // SRAM) so the blob is loadable as an "Initial State File Path" by a freshly-constructed
    // emulator (which has everything enabled). Saving the reduced hot state produced an
    // undersized blob that the loader rejected ("Maximum input data position reached").
    auto* saveEmu = r.getGame()->getEmulator();
    saveEmu->enableAllStateProperties();
    std::string  saveData;
    const size_t stateSize = saveEmu->getStateSize();
    saveData.resize(stateSize);
    jaffarCommon::serializer::Contiguous s(saveData.data(), stateSize);
    saveEmu->serializeState(s);
    saveEmu->reapplyDisabledStateProperties();
    if (jaffarCommon::file::saveStringToFile(saveData, saveStateFilePath.c_str()) == false)
      JAFFAR_THROW_LOGIC("[ERROR] Could not write state at step %ld to: %s\n", (long)step, saveStateFilePath.c_str());
    jaffarCommon::logger::log("[J+] Saved emulator state at step %ld to %s (%lu bytes)\n", (long)step, saveStateFilePath.c_str(), stateSize);
    r.getGame()->printInfo();
    return 0;
  }

  // Interactive section
  while (isFinalize == false)
  {
    // Updating the SDL display window (headless screenshots are handled by a separate pass above)
    if (disableRender == false)
      if (currentStep % frameskip == 0) p.renderFrame(currentStep);

    // Loading step data
    p.loadStepData(currentStep);

    // Getting input string
    const auto& inputString = p.getStateInputString(currentStep);

    // Getting input index
    const auto& inputIndex = p.getStateInputIndex(currentStep);

    // Getting state hash
    const auto hash = p.getStateHash(currentStep);

    // Getting repeated step hashes (if any)
    const auto repeatedHashSteps = p.getStateRepeatedHashSteps(currentStep);

    // Checking if the current input is within the allowed inputs for this state
    const auto isInputAllowed = p.isInputAllowed(currentStep);

    // If running a command, don't print frame info, and finalize immediately after
    if (runCommand != "")
    {
      isFinalize    = true;
      showFrameInfo = false;
      isReproduce   = false;
      isUnattended  = true;
    }

    // Printing data and commands
    if (showFrameInfo)
    {
      jaffarCommon::logger::clearTerminal();

      jaffarCommon::logger::log("[J+] ----------------------------------------------------------------\n");
      jaffarCommon::logger::log("[J+] Current Step #:              %lu / %lu\n", currentStep, sequenceLength);
      jaffarCommon::logger::log("[J+] Playback:                    %s\n", isReproduce ? "Playing" : "Stopped");
      jaffarCommon::logger::log("[J+] Input:                       %s (0x%X)\n", inputString.c_str(), inputIndex);
      jaffarCommon::logger::log("[J+] On Finish:                   %s\n", isReload ? "Auto Reload" : "Stop");

      jaffarCommon::logger::log("[J+] Repeated Hash Steps:         %lu total [ ", repeatedHashStates.size());
      if (repeatedHashStates.size() < 5)
        for (const auto step : repeatedHashStates) jaffarCommon::logger::log(" %ld ", step);
      else
      {
        for (size_t i = 0; i < 5; i++) jaffarCommon::logger::log(" %ld ", repeatedHashStates[i]);
        jaffarCommon::logger::log(" ... ");
      }
      jaffarCommon::logger::log(" ] \n");

      jaffarCommon::logger::log("[J+] Not Allowed Input Steps:     %lu total [ ", notAllowedInputStates.size());
      if (notAllowedInputStates.size() < 5)
        for (const auto step : notAllowedInputStates) jaffarCommon::logger::log(" %ld ", step);
      else
      {
        for (size_t i = 0; i < 5; i++) jaffarCommon::logger::log(" %ld ", notAllowedInputStates[i]);
        jaffarCommon::logger::log(" ... ");
      }
      jaffarCommon::logger::log(" ] \n");

      jaffarCommon::logger::log("[J+] Game Name:                  '%s'\n", r.getGame()->getName().c_str());
      jaffarCommon::logger::log("[J+] Emulator Name:              '%s'\n", r.getGame()->getEmulator()->getName().c_str());
      jaffarCommon::logger::log("[J+] State Hash:                  0x%lX%lX\n", hash.first, hash.second);
      jaffarCommon::logger::log("[J+] State Repeated Hash Steps:   [ ");
      for (const auto step : repeatedHashSteps) jaffarCommon::logger::log(" %lu ", step);
      jaffarCommon::logger::log(" ] \n");
      jaffarCommon::logger::log("[J+] Is Input Allowed:            %s\n", isInputAllowed ? "True" : "False");
      jaffarCommon::logger::log("[J+] State Size:                  %lu\n", stateSize);
      jaffarCommon::logger::log("[J+] Solution File:              '%s'\n", solutionFile.c_str());
      jaffarCommon::logger::log("[J+] Sequence Length:             %lu\n", sequenceLength);
      jaffarCommon::logger::log("[J+] Frame Rate:                  %f (%u)\n", frameRate, inverseFrameRate);
      jaffarCommon::logger::log("[J+] Checkpoint:                  Level: %lu, Tolerance: %lu\n", r.getGame()->getCheckpointLevel(), r.getGame()->getCheckpointTolerance());
      jaffarCommon::logger::log("[J+] Manual Save Solution:        Active: %s, Path: '%s', Last Rule: (Current: %ld), (Prev: %ld)\n", r.getGame()->isSaveSolution() ? "Yes" : "No",
                                r.getGame()->getSaveSolutionPath().c_str(), r.getGame()->getSaveSolutionCurrentLastRuleIdx(), r.getGame()->getSaveSolutionPrevLastRuleIdx());
      p.printInfo();

      // Print General Commands
      jaffarCommon::logger::log("[J+] Commands: n: -1 m: +1 | h: -10 | j: +10 | y: -100 | u: +100 | k: -1000 | i: +1000 | s: quicksave | p: play | r: autoreload | q: quit\n");

      // Print any game-specific commands (optional)
      r.getGame()->playerPrintCommands();

      jaffarCommon::logger::refreshTerminal();
    }

    // Resetting show frame info flag
    showFrameInfo = true;

    // Specifies the command to execute next
    int command = 0;

    // If it's reproducing,
    if (isReproduce == true)
    {
      // Headless batch reproduction (no renderer, unattended) has nobody watching or typing, so skip
      // the frame-rate pacing sleep and key polling and advance at full emulation speed instead.
      if (disableRender == false || isUnattended == false)
      {
        // Introducing sleep related to the frame rate
        usleep(inverseFrameRate);

        // Get command without interrupting
        command = jaffarCommon::logger::getKeyPress();
      }

      // Advance to the next frame
      currentStep++;
    }

    // If it's not reproducing, grab command with a wait
    if (isReproduce == false && isUnattended == false) command = jaffarCommon::logger::waitForKeyPress();

    // Headless fast-forward: when unattended and not actively reproducing (and not running a one-shot
    // command), advance through the sequence as fast as possible -- no key wait, no frame-rate sleep.
    // This is what makes --unattended --exitOnEnd terminate promptly for batch/verification runs
    // (otherwise neither branch above advances and the loop spins forever).
    if (isReproduce == false && isUnattended == true && runCommand == "") currentStep++;

    // If running a command given from the console, set it now
    if (runCommand != "") command = runCommand[0];

    // Handle commands
    switch (command)
    {
      // Advance/Rewind commands
      case 'n': currentStep = currentStep - 1; break;
      case 'm': currentStep = currentStep + 1; break;
      case 'h': currentStep = currentStep - 10; break;
      case 'j': currentStep = currentStep + 10; break;
      case 'y': currentStep = currentStep - 100; break;
      case 'u': currentStep = currentStep + 100; break;
      case 'k': currentStep = currentStep - 1000; break;
      case 'i': currentStep = currentStep + 1000; break;

      case 's':
      {
        // Storing state file
        std::string saveFileName = "quicksave.state";

        std::string saveData;
        size_t      stateSize = r.getGame()->getEmulator()->getStateSize();
        saveData.resize(stateSize);
        jaffarCommon::serializer::Contiguous s(saveData.data(), stateSize);
        r.getGame()->getEmulator()->serializeState(s);
        if (jaffarCommon::file::saveStringToFile(saveData, saveFileName.c_str()) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not save state file: %s\n", saveFileName.c_str());
        jaffarCommon::logger::log("[J+] Saved state to %s\n", saveFileName.c_str());

        // Do no show frame info again after this action
        showFrameInfo = false;

        break;
      }

      // Toggles playback from current point
      case 'p': isReproduce = !isReproduce; break;

      // Toggles Auto Reload
      case 'r': isReload = !isReload; break;

      // Triggers the exit
      case 'q': isFinalize = true; break;

      // Handle any game-specific commands. If such command is executed, do not clear output
      default: showFrameInfo = r.getGame()->playerParseCommand(command) == false;
    }

    // Correct current step if requested more than possible
    if (currentStep < 0) currentStep = 0;

    // If reloading on finish, do it now
    if (currentStep > sequenceLength && isReload == true) break;

    // If exiting on finish, do it now
    if (currentStep > sequenceLength && isExitOnEnd == true) break;

    // If not reloading on finish, simply stop
    if (currentStep > sequenceLength)
    {
      currentStep = sequenceLength;
      isReproduce = false;
    }
  }

  // If requested, print a stable summary of the final (end-of-sequence) state. This is the
  // machine-checkable oracle for headless reproduction tests: the hash is deterministic, so the
  // same config+solution must always produce the same value here.
  if (printFinalState)
  {
    const auto        finalHash       = p.getStateHash(sequenceLength);
    const auto        stateType       = r.getGame()->getStateType();
    const std::string stateTypeString = stateType == jaffarPlus::Game::stateType_t::win ? "Win" : (stateType == jaffarPlus::Game::stateType_t::fail ? "Fail" : "Normal");
    jaffarCommon::logger::log("[J+] Final Step:                  %ld\n", sequenceLength);
    jaffarCommon::logger::log("[J+] Final State Type:            %s\n", stateTypeString.c_str());
    // First step (inputs applied) at which the solution reaches a win / fail state. Useful to spot a
    // movie that wins before its end (wasted trailing inputs) or fails midway. "none" if it never does.
    const auto        firstWinStep        = p.getFirstWinStep();
    const auto        firstFailStep       = p.getFirstFailStep();
    const std::string firstWinStepString  = firstWinStep < 0 ? "none" : std::to_string(firstWinStep);
    const std::string firstFailStepString = firstFailStep < 0 ? "none" : std::to_string(firstFailStep);
    jaffarCommon::logger::log("[J+] First Win Step:              %s\n", firstWinStepString.c_str());
    jaffarCommon::logger::log("[J+] First Fail Step:             %s\n", firstFailStepString.c_str());
    jaffarCommon::logger::log("[J+] Final State Hash:            0x%lX%lX\n", finalHash.first, finalHash.second);
    // Solution-quality counts: inputs the engine would not have considered at their frame, and
    // states the engine would have pruned as duplicates. Both are 0 for a clean engine-found path.
    jaffarCommon::logger::log("[J+] Not Allowed Input Count:     %lu\n", notAllowedInputStates.size());
    jaffarCommon::logger::log("[J+] Repeated State Count:        %lu\n", repeatedHashStates.size());
  }

  // returning false on exit to trigger the finalization
  if (isFinalize) return false;

  // Otherwise, keep looping
  return true;
}

/**
 * @brief Entry point for the jaffar-player (jaffar-tester) executable.
 *
 * @details Parses the command-line arguments (required @c configFile and @c solutionFile, plus the
 * @c --reproduce, @c --reload, @c --exitOnEnd, @c --unattended, @c --disableRender, @c --frameskip,
 * @c --initialSequence, @c --runCommand, @c --screenshotDir, @c --screenshotSteps and
 * @c --printFinalState flags), seeding the file-scope playback switches from them. Loads and parses
 * the JSON config, optionally overrides the emulator's "Initial Sequence File Path", forces the
 * runner's frameskip rate to 0, then builds and initializes a @ref jaffarPlus::Runner. If rendering
 * is enabled it initializes video output and rendering. Serializes the runner's initial state, then
 * repeatedly invokes @ref mainCycle; when a cycle requests another pass it sleeps briefly and
 * restores the saved initial state. Finalizes video output and the terminal on exit.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on normal exit (implicit).
 */
int main(int argc, char* argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("jaffar-tester", "2.0.0");

  program.add_argument("configFile").help("path to the Jaffar configuration script (.jaffar) file to run.").required();
  program.add_argument("solutionFile").help("path to the solution sequence file (.sol) to reproduce.").required();
  program.add_argument("--reproduce").help("Starts playing from the start").default_value(false).implicit_value(true);
  program.add_argument("--reload").help("Reloads the solution after reaching the end").default_value(false).implicit_value(true);
  program.add_argument("--exitOnEnd").help("Exits the program upon reaching the last step").default_value(false).implicit_value(true);
  program.add_argument("--unattended").help("Indicates the player not to print the interactive prompt nor wait for inputs").default_value(false).implicit_value(true);
  program.add_argument("--disableRender").help("Do not render game window.").default_value(false).implicit_value(true);
  program.add_argument("--frameskip").help("How many frames to skip between renderings.").default_value(std::string("1"));
  program.add_argument("--initialSequence").help("Overrides the solution file to use as initial sequence to play before starting.").default_value(std::string(""));
  program.add_argument("--runCommand").help("Specifies a command to run and then exit").default_value(std::string(""));
  program.add_argument("--screenshotDir").help("Directory to write per-frame screenshots (BMP) into (requires rendering enabled).").default_value(std::string(""));
  program.add_argument("--screenshotSteps").help("Comma-separated list of steps to screenshot (empty = every rendered frame).").default_value(std::string(""));
  program.add_argument("--printFinalState")
      .help("Prints a stable summary (step, state type, state hash) of the final state on exit, for headless verification.")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--dumpHashes")
      .help("Writes the per-step game-state hash for every step to the given file (for cross-emulator divergence checks).")
      .default_value(std::string(""));
  program.add_argument("--dumpRam")
      .help("Writes the full low work-RAM (LRAM) for every step to the given file as flat binary (for byte-level cross-emulator diffs).")
      .default_value(std::string(""));
  program.add_argument("--dumpVram")
      .help("Writes the full video RAM (VRAM) for every step to the given file as flat binary (for locating VRAM in a foreign savestate).")
      .default_value(std::string(""));
  program.add_argument("--dumpPolls")
      .help("Writes the per-step joypad read count to the given file (one 'step\\tcount' line per step; 0 = lag frame, input not polled).")
      .default_value(std::string(""));
  program.add_argument("--dumpReward")
      .help("Writes the per-step game reward (one value per line) to the given file (for use as a 'Reference Reward Floor' trace).")
      .default_value(std::string(""));
  program.add_argument("--dumpTrace")
      .help("Writes the game's per-step trace line (Game::getTraceLine) to the given file (for use as a game 'Trace File Path' / trace magnet).")
      .default_value(std::string(""));
  program.add_argument("--dumpRacer").help("Replay solution, dump frame,gx,gy,spd + full 234-byte racer struct per frame to CSV, then exit.").default_value(std::string(""));
  program.add_argument("--pokeRAM")
      .help("Poke work-RAM bytes into the post-initial-sequence state before replay, e.g. --pokeRAM \"0x8010=0x42,0x804e=0x00\" (mechanic experiments).")
      .default_value(std::string(""));
  program.add_argument("--terrainDrive")
      .help("Branch off the replayed solution, steer a real car into neighboring cells, log true effect (slowdown/hole/jump) per cell to CSV, then exit.")
      .default_value(std::string(""));
  program.add_argument("--terrainSweep")
      .help(
          "Probe terrain at a grid of player positions: \"out.csv:x0,x1,y0,y1,step\". Reloads state, pokes 0x80A6/0x80A8, steps, reads 0xA649/0xA648/0xA66C. Writes CSV and exits.")
      .default_value(std::string(""));
  program.add_argument("--saveStateStep").help("Step at which to save the emulator state (used with --saveStateFile), then exit.").default_value(std::string(""));
  program.add_argument("--saveStateFile")
      .help("File to write the emulator's full state at --saveStateStep to (load as Emulator 'Initial State File Path').")
      .default_value(std::string(""));

  // Try to parse arguments
  try
  {
    program.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err)
  {
    JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str());
  }

  // Parsing config file
  const std::string configFile = program.get<std::string>("configFile");

  // Parsin solution file
  const std::string solutionFile = program.get<std::string>("solutionFile");

  // Getting reload flag
  bool doReload = program.get<bool>("--reload");

  // Getting reproduce flag
  bool reproduceStart = program.get<bool>("--reproduce");

  // Getting disablerender flag
  bool disableRender = program.get<bool>("--disableRender");

  // Parsing --pokeRAM "0xADDR=VAL,0xADDR=VAL" (addresses/values accept 0x hex or decimal)
  {
    const auto pokeSpec = program.get<std::string>("--pokeRAM");
    if (pokeSpec.empty() == false)
      for (const auto& tok : jaffarCommon::string::split(pokeSpec, ','))
      {
        const auto eq = tok.find('=');
        if (eq == std::string::npos) JAFFAR_THROW_LOGIC("Bad --pokeRAM token (need addr=val): '%s'", tok.c_str());
        const uint32_t addr = std::stoul(tok.substr(0, eq), nullptr, 0);
        const uint32_t val  = std::stoul(tok.substr(eq + 1), nullptr, 0);
        pokeRam.emplace_back((uint32_t)addr, (uint8_t)val);
      }
  }

  // Getting screenshot options
  terrainDrivePath = program.get<std::string>("--terrainDrive");
  terrainSweepSpec = program.get<std::string>("--terrainSweep");
  screenshotDir    = program.get<std::string>("--screenshotDir");
  {
    const auto stepsStr = program.get<std::string>("--screenshotSteps");
    if (stepsStr.empty() == false)
      for (const auto& tok : jaffarCommon::string::split(stepsStr, ','))
        if (tok.empty() == false) screenshotSteps.insert(parseUInt(tok, "--screenshotSteps"));
  }

  // Getting exitOnEnd flag
  bool exitOnEnd = program.get<bool>("--exitOnEnd");

  // Getting unattended flag
  bool unattended = program.get<bool>("--unattended");

  // Getting frameskip
  frameskip = parseUInt(program.get<std::string>("--frameskip"), "--frameskip");

  // Getting frameskip
  const std::string initialSequence = program.get<std::string>("--initialSequence");

  // Getting command to run (if any)
  runCommand = program.get<std::string>("--runCommand");

  // Getting the print-final-state flag
  printFinalState = program.get<bool>("--printFinalState");

  // Getting the per-step hash dump path (if any)
  dumpHashesPath = program.get<std::string>("--dumpHashes");

  // Getting the per-step RAM dump path (if any)
  dumpRamPath       = program.get<std::string>("--dumpRam");
  dumpVramPath      = program.get<std::string>("--dumpVram");
  dumpPollsPath     = program.get<std::string>("--dumpPolls");
  dumpRewardPath    = program.get<std::string>("--dumpReward");
  dumpTracePath     = program.get<std::string>("--dumpTrace");
  dumpRacerPath     = program.get<std::string>("--dumpRacer");
  saveStateStepStr  = program.get<std::string>("--saveStateStep");
  saveStateFilePath = program.get<std::string>("--saveStateFile");

  // Initializing terminal
  jaffarCommon::logger::initializeTerminal();

  // Setting initial reproduction values
  isReload     = doReload;
  isReproduce  = reproduceStart;
  isExitOnEnd  = exitOnEnd;
  isUnattended = unattended;

  // If config file defined, read it now
  std::string configFileString;
  if (jaffarCommon::file::loadStringFromFile(configFileString, configFile) == false)
    JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from Jaffar config file: %s\n", configFile.c_str());

  // Parsing configuration file
  nlohmann::json config;
  try
  {
    config = nlohmann::json::parse(configFileString);
  }
  catch (const std::exception& err)
  {
    JAFFAR_THROW_LOGIC("[ERROR] Parsing configuration file %s. Details:\n%s\n", configFile.c_str(), err.what());
  }

  // Getting component configurations
  auto emulatorConfig = jaffarCommon::json::getObject(config, "Emulator Configuration");
  auto gameConfig     = jaffarCommon::json::getObject(config, "Game Configuration");
  auto runnerConfig   = jaffarCommon::json::getObject(config, "Runner Configuration");

  // Overriding initial solution file, if provided
  if (initialSequence != "") emulatorConfig["Initial Sequence File Path"] = initialSequence;

  // Disabling frameskip, if enabled
  runnerConfig["Frameskip"]["Rate"] = 0;

  // Creating runner from the configuration
  auto r = jaffarPlus::Runner::getRunner(emulatorConfig, gameConfig, runnerConfig);

  // Initializing runner
  r->initialize();

  // Enabling rendering, if required
  if (screenshotDir.empty() == false)
  {
    // Headless screenshot mode: render frames into the emulator's framebuffer without opening an SDL window.
    r->getGame()->getEmulator()->enableHeadlessRendering();
  }
  else if (disableRender == false)
  {
    r->getGame()->getEmulator()->initializeVideoOutput();
    r->getGame()->getEmulator()->enableRendering();
  }

  // Getting game state size
  const auto stateSize = r->getStateSize();

  // Storage for the initial state
  std::string initialState;
  initialState.resize(stateSize);

  // Getting initial state
  jaffarCommon::serializer::Contiguous s(initialState.data(), initialState.size());
  r->serializeState(s);

  // ALSO capture a FULL-fidelity emulator snapshot (all state properties enabled, including any
  // configured-out blocks like NES nametables). The reduced runner state above restores game RAM
  // but leaves excluded video state (e.g. NTAB) at its end-of-movie contents, so every reload
  // cycle displayed the PREVIOUS cycle's background. Restoring this full snapshot first fixes it.
  auto*       fullEmu = r->getGame()->getEmulator();
  std::string fullInitialState;
  {
    fullEmu->enableAllStateProperties();
    const size_t fullSize = fullEmu->getStateSize();
    fullInitialState.resize(fullSize);
    jaffarCommon::serializer::Contiguous fs(fullInitialState.data(), fullInitialState.size());
    fullEmu->serializeState(fs);
    fullEmu->reapplyDisabledStateProperties();
  }

  // Running main cycle
  bool continueRunning = true;
  while (continueRunning == true)
  {
    // Running main cycle
    continueRunning = mainCycle(*r, solutionFile, disableRender);

    // If the exit-on-end flag is set, then do not repeat reproduction
    if (exitOnEnd == true) break;

    // If the playback repeats, then sleep and restore the initial state
    if (continueRunning == true)
    {
      // If repeating, then wait a bit before repeating to prevent fast repetition of short movies
      sleep(1);

      // Reloading the initial state (captured at step 0); the step counter is not in the stream, so reset
      // it here before deserializing (the player advances it itself as it replays).
      // Full-fidelity emulator restore FIRST (brings back excluded blocks like nametables), then
      // the reduced runner restore for game/runner bookkeeping.
      {
        fullEmu->enableAllStateProperties();
        jaffarCommon::deserializer::Contiguous fd(fullInitialState.data(), fullInitialState.size());
        fullEmu->deserializeState(fd);
        fullEmu->reapplyDisabledStateProperties();
      }
      r->setStepCount(0);
      jaffarCommon::deserializer::Contiguous d(initialState.data(), initialState.size());
      r->deserializeState(d);
    }
  }

  // If redering was enabled, finish it now
  if (disableRender == false) r->getGame()->getEmulator()->finalizeVideoOutput();

  // Ending ncurses window
  jaffarCommon::logger::finalizeTerminal();
}