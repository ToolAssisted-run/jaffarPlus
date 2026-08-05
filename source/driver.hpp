#pragma once

/**
 * @file driver.hpp
 * @brief Top-level orchestration of a Jaffar search run: drives the engine's step loop, tracks the
 *        best/worst states and win states, and periodically saves intermediate solutions.
 */

#include "engine.hpp"
#include "game.hpp"
#include "runner.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/string.hpp>
#include <limits>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief Owns and runs the engine's step loop and reports how the run ended.
 *
 * @details The driver reads its configuration sections (Driver/Emulator/Game/Runner/Engine),
 * constructs a @ref Runner and an @ref Engine, and repeatedly advances the engine one step at a time
 * until a termination condition is reached (a win state, exhaustion of states, or the maximum step
 * count). On each step it refreshes the tracked best and worst states, optionally stores a manually
 * requested solution, and prints status. A separate thread periodically saves the best and worst
 * solutions to file when intermediate result saving is enabled.
 */
class Driver final

{
public:
  /// @brief Reason the run loop terminated, returned by @ref run.
  enum exitReason_t
  {
    winStateFound = 0, ///< Found a win state

    winCollectionFull = 7, ///< Win-state collection reached its Max Files cap

    outOfStates = 1, ///< Engine ran out of states

    maximumStepReached = 2, ///< Maximum step reached

    bestBelowReference = 3, ///< The best state's reward fell below the reference reward floor at this step

    inputHistoryNearCapacity = 4, ///< The shared input-history trie neared/hit its hard memory ceiling

    referenceBelowWorst = 5, ///< The reference reward fell below the worst kept state (reference evicted from the frontier)

    exceededReferenceFrames = 6 ///< The run reached the reference's frame count without winning (can no longer beat it)
  };

  /**
   * @brief Constructs the driver and its engine/runner from the parsed configuration.
   *
   * Reads the "Driver Configuration" section (end-on-first-win flag, max steps, intermediate result
   * settings), derives a job identifier from the current system time, and builds the @ref Runner and
   * @ref Engine from their respective configuration sections.
   * @param configFilePath Path to the config file, kept for reference and reporting.
   * @param config         Parsed configuration object containing all component sections.
   */
  Driver(const std::string& configFilePath, const nlohmann::json& config) : _configFilePath(configFilePath)
  {
    // Getting job identifier from the system timer
    auto currentTime = std::chrono::system_clock::now();
    _jobId           = std::chrono::duration_cast<std::chrono::seconds>(currentTime.time_since_epoch()).count();

    // Mutable working copy of the root config; each recognized key is consumed (popped) below, so any
    // leftover key at the end is unrecognized (a typo or unsupported option) and is reported by name.
    auto configRemaining = config;

    // Getting driver configuration
    auto driverConfig = jaffarCommon::json::popObject(configRemaining, "Driver Configuration");

    // Getting end win delay config
    // Stop condition on wins: "Stop Frames After First Win" = N keeps the search expanding N more
    // steps past the FIRST win before stopping. 0 = stop immediately; absent/-1 = never stop on
    // wins (run to Max Steps). Pairs naturally with "Win State Collection" below to harvest every
    // distinct ending inside the window (e.g. board reseeding: distinct frozen RNG = distinct
    // next layout). The legacy boolean "End On First Win State" is accepted as an alias
    // (true -> 0, false -> -1) so archived configs keep their exact semantics.
    _stopFramesAfterFirstWin = -1;
    if (driverConfig.contains("End On First Win State")) _stopFramesAfterFirstWin = jaffarCommon::json::popBoolean(driverConfig, "End On First Win State") ? 0 : -1;
    if (driverConfig.contains("Stop Frames After First Win"))
      _stopFramesAfterFirstWin = (ssize_t)jaffarCommon::json::popNumber<size_t>(driverConfig, "Stop Frames After First Win");
    // Optional: save EVERY win state's solution, deduplicated by a configurable tuple of game
    // properties. Driver-owned config; the engine performs the capture at the moment each win
    // state is produced (the worker runner still holds it live).
    if (driverConfig.contains("Win State Collection"))
    {
      auto                     wc      = jaffarCommon::json::popObject(driverConfig, "Win State Collection");
      auto                     enabled = jaffarCommon::json::popBoolean(wc, "Enabled");
      std::vector<std::string> props;
      for (const auto& n : jaffarCommon::json::getArray<nlohmann::json>(wc, "Dedup Properties")) props.push_back(n.get<std::string>());
      wc.erase("Dedup Properties");
      auto   prefix = jaffarCommon::json::popString(wc, "Path Prefix");
      size_t maxF   = wc.contains("Max Files") ? jaffarCommon::json::popNumber<size_t>(wc, "Max Files") : 1000;
      jaffarCommon::json::checkEmpty(wc, "Driver Configuration > Win State Collection");
      // The engine does not exist yet at parse time -- stash and apply after construction below
      if (enabled)
      {
        _winCollectProps  = props;
        _winCollectPrefix = prefix;
        _winCollectMax    = maxF;
        _winCollectArm    = true;
      }
    }

    // Getting maximum number of steps (zero is not established)
    _maxSteps = jaffarCommon::json::popNumber<uint32_t>(driverConfig, "Max Steps");

    // For testing purposes, the maximum number of steps can be overriden via environment variables
    if (auto* value = std::getenv("JAFFAR_DRIVER_OVERRIDE_DRIVER_MAX_STEP")) _maxSteps = std::stoul(value);

    // Getting intermediate result configuration
    auto saveIntermediateResultsJs     = jaffarCommon::json::popObject(driverConfig, "Save Intermediate Results");
    _saveIntermediateResultsEnabled    = jaffarCommon::json::popBoolean(saveIntermediateResultsJs, "Enabled");
    _saveIntermediateFrequency         = jaffarCommon::json::popNumber<float>(saveIntermediateResultsJs, "Frequency (s)");
    _saveIntermediateBestSolutionPath  = jaffarCommon::json::popString(saveIntermediateResultsJs, "Best Solution Path");
    _saveIntermediateWorstSolutionPath = jaffarCommon::json::popString(saveIntermediateResultsJs, "Worst Solution Path");
    jaffarCommon::json::checkEmpty(saveIntermediateResultsJs, "Driver Configuration > Save Intermediate Results");

    // Optional reference reward floor: cancel the whole job if the BEST state's reward falls below a
    // per-step reference reward trace (beyond tolerance). This is purely a driver-level stopping criterion
    // and diagnostic -- it does NOT prune states (recoverable slower-then-faster lines are kept); it only
    // detects the first step at which the leading edge falls behind the reference (e.g. a reference TAS) and
    // stops, so the run isn't ground on once it can no longer keep pace. Reward must be monotone-comparable.
    _referenceFloorEnabled       = false;
    _referenceFloorTolerance     = 0.0f;
    _cancelIfReferenceBelowWorst = false;
    if (driverConfig.contains("Reference Reward Floor"))
    {
      auto refJs               = jaffarCommon::json::popObject(driverConfig, "Reference Reward Floor");
      _referenceFloorEnabled   = jaffarCommon::json::popBoolean(refJs, "Enabled");
      _referenceFloorTolerance = jaffarCommon::json::popNumber<float>(refJs, "Tolerance");
      // Optional (default 0): compare best against the reference's reward G steps EARLIER. With a
      // discrete-jump reward (waypoint boxes), a zero reward tolerance cancels at every box boundary
      // on a frames-scale timing skew even when the frontier is at pace; a step grace bounds the
      // allowed slack in TIME uniformly (<= G frames behind schedule anywhere) instead of in reward
      // units (which a jump makes step-dependent).
      _referenceFloorStepGrace = refJs.contains("Step Grace") ? jaffarCommon::json::popNumber<uint32_t>(refJs, "Step Grace") : 0;
      // The reference reward trace can be supplied two ways:
      //  - "Solution File": a reference solution (a .sol input sequence). Its per-step floor-reward trace is
      //    computed at engine init by replaying it through THIS run's own runner/game (see initialize()), so the
      //    floor is always consistent with the live reward function -- no external jaffar-player --dumpReward pass.
      //  - "Path": a precomputed file with one reward value per line (legacy; kept for backward compatibility).
      _referenceSolutionPath = refJs.contains("Solution File") ? jaffarCommon::json::popString(refJs, "Solution File") : std::string();
      const auto refPath     = refJs.contains("Path") ? jaffarCommon::json::popString(refJs, "Path") : std::string();
      // Optional (default false): also cancel the instant the reference reward falls BELOW the worst kept
      // state -- i.e. the reference solution has been evicted from the frontier, so the winning line can no
      // longer be reached no matter how good "best" looks. A stricter companion to the best-below check.
      _cancelIfReferenceBelowWorst = refJs.contains("Cancel If Reference Below Worst") ? jaffarCommon::json::popBoolean(refJs, "Cancel If Reference Below Worst") : false;
      // Optional margin for the below-worst check (default 0): cancel only when ref + margin < worst. Set this
      // to the Reference Pinning bonus so the guard does not fire while the pinned reference state (whose
      // stored reward includes that bonus) is in fact still alive in the frontier.
      _referenceBelowWorstMargin = refJs.contains("Below Worst Margin") ? jaffarCommon::json::popNumber<float>(refJs, "Below Worst Margin") : 0.0f;
      jaffarCommon::json::checkEmpty(refJs, "Driver Configuration > Reference Reward Floor");
      if (_referenceFloorEnabled && _referenceSolutionPath.empty() && refPath.empty())
        JAFFAR_THROW_LOGIC("[ERROR] 'Reference Reward Floor' is enabled but neither 'Solution File' nor 'Path' was provided\n");
      // Legacy precomputed-trace file: load it now. (The "Solution File" form defers to initialize(), once the
      // runner/game exist to replay it.) Files are runtime artifacts, so skip the read under --dryRun
      // (JAFFAR_IS_DRY_RUN): validate the config shape but don't depend on the cwd-relative file being present.
      _referenceTracePath = refPath;
      if (_referenceFloorEnabled && _referenceSolutionPath.empty() && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr)
      {
        std::ifstream f(refPath);
        if (f.good() == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not open 'Reference Reward Floor' > 'Path': '%s'\n", refPath.c_str());
        float v;
        while (f >> v) _referenceReward.push_back(v);
        jaffarCommon::logger::log("[J+] Reference reward floor enabled: %lu steps loaded, tolerance %.4f\n", _referenceReward.size(), _referenceFloorTolerance);
      }
    }

    jaffarCommon::json::checkEmpty(driverConfig, "Driver Configuration");

    // Getting component configurations (consumed from the root so the root check below can flag strays)
    auto emulatorConfig = jaffarCommon::json::popObject(configRemaining, "Emulator Configuration");
    auto gameConfig     = jaffarCommon::json::popObject(configRemaining, "Game Configuration");
    auto runnerConfig   = jaffarCommon::json::popObject(configRemaining, "Runner Configuration");
    auto engineConfig   = jaffarCommon::json::popObject(configRemaining, "Engine Configuration");

    // Any remaining top-level key is unrecognized
    jaffarCommon::json::checkEmpty(configRemaining, "configuration root");

    // Creating runner from the configuration
    _runner = jaffarPlus::Runner::getRunner(emulatorConfig, gameConfig, runnerConfig);

    // Creating engine from the configuration
    _engine = std::make_unique<Engine>(emulatorConfig, gameConfig, runnerConfig, engineConfig);
    if (_winCollectArm) _engine->setWinStateCollection(_winCollectProps, _winCollectPrefix, _winCollectMax);
  }

  /// @brief Destroys the driver.
  ~Driver() {}

  /**
   * @brief Resets the execution back to the starting point.
   *
   * Clears the step and win-state counters, resets the best/worst reward trackers, initializes the
   * runner and engine, and allocates storage for the current best and worst states.
   */
  void initialize()
  {
    // Resetting step counter
    _currentStep = 0;

    // Resetting win state counter
    _winStatesFound = 0;

    // Resetting best states reward
    _bestWinStateReward   = -std::numeric_limits<float>::infinity();
    _bestStateReward      = -std::numeric_limits<float>::infinity();
    _bestStateFloorReward = -std::numeric_limits<float>::infinity();

    // Resetting worst state reward
    _worstStateReward = std::numeric_limits<float>::infinity();

    // Initializing runner
    _runner->initialize();

    // Initializing engine
    _engine->initialize();

    // Allocating space for the current best and worst states. These are standalone snapshots outside the
    // NUMA slabs, so they hold the FULL self-contained state ([hot]+[history]), not just the hot slot.
    _stateSize = _engine->getFullStateSize();
    _bestStateStorage.resize(_stateSize);
    _worstStateStorage.resize(_stateSize);

    // If a reference SOLUTION file was provided for the reward floor, compute its per-step floor-reward trace
    // now by replaying it through our own runner/game -- guaranteeing the floor is measured with the exact same
    // reward function the search uses. Depth 0 (the initial post-initial-sequence state, i.e. the search root)
    // is recorded first, then one reward per applied input, so _referenceReward[N] is the reference's floor
    // reward at search depth N -- aligned with _bestStateFloorReward, which the run loop compares against it.
    const bool referencePruneRequested = _engine->isReferencePruneRequested();
    if ((_referenceFloorEnabled || referencePruneRequested) && _referenceSolutionPath.empty() == false && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr)
    {
      // Load the reference solution (.sol files are null-separated input strings, as the player reads them)
      std::string solutionString;
      if (jaffarCommon::file::loadStringFromFile(solutionString, _referenceSolutionPath) == false)
        JAFFAR_THROW_RUNTIME("[ERROR] Could not open 'Reference Reward Floor' > 'Solution File': '%s'\n", _referenceSolutionPath.c_str());
      const auto referenceInputs = jaffarCommon::string::split(solutionString, '\0');

      // Snapshot the runner's initial (post-initial-sequence) state so we can restore it after the replay. The
      // engine already captured the root from this same state above, so the replay only borrows the runner.
      std::vector<uint8_t> initialStateStorage(_runner->getStateSize());
      {
        jaffarCommon::serializer::Contiguous s(initialStateStorage.data(), initialStateStorage.size());
        _runner->serializeState(s);
      }

      auto* game     = _runner->getGame();
      auto* emulator = game->getEmulator();
      _referenceReward.clear();

      // Per-depth serialized reference states, captured for EXACT pinning: with a quantized (coarse)
      // dedup hash, hash equality no longer identifies the reference lineage, so the engine byte-compares
      // pin candidates against these exact states (hash match = cheap pre-filter only).
      std::vector<std::vector<uint8_t>> referenceStates;
      const auto                        captureState = [&]()
      {
        // Capture in CANONICAL (round-tripped) form: serialize, load back, serialize again. Search-side
        // states are always post-round-trip, and a few emulator header bytes normalize on load -- a raw
        // linear-replay capture differs in those and would fail the exact byte comparison.
        std::vector<uint8_t> st(_runner->getStateSize());
        {
          jaffarCommon::serializer::Contiguous s(st.data(), st.size());
          _runner->serializeState(s);
        }
        {
          jaffarCommon::deserializer::Contiguous d(st.data(), st.size());
          _runner->deserializeState(d);
        }
        {
          jaffarCommon::serializer::Contiguous s(st.data(), st.size());
          _runner->serializeState(s);
        }
        referenceStates.push_back(std::move(st));
      };

      // Depth 0: evaluate the initial state exactly as the engine does when it seeds the root
      game->evaluateRules();
      game->updateGameStateType();
      game->updateReward();
      _referenceReward.push_back(game->getFloorReward());
      captureState();

      // Replay each input, recording the floor reward at each resulting depth. Each step first
      // round-trips the runner (serialize + load) so the replay follows the exact same
      // load -> advance -> serialize path as the search; otherwise path-dependent residue bytes
      // (CPU scratch, audio buffer tails) differ and exact pin verification can never match.
      std::vector<uint8_t> rt(_runner->getStateSize());
      for (const auto& inputString : referenceInputs)
      {
        if (inputString.empty()) continue;
        {
          jaffarCommon::serializer::Contiguous s(rt.data(), rt.size());
          _runner->serializeState(s);
        }
        {
          jaffarCommon::deserializer::Contiguous d(rt.data(), rt.size());
          _runner->deserializeState(d);
        }
        _runner->advanceState(emulator->registerInput(inputString));
        game->evaluateRules();
        game->updateGameStateType();
        game->updateReward();
        _referenceReward.push_back(game->getFloorReward());
        captureState();
      }
      std::vector<std::string> refInputStrings;
      for (const auto& s : referenceInputs)
        if (s.empty() == false) refInputStrings.push_back(s);
      _engine->setReferenceStates(std::move(referenceStates), refInputStrings);

      // Restore the runner to its initial state (and reset its step counter) for the search
      _runner->setStepCount(0);
      {
        jaffarCommon::deserializer::Contiguous d(initialStateStorage.data(), initialStateStorage.size());
        _runner->deserializeState(d);
      }

      jaffarCommon::logger::log("[J+] Reference reward floor computed from solution '%s': %lu steps, tolerance %.4f\n", _referenceSolutionPath.c_str(), _referenceReward.size(),
                                _referenceFloorTolerance);
    }

    // Arm reference pruning in the engine now that the trace exists. The trace source is the
    // driver's "Reference Reward Floor" (Solution File replayed above, or the legacy "Path" file --
    // loaded here if the floor itself is disabled and only the engine-side prune needs it).
    if (referencePruneRequested)
    {
      if (_referenceReward.empty() && _referenceTracePath.empty() == false && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr)
      {
        std::ifstream f(_referenceTracePath);
        if (f.good() == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not open 'Reference Reward Floor' > 'Path': '%s'\n", _referenceTracePath.c_str());
        float v;
        while (f >> v) _referenceReward.push_back(v);
      }
      if (_referenceReward.empty() && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr)
        JAFFAR_THROW_LOGIC("[ERROR] 'Reference Reward Prune' is enabled but the 'Reference Reward Floor' section provides no trace ('Solution File' or 'Path')\n");
      if (_referenceReward.empty() == false)
      {
        _engine->setReferencePruneTrace(_referenceReward);
        jaffarCommon::logger::log("[J+] Reference pruning armed: %lu-step trace supplied to the engine\n", _referenceReward.size());
      }
    }
  }

  /**
   * @brief Runs the engine's step loop until a termination condition is met.
   *
   * Optionally launches the intermediate result saver thread, then advances the engine one step per
   * iteration, updating the tracked best/worst states, storing any manually requested solution, and
   * printing status each step. The loop ends on the first win state (when so configured), when the
   * engine runs out of states, or when the maximum step count is reached. On exit it joins the saver
   * thread, saves the best state information, and prints a final report.
   * @return The @ref exitReason_t describing why the loop stopped.
   */
  int run()
  {
    // Internal flag to indicate we are still running
    _hasFinished = false;

    // If using ncurses, initialize terminal now
    jaffarCommon::logger::initializeTerminal();

    // Storage for the exit
    exitReason_t exitReason;

    // Starting intermediate result saving thread
    std::thread intermediateResultSaverThread;
    if (_saveIntermediateResultsEnabled == true) intermediateResultSaverThread = std::thread([this]() { intermediateResultSaveLoop(); });

    // Running engine until a termination point
    while (true)
    {
      // If found winning state, report it now
      if (_engine->isWinCollectionFull())
      {
        exitReason = exitReason_t::winCollectionFull;
        break;
      }
      if (_engine->getWinStatesFound() > 0 && _firstWinStep < 0) _firstWinStep = (ssize_t)_currentStep;
      if (_stopFramesAfterFirstWin >= 0 && _firstWinStep >= 0 && (ssize_t)_currentStep >= _firstWinStep + _stopFramesAfterFirstWin)
      {
        exitReason = exitReason_t::winStateFound;
        break;
      }

      // If ran out of states, finish now
      if (_engine->getStateCount() == 0)
      {
        exitReason = _engine->getWinStatesFound() > 0 ? exitReason_t::winStateFound : exitReason_t::outOfStates;
        break;
      }

      // If maximum step established and reached, finish now
      if (_maxSteps > 0 && _currentStep >= _maxSteps)
      {
        if (_winStatesFound > 0) exitReason = exitReason_t::winStateFound;
        if (_winStatesFound == 0) exitReason = exitReason_t::maximumStepReached;
        break;
      }

      // Updating best and worst states
      updateBestState();
      updateWorstState();

      // Reference frame-count cap: the win state is checked at the top of the loop, so if the floor is enabled and
      // we have reached the reference's frame count here without winning, the run has spent the reference's entire
      // frame budget -- it can no longer even match, let alone beat, the reference. Cancel.
      if (_referenceFloorEnabled && _referenceReward.size() > 0 && _currentStep >= _referenceReward.size())
      {
        jaffarCommon::logger::log("[J+] Reached reference frame count (%lu frames) at step %lu without winning -- can no longer beat the reference, cancelling.\n",
                                  _referenceReward.size(), _currentStep);
        exitReason = exitReason_t::exceededReferenceFrames;
        break;
      }

      // Reference reward floor: if the best leading edge has fallen below the reference at this step, the run
      // can no longer keep pace with the reference -- cancel now (purely a stop signal; nothing was pruned).
      // With Step Grace G, the comparison point is the reference G steps earlier (bounded time slack).
      const size_t floorRefStep = (_currentStep > _referenceFloorStepGrace) ? _currentStep - _referenceFloorStepGrace : 0;
      if (_referenceFloorEnabled && _currentStep < _referenceReward.size() && _bestStateFloorReward < _referenceReward[floorRefStep] - _referenceFloorTolerance)
      {
        jaffarCommon::logger::log("[J+] Best (%.6f) fell below reference floor (%.6f, tol %.4f, grace %u steps) at step %lu by %.6f -- cancelling.\n", _bestStateFloorReward,
                                  _referenceReward[floorRefStep], _referenceFloorTolerance, _referenceFloorStepGrace, _currentStep,
                                  _referenceReward[floorRefStep] - _referenceFloorTolerance - _bestStateFloorReward);
        exitReason = exitReason_t::bestBelowReference;
        break;
      }

      // Reference-below-worst check (opt-in): the reference reward has dropped below the WORST kept state,
      // meaning the (un-pinned, un-bonused) reference would be evicted from the frontier. This is a WARNING
      // only -- the pinned reference lineage may still survive via its pinning bonus, and a frontier composed
      // entirely of states ahead of the reference is a healthy sign, not a failure. No cancellation.
      if (_cancelIfReferenceBelowWorst && _referenceFloorEnabled && _currentStep < _referenceReward.size() &&
          _referenceReward[_currentStep] + _referenceBelowWorstMargin < _worstStateReward)
      {
        jaffarCommon::logger::log(
            "[J+] WARNING: reference (%.6f) below worst kept state (%.6f) at step %lu -- un-pinned reference would be evicted (pinned lineage survives only via its pin bonus).\n",
            _referenceReward[_currentStep], _worstStateReward, _currentStep);
      }

      // Input-history backing guard: the "Trie" strategy's shared node pool grows ~ live-states x depth
      // toward a hard ceiling (getInputHistoryMaxMemoryBytes). Stop GRACEFULLY at a high-water mark -- or, as
      // a backstop, if a worker already latched the pool exhausted -- so the search saves its best result and
      // exits cleanly, instead of a worker hitting the ceiling mid-step and terminating the whole process.
      // No-op for None/Raw (ceiling 0): their history lives in the StateDb slot, already bounded.
      {
        const size_t ihCeiling = _engine->getInputHistoryMaxMemoryBytes();
        if (ihCeiling > 0)
        {
          const size_t ihNow     = _engine->getInputHistoryApproxMemoryBytes();
          const bool   exhausted = _engine->isInputHistoryExhausted();
          if (exhausted || ihNow >= (size_t)((double)ihCeiling * _inputHistoryCapacityWatermark))
          {
            const double GB = 1024.0 * 1024.0 * 1024.0;
            jaffarCommon::logger::log("[J+] Input-history trie at %.1f / %.1f GB (%.0f%% of its hard ceiling)%s at step %lu -- stopping "
                                      "gracefully. The Trie node pool grows ~ live-states x depth and cannot be enlarged past RAM; switch "
                                      "Store Input History Type to \"Raw\" (bounded by 'State Database/Max Size (Mb)'), or lower the State "
                                      "DB size so fewer live states slow the trie's growth.\n",
                                      (double)ihNow / GB, (double)ihCeiling / GB, 100.0 * (double)ihNow / (double)ihCeiling, exhausted ? " (pool exhausted)" : "", _currentStep);
            exitReason = exitReason_t::inputHistoryNearCapacity;
            break;
          }
        }
      }

      // Storing manually saved solution, if required
      storeManualSaveSolution();

      // Printing information
      printInfo();

      // Running engine step
      _engine->runStep();

      // Summing amount of win states found
      _winStatesFound = _engine->getWinStatesFound();

      // Increasing step counter
      _currentStep++;
    }

    // Setting finalized flag
    _hasFinished = true;

    // Waiting for saver thread
    if (_saveIntermediateResultsEnabled == true) intermediateResultSaverThread.join();

    // If using ncurses, terminate terminal now
    jaffarCommon::logger::finalizeTerminal();

    // Updating and storing best states
    updateBestState();
    saveBestStateInformation();

    // Also flush any pending "Trigger Save Solution" request: with End-On-First-Win the loop breaks at
    // the top before the winning step's storeManualSaveSolution() runs, so a win rule's save would be
    // lost. The engine's manual-save request persists across steps, so writing it here captures it.
    storeManualSaveSolution();

    // Final report
    printInfo();

    // Otherwise return the reason why we stopped
    return exitReason;
  }

  /**
   * @brief Saves a solution explicitly requested by the engine, if any.
   *
   * Queries the engine for a manual-save request; when one has a non-empty path, it loads that state
   * into the runner and writes the runner's input history to the requested file.
   */
  void storeManualSaveSolution()
  {
    auto manualSaveSolution = _engine->getManualSaveSolution();

    if (manualSaveSolution.path != "")
    {
      // Loading the saved state into the runner (its depth was recorded at capture; set it before
      // deserializing so the trie can rebuild and the solution renders to the right length).
      _runner->setStepCount(manualSaveSolution.stepCount);
      _engine->getStateDb()->loadStateIntoRunner(*_runner, manualSaveSolution.stateData);

      // Saving manually stored solution
      std::string solutionData = _runner->getInputHistoryString();
      jaffarCommon::file::saveStringToFile(solutionData, manualSaveSolution.path);
    }
  }

  /**
   * @brief Writes the current best solution to file, under the mutex.
   *
   * When a best solution path is configured, saves the best solution both under the plain configured
   * name and under a copy suffixed with the job id and current step number.
   */
  void saveBestStateInformation()
  {
    // Making sure the main thread is not currently writing
    _updateIntermediateResultMutex.lock();

    // Saving best solution and state
    std::string jobSuffix  = std::string(".") + std::to_string(_jobId);
    std::string stepSuffix = std::string(".") + std::to_string(_currentStep);

    // Saving files with standard name
    if (_saveIntermediateBestSolutionPath != "") jaffarCommon::file::saveStringToFile(_bestSolutionStorage, _saveIntermediateBestSolutionPath);

    // Saving files with a job suffix and step number
    if (_saveIntermediateBestSolutionPath != "") jaffarCommon::file::saveStringToFile(_bestSolutionStorage, _saveIntermediateBestSolutionPath + jobSuffix + stepSuffix);

    // Making sure the main thread is not currently writing
    _updateIntermediateResultMutex.unlock();
  }

  /**
   * @brief Writes the current worst solution to file, under the mutex.
   *
   * When a worst solution path is configured, saves the worst solution both under the plain
   * configured name and under a copy suffixed with the job id.
   */
  void saveWorstStateInformation()
  {
    // Making sure the main thread is not currently writing
    _updateIntermediateResultMutex.lock();

    // Saving best solution and state
    std::string jobSuffix = std::string(".") + std::to_string(_jobId);

    // Saving best solution and state
    if (_saveIntermediateWorstSolutionPath != "") jaffarCommon::file::saveStringToFile(_worstSolutionStorage, _saveIntermediateWorstSolutionPath);

    // Saving best solution and state
    if (_saveIntermediateWorstSolutionPath != "") jaffarCommon::file::saveStringToFile(_worstSolutionStorage, _saveIntermediateWorstSolutionPath + jobSuffix);

    // Making sure the main thread is not currently writing
    _updateIntermediateResultMutex.unlock();
  }

  /**
   * @brief Background loop that periodically saves best and worst solutions to file.
   *
   * Runs while the driver has not finished, waking every 100 ms; once the configured frequency has
   * elapsed since the last save (and at least one step has been performed), it saves the best and
   * worst state information and resets the timer.
   */
  void intermediateResultSaveLoop()
  {
    // Timer for saving to file
    auto lastSaveTime = jaffarCommon::timing::now();

    // Run loop while the driver is still running
    while (_hasFinished == false)
    {
      // Sleeping for 100ms intervals to prevent excessive overheads
      usleep(100000);

      // Getting time elapsed since last save
      auto currentTime              = jaffarCommon::timing::now();
      auto timeElapsedSinceLastSave = jaffarCommon::timing::timeDeltaSeconds(currentTime, lastSaveTime);

      // Checking if we need to save best state
      if (timeElapsedSinceLastSave > _saveIntermediateFrequency && _currentStep > 1)
      {
        // Saving worst and best state information
        saveBestStateInformation();
        saveWorstStateInformation();

        // Resetting timer
        lastSaveTime = jaffarCommon::timing::now();
      }
    }
  }

  /**
   * @brief Refreshes the tracked worst state, its solution, and its reward.
   *
   * Does nothing when the state database is empty. Otherwise, under the mutex, copies the engine's
   * worst state into local storage, loads it into the runner, records the runner's input history as
   * the worst solution, and updates the worst-state reward.
   */
  void updateWorstState()
  {
    // If no states in database, there is nothing to update
    if (_engine->getStateDb()->getStateCount() == 0) return;

    // Making sure the intermediate result thread is not currently reading
    _updateIntermediateResultMutex.lock();

    // Getting worst state so far
    auto worstState = _engine->getStateDb()->getWorstState();

    // Saving worst state into the storage (gather hot slab slot + its cold history into the full buffer)
    _engine->getStateDb()->captureSlotToBuffer(worstState, _worstStateStorage.data());

    // The worst state belongs to the current frontier, so its depth is the current search step (the step
    // counter is not stored per-state). Set it before loading so the solution renders to the right length.
    _runner->setSearchStep(_currentStep);
    _engine->getStateDb()->loadStateIntoRunner(*_runner, _worstStateStorage.data());

    // Saving worst solution into storage
    _worstSolutionStorage = _runner->getInputHistoryString();

    // Updating worst state reward
    _worstStateReward = _runner->getGame()->getReward();

    // Making sure the intermediate result thread is not currently reading
    _updateIntermediateResultMutex.unlock();
  }

  /**
   * @brief Refreshes the tracked best state, its solution, and its reward.
   *
   * Does nothing when the state database is empty and no win states have been found. Otherwise, under
   * the mutex: if no win state has been found yet it uses the engine's current best state; if a win
   * state was found this step and improves on the best win reward so far, it adopts that win state.
   * It then loads the best state into the runner, updates the best-state reward, and records the
   * runner's input history as the best solution.
   */
  void updateBestState()
  {
    // If no states in database and no win states, there is nothing to update
    if (_engine->getStateDb()->getStateCount() == 0 && _winStatesFound == 0) return;

    // Making sure the intermediate result thread is not currently reading
    _updateIntermediateResultMutex.lock();

    // If we haven't found any winning state, simply use the currently best state
    if (_winStatesFound == 0)
    {
      // Getting best state so far
      auto bestState = _engine->getStateDb()->getBestState();

      // Saving best state into the storage (gather hot slab slot + its cold history into the full buffer)
      if (bestState != nullptr) _engine->getStateDb()->captureSlotToBuffer(bestState, _bestStateStorage.data());
    }

    // If we have found a winning state in this step that improves on the current best, save it now
    if (_engine->getWinStatesFound() > 0)
    {
      // Getting best win state (best reward) for the current step
      auto winStateEntry = _engine->getStepBestWinState();

      // If the reward if better than the current best, then make it the new best state
      if (winStateEntry.reward > _bestWinStateReward)
      {
        // Saving new best
        _bestWinStateReward = winStateEntry.reward;

        // Saving win state into the storage (and remembering its depth for solution rendering)
        memcpy(_bestStateStorage.data(), winStateEntry.stateData, _stateSize);
        _bestWinStateStepCount = winStateEntry.stepCount;
      }
    }

    // Set the runner's step counter to the best state's depth before loading (the counter is not stored
    // per-state): a win's depth was recorded at capture; an ordinary best belongs to the current frontier.
    if (_winStatesFound > 0)
      _runner->setStepCount(_bestWinStateStepCount);
    else
      _runner->setSearchStep(_currentStep);
    _bestStateStepCount = _runner->getStepCount(); // remembered so the printInfo reload uses the same depth
    _engine->getStateDb()->loadStateIntoRunner(*_runner, _bestStateStorage.data());

    // Updating best state reward
    _bestStateReward      = _runner->getGame()->getReward();
    _bestStateFloorReward = _runner->getGame()->getFloorReward(); // un-biased position for the Reference Reward Floor (decoupled from the magnet)

    // Storing best solution
    _bestSolutionStorage = _runner->getInputHistoryString();

    // Making sure the intermediate result thread is not currently reading
    _updateIntermediateResultMutex.unlock();
  }

  /**
   * @brief Prints the current state of execution to the logger.
   *
   * Logs the job id, script file, emulator and game names, current/max step, and the best/worst (or
   * win/worst) reward summary, followed by the engine's information and, after loading the best state
   * into the runner, the runner/game/emulator information for that best state.
   */
  void printInfo()
  {
    // If using ncurses, clear terminal before printing the information for this step
    jaffarCommon::logger::clearTerminal();

    // Printing information
    jaffarCommon::logger::log("[J+] Job Id:                                      %lu\n", _jobId);
    jaffarCommon::logger::log("[J+] Script File:                                 '%s'\n", _configFilePath.c_str());
    jaffarCommon::logger::log("[J+] Emulator Name:                               '%s'\n", _runner->getGame()->getEmulator()->getName().c_str());
    jaffarCommon::logger::log("[J+] Game Name:                                   '%s'\n", _runner->getGame()->getName().c_str());
    jaffarCommon::logger::log("[J+] Current Step #:                              %lu", _currentStep);
    if (_maxSteps > 0) jaffarCommon::logger::log(" (Max: %lu)", _maxSteps);
    jaffarCommon::logger::log("\n");

    if (_winStatesFound == 0)
      jaffarCommon::logger::log("[J+] Current Reward (Best / Worst):               %.6f / %.6f (Diff: %.6f)\n", _bestStateReward, _worstStateReward,
                                _bestStateReward - _worstStateReward);

    if (_winStatesFound > 0)
      jaffarCommon::logger::log("[J+] Current Reward (Win / Worst):                %.6f / %.6f (Diff: %.6f)\n", _bestStateReward, _worstStateReward,
                                _bestStateReward - _worstStateReward);

    // When a reference trace exists (floor cancel and/or engine-side pruning), show the reference
    // reward at this step and how the best compares to it (positive = best ahead of the reference,
    // negative = best behind), for easy human review.
    if (_referenceReward.empty() == false)
    {
      if (_currentStep < _referenceReward.size())
        jaffarCommon::logger::log("[J+] Reference Reward (Ref / Best-Ref):           %.6f (Best-Ref %+.6f, floor tol %.4f) [step %lu / %lu ref steps]\n",
                                  _referenceReward[_currentStep], _bestStateFloorReward - _referenceReward[_currentStep], _referenceFloorTolerance, _currentStep,
                                  _referenceReward.size());
      else
        jaffarCommon::logger::log("[J+] Reference Reward (Ref / Best-Ref):           (none: step %lu beyond reference trace of %lu steps)\n", _currentStep,
                                  _referenceReward.size());
    }

    // Printing engine information
    jaffarCommon::logger::log("[J+] Engine Information: \n");
    _engine->printInfo();

    // Loading best state into runner (same depth updateBestState() used, so the trie rebuilds correctly)
    _runner->setStepCount(_bestStateStepCount);
    _engine->getStateDb()->loadStateIntoRunner(*_runner, _bestStateStorage.data());

    // Printing best state information to screen
    jaffarCommon::logger::log("[J+] Runner Information (Best State): \n");
    _runner->printInfo();
    jaffarCommon::logger::log("[J+] Game Information (Best State): \n");
    _runner->getGame()->printInfo();
    jaffarCommon::logger::log("[J+] Emulator Information (Best State): \n");
    _runner->getGame()->getEmulator()->printInfo();

    // Division rule to separate different steps
    jaffarCommon::logger::log("[J+] --------------------------------------------------------------\n");

    // If using ncurses, refresh terminal now
    jaffarCommon::logger::refreshTerminal();
  }

  /**
   * @brief Factory that constructs a driver from configuration.
   * @param configFilePath Path to the config file, kept for reference and reporting.
   * @param config         Parsed configuration object containing all component sections.
   * @return A unique pointer to the newly constructed @ref Driver.
   */
  static std::unique_ptr<Driver> getDriver(const std::string& configFilePath, const nlohmann::json& config)
  {
    // Creating new engine
    auto d = std::make_unique<Driver>(configFilePath, config);

    // Returning engine
    return d;
  }

  /** @brief Returns the current step counter. */
  size_t getCurrentStep() { return _currentStep; }

private:
  const std::string _configFilePath; ///< Path to the config file, kept for reference.

  std::unique_ptr<Engine> _engine; ///< Pointer to the internal Jaffar engine.

  std::unique_ptr<Runner> _runner; ///< Runner used for printing information and saving partial results.

  size_t _jobId; ///< Job identifier (derived from system time) distinguishing intermediate values between jobs.

  size_t _maxSteps; ///< Maximum number of steps (zero = not established).

  size_t _currentStep; ///< Counter for the number of steps performed; the initial state counts as step zero.

  ssize_t _stopFramesAfterFirstWin = -1; ///< Stop N steps after the first win (0 = immediately; -1 = never stop on wins)

  ssize_t _firstWinStep = -1; ///< Step at which the first win was found

  bool _winCollectArm = false; ///< Whether "Win State Collection" was enabled in the config (applied to the engine post-construction)

  std::vector<std::string> _winCollectProps; ///< Win-state collection: dedup property names

  std::string _winCollectPrefix; ///< Win-state collection: output path prefix

  size_t _winCollectMax = 1000; ///< Win-state collection: Max Files cap

  size_t _winStatesFound; ///< Total number of win states found so far.

  float  _bestWinStateReward;        ///< Reward for the best win state found so far.
  size_t _bestWinStateStepCount = 0; ///< Depth of the best win state (recorded at capture; the count is not stored per-state).
  size_t _bestStateStepCount    = 0; ///< Depth of the current best state, set by updateBestState() and reused by the printInfo reload.

  float _bestStateReward;      ///< Ranking reward (magnet-biased) for the best state found so far; drives eviction/display.
  float _bestStateFloorReward; ///< Un-biased progress (position) reward of the best state; used for the Reference Reward Floor comparison.

  float _worstStateReward; ///< Reward for the worst state found so far.

  bool     _referenceFloorEnabled;       ///< Whether the reference reward floor cancel is active.
  float    _referenceFloorTolerance;     ///< Allowed shortfall of best below the reference per step.
  uint32_t _referenceFloorStepGrace = 0; ///< Compare best against the reference this many steps earlier (bounded time slack for jumpy rewards).
  bool     _cancelIfReferenceBelowWorst; ///< Opt-in: cancel when the reference reward drops below the worst kept state (reference evicted).
  float    _referenceBelowWorstMargin;   ///< Margin added to the reference before the below-worst comparison (typically the pinning bonus).

  std::string        _referenceTracePath;    ///< Legacy precomputed-trace file path (loaded on demand if only the engine-side prune needs it)
  std::vector<float> _referenceReward;       ///< Per-step reference reward floor (index = step).
  std::string        _referenceSolutionPath; ///< Optional reference solution (.sol) replayed at init to build @ref _referenceReward.

  /// @brief Fraction of the input-history trie's hard ceiling at which the run stops gracefully (high-water
  /// mark). One step's growth (~ live-states nodes) is far below the remaining headroom at this level, so the
  /// next per-step check always fires before a worker hits the actual ceiling. Not a config key (sane fixed
  /// default); the real lever is Store Input History Type ("Raw" vs "Trie") and the State DB size.
  double _inputHistoryCapacityWatermark = 0.95;

  std::string _bestStateStorage; ///< Storage for the current best (win or otherwise) state.

  std::string _worstStateStorage; ///< Storage for the current worst (win or otherwise) state.

  std::string _bestSolutionStorage; ///< Storage for the current best solution's input history.

  std::string _worstSolutionStorage; ///< Storage for the current worst solution's input history.

  size_t _stateSize; ///< Storage size of a runner state.

  __volatile__ bool _hasFinished; ///< Internal flag indicating the driver has finished.

  /////////////// Intermediate Result Storage

  bool _saveIntermediateResultsEnabled; ///< Whether to store intermediate results at all.

  float _saveIntermediateFrequency; ///< Minimum interval, in seconds, between intermediate result saves.

  std::string _saveIntermediateBestSolutionPath; ///< Path to store the best solution found so far.

  std::string _saveIntermediateWorstSolutionPath; ///< Path to store the worst solution found so far.

  std::mutex _updateIntermediateResultMutex; ///< Guards intermediate result storage between the main and saver threads.
};

} // namespace jaffarPlus