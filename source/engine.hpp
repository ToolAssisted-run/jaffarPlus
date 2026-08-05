#pragma once

/**
 * @file engine.hpp
 * @brief Parallel breadth-first search engine that expands game states step by step, deduplicating
 *        via a hash database and storing survivors in a state database while tracking win states,
 *        checkpoints, and per-operation timing statistics.
 */

#include "game.hpp"
#include "hashDb.hpp"
#include "numa.hpp"
#include "runner.hpp"
#include "stateDb.hpp"
#include <algorithm>
#include <emulatorList.hpp>
#include <fstream>
#include <gameList.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/parallel.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/timing.hpp>
#include <mutex>
#include <set>

// Fine-grained per-operation timing for the engine hot loop. Each timed region costs two
// clock_gettime calls; multiplied across millions of states processed per step this is a real
// overhead for a light emulator (~25% with QuickerSDLPoP), while being negligible under a heavy one
// (QuickerNES is ~92% emulation). It is therefore compiled out unless built with
// -DdetailedProfiling=true. When disabled, JAFFAR_PROF_DECL declares no variable and JAFFAR_PROF_ACC
// is a no-op, so there is neither timing overhead nor an unused-variable warning. The coarse
// per-step timers (step wall time, throughput, serial DB-advance stages) are always kept.
#ifdef JAFFARPLUS_DETAILED_PROFILING
/// @brief Declares a timestamp variable @p var holding the current time (detailed-profiling build).
#define JAFFAR_PROF_DECL(var) const auto var = jaffarCommon::timing::now()
/// @brief Accumulates the microseconds elapsed since timestamp @p var into @p field.
#define JAFFAR_PROF_ACC(field, var) field += jaffarCommon::timing::timeDeltaMicroseconds(jaffarCommon::timing::now(), var)
#else
#define JAFFAR_PROF_DECL(var) ((void)0)
#define JAFFAR_PROF_ACC(field, var) ((void)0)
#endif

namespace jaffarPlus
{

/**
 * @brief Parallel state-space search engine.
 *
 * @details Owns one @ref Runner per worker thread, a @ref StateDb holding the current and next
 * step's states, and an optional @ref HashDb for detecting repeated states. Each call to
 * @ref runStep expands every base state in the current database by trying its allowed and candidate
 * inputs, evaluating game rules on the resulting states, and pushing the surviving normal states
 * into the next database while recording win states, checkpoints, drop counts, and per-operation
 * timing.
 */
class Engine final
{
public:
  /**
   * @brief Constructs the engine, building one runner per worker thread and the state/hash databases.
   * @param emulatorConfig Configuration object passed to each runner's emulator.
   * @param gameConfig     Configuration object passed to each runner's game.
   * @param runnerConfig   Configuration object passed to each runner.
   * @param engineConfig   Engine configuration containing the "State Database" and "Hash Database" objects.
   * @throws A logic error if the configured worker thread count is zero.
   */
  Engine(const nlohmann::json& emulatorConfig, const nlohmann::json& gameConfig, const nlohmann::json& runnerConfig, const nlohmann::json& engineConfig)
  {
    // Initializing NUMA and threading subsystems
    initializeNUMA();

    // Sanity check
    if (_threadCount == 0) JAFFAR_THROW_LOGIC("The number of worker threads must be at least one. Provided: %lu\n", _threadCount);

    // Printing initial information
    jaffarCommon::logger::log("[J+] Using %lu worker threads.\n", _threadCount);

    // Creating storage for the runnners (one per thread)
    _runners.resize(_threadCount);

    // Creating runners, one per thread
    JAFFAR_PARALLEL
    {
      // Creating runner from the configuration
      auto r = jaffarPlus::Runner::getRunner(emulatorConfig, gameConfig, runnerConfig);

      // Storing runner. Index by the (dense) OpenMP thread id, NOT sched_getcpu(): the latter is only
      // equal to the thread id when threads are pinned 1:1 (OMP_PROC_BIND), and otherwise leaves
      // _runners[0] null -> the *_runners[0] use below (and workerFunction, which already uses the
      // thread id) would dereference a null runner. This bites CI runners that don't pin threads.
      _runners[jaffarCommon::parallel::getThreadId()] = std::move(r);
    }

    // Grabbing a runner to do continue build the state databases
    auto& r = *_runners[0];

    // Mutable copy so unrecognized Engine keys can be flagged after the known ones are consumed
    auto engineConfigRemaining = engineConfig;

    // Creating State database
    auto stateDatabaseJs = jaffarCommon::json::popObject(engineConfigRemaining, "State Database");
    _stateDb             = std::make_unique<jaffarPlus::StateDb>(r, stateDatabaseJs);

    // Creating hash database
    auto hashDbConfig = jaffarCommon::json::popObject(engineConfigRemaining, "Hash Database");
    _hashDbEnabled    = jaffarCommon::json::getBoolean(hashDbConfig, "Enabled");
    if (_hashDbEnabled == true) _hashDb = std::make_unique<jaffarPlus::HashDb>(hashDbConfig);

    // Base-state pull batch size (per-worker queue-pull granularity). Optional: an explicit value
    // always wins; otherwise 0 here means "auto-tune", resolved in initialize() from the measured
    // per-state cost (light cores -> larger batch to amortize the queue lock; heavy/variable cores
    // -> small batch for load balance).
    _baseStateBatch = engineConfigRemaining.contains("Base State Batch Size") ? jaffarCommon::json::popNumber<size_t>(engineConfigRemaining, "Base State Batch Size") : 0;
    if (_baseStateBatch > BASE_STATE_BATCH_MAX) _baseStateBatch = BASE_STATE_BATCH_MAX;

    // Log verbosity: "Full" (default, everything) or "Compact" (per-step engine block collapses to
    // step timing, checkpoint, state-flow counters, and one-line DB summaries; the game module's own
    // print is unaffected -- it is what live observers actually watch). Labels that external
    // tooling greps ("Win States:", "Checkpoint (Level/Tolerance/Cutoff)") are kept verbatim.
    _compactLog = false;
    if (engineConfigRemaining.contains("Log Verbosity"))
    {
      const auto v = jaffarCommon::json::popString(engineConfigRemaining, "Log Verbosity");
      if (v != "Full" && v != "Compact") JAFFAR_THROW_LOGIC("[ERROR] 'Log Verbosity' must be 'Full' or 'Compact' (got '%s')\n", v.c_str());
      _compactLog = (v == "Compact");
    }

    // Optional reference pinning: keep a known (reference) solution's lineage anchored in the frontier so
    // it is never evicted, and reward being AHEAD of it. We load the reference's per-depth state hashes;
    // when a newly generated state's hash matches the reference hash at depth d (its own depth), or at
    // d+1..d+Lookahead (i.e. it has reached the reference's near-future state early -- it is 1..L frames
    // ahead), we add a large reward bonus, escalating with how far ahead it is. The base match (k=0)
    // guarantees the reference path is never the worst state (never evicted); the k>0 matches steer the
    // search to stay slightly ahead. Purely additive to the DB-ordering reward; the reference-floor
    // comparison uses the unbiased floor reward and is unaffected.
    _refPinEnabled = false;
    if (engineConfigRemaining.contains("Reference Pinning"))
    {
      auto pinJs            = jaffarCommon::json::popObject(engineConfigRemaining, "Reference Pinning");
      _refPinEnabled        = jaffarCommon::json::popBoolean(pinJs, "Enabled");
      _refPinBonus          = jaffarCommon::json::popNumber<float>(pinJs, "Bonus");
      _refPinLookahead      = jaffarCommon::json::popNumber<size_t>(pinJs, "Lookahead");
      _refPinLookaheadBonus = jaffarCommon::json::popNumber<float>(pinJs, "Lookahead Bonus");
      const auto pinPath    = jaffarCommon::json::popString(pinJs, "Path");
      jaffarCommon::json::checkEmpty(pinJs, "Engine Configuration > Reference Pinning");
      if (_refPinEnabled && std::getenv("JAFFAR_IS_DRY_RUN") == nullptr)
      {
        std::ifstream f(pinPath);
        if (f.good() == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not open 'Reference Pinning' > 'Path': '%s'\n", pinPath.c_str());
        // Format matches jaffar-player --dumpHashes: "<step>\t<016X first><016X second>" per line.
        size_t      step;
        std::string hex;
        while (f >> step >> hex)
        {
          if (hex.size() != 32) continue;
          jaffarCommon::hash::hash_t h;
          h.first  = std::stoull(hex.substr(0, 16), nullptr, 16);
          h.second = std::stoull(hex.substr(16, 16), nullptr, 16);
          if (step >= _refPinHashes.size()) _refPinHashes.resize(step + 1);
          _refPinHashes[step] = h;
        }
        _refPinnedAtDepth = std::make_unique<std::atomic<uint8_t>[]>(_refPinHashes.size());
        for (size_t i = 0; i < _refPinHashes.size(); i++) _refPinnedAtDepth[i].store(0, std::memory_order_relaxed);
        jaffarCommon::logger::log("[J+] Reference pinning enabled: %lu hashes loaded, bonus %.1f, lookahead %lu (+%.1f/frame ahead)\n", _refPinHashes.size(), _refPinBonus,
                                  _refPinLookahead, _refPinLookaheadBonus);
      }
    }

    // Reference-reward pruning (greedy polish mode): drop every produced non-win state whose floor
    // reward falls below the reference trace at its depth beyond Tolerance. The trace itself comes
    // from the driver's "Reference Reward Floor" source (Solution File or Path) -- the driver arms
    // it via setReferencePruneTrace() once the trace exists; until then the prune is inert.
    if (engineConfigRemaining.contains("Reference Reward Prune"))
    {
      auto pruneJs       = jaffarCommon::json::popObject(engineConfigRemaining, "Reference Reward Prune");
      _refPruneRequested = jaffarCommon::json::popBoolean(pruneJs, "Enabled");
      _refPruneTolerance = jaffarCommon::json::popNumber<float>(pruneJs, "Tolerance");
      jaffarCommon::json::checkEmpty(pruneJs, "Engine Configuration > Reference Reward Prune");
    }

    // Any remaining Engine key is unrecognized
    jaffarCommon::json::checkEmpty(engineConfigRemaining, "Engine Configuration");

    // Reserving storage for timing information
    _threadStepTime.resize(_threadCount);

    // Reserving per-thread accumulators (cache-line aligned to avoid false sharing).
    // These collect all hot-loop timing/counter increments without atomics, and are
    // reduced into the shared totals once per step (see runStep).
    _threadAccumulators.resize(_threadCount);
  };

  /**
   * @brief Resets execution back to step zero and clears all databases and counters.
   *
   * @details Re-initializes all runners, the state database, and (when enabled) the hash database;
   * evaluates rules, determines the state type, and computes the reward for the initial state; pushes
   * that initial state into the database; allocates the best-win and manual-save state buffers; and
   * inserts the initial state's hash into the hash database.
   */
  void initialize()
  {
    // Initializing state counters
    _totalBaseStatesProcessed = 0;
    _totalNewStatesProcessed  = 0;

    // Initializing cumulative timing
    _runnerStateAdvanceAverageCumulativeTime = 0;
    _runnerStateLoadAverageCumulativeTime    = 0;
    _runnerStateSaveAverageCumulativeTime    = 0;
    _calculateHashAverageCumulativeTime      = 0;
    _checkHashAverageCumulativeTime          = 0;
    _ruleCheckingAverageCumulativeTime       = 0;
    _getFreeStateAverageCumulativeTime       = 0;
    _returnFreeStateAverageCumulativeTime    = 0;
    _calculateRewardAverageCumulativeTime    = 0;
    _advanceHashDbAverageCumulativeTime      = 0;
    _getAllowedInputsAverageCumulativeTime   = 0;
    _advanceStateDbAverageCumulativeTime     = 0;
    _popBaseStateDbAverageCumulativeTime     = 0;

    // Resetting total running time
    _totalRunningTime = 0;

    // Resetting state counts
    _droppedStatesNoStorage           = 0;
    _droppedStatesFailedSerialization = 0;
    _droppedStatesCheckpoint          = 0;
    _droppedStatesBelowReference      = 0;
    _repeatedStates                   = 0;
    _failedStates                     = 0;
    _winStates                        = 0;
    _normalStates                     = 0;

    // Resetting checkpoint counters
    _checkpointLevel     = 0;
    _checkpointTolerance = 0;
    _checkpointCutoff    = 0;

    // Resetting counter for the current step
    _currentStep = 0;

    // Resetting last active manually solution save rule id
    _manualSaveSolutionActiveLastRuleId = -1;

    // Create the one shared input-history backing (e.g. the trie, for the "Trie" strategy; null for
    // raw/none) and inject it into every worker runner BEFORE they initialize, so all workers share it.
    // It is sized with one free-list shard per worker thread plus one for the driver's intermediate-result
    // thread (contention-free alloc/free). The StateDb reaches the strategy via the reference runner.
    _inputHistoryBacking = inputHistory::createSharedBacking(_runners[0]->getInputHistoryConfig(), _threadCount + 1);

    // Initializing runners, one per thread
    JAFFAR_PARALLEL
    {
      // Creating thread's own runner (index by OpenMP thread id, consistent with construction/workerFunction)
      auto& r = _runners[jaffarCommon::parallel::getThreadId()];

      // Share the one backing (prefix sharing) and give each worker its own free-list shard (its thread id).
      r->setInputHistoryBacking(_inputHistoryBacking, (uint32_t)jaffarCommon::parallel::getThreadId(), _threadCount + 1);
      r->initialize();
    }

    // Combined RAM guard: the per-NUMA check inside StateDb::initialize() validates ONLY the state
    // DB, and the hash DB's peak footprint is (Max Store Size x Max Store Count) -- NOT just Max Store
    // Size. Summing both against total free RAM here catches a silent overcommit that the OS would
    // otherwise resolve by OOM-killing the process mid-run (hours in). Done before any DB allocates.
    {
      const size_t stateBudget = _stateDb->getMaxBudgetBytes();
      const size_t hashBudget  = (_hashDbEnabled == true) ? _hashDb->getMaxBudgetBytes() : 0;
      // Shared input-history trie: an uncounted structure that grows ~ live-states x depth up to its hard
      // node cap (~384 GiB for the Trie strategy on this build). Reserve its ceiling. (0 for None/Raw.)
      const size_t historyBound = inputHistory::getSharedBackingMaxMemoryBytes(_inputHistoryBacking);
      // The state DB is a fixed pool (1x; full slots are scavenged from the current step, never doubled).
      // The hash DB phmap grows by doubling, so during a rehash the old table and the new 2x table briefly
      // coexist -- reserve ~2x the hash budget for that transient. (Empirically a 270 GB state+hash config
      // peaked at ~520 GB RSS = state + ~2*hash + trie, then OOM-killed mid-run.)
      const size_t hashPeak    = hashBudget * 2;
      const size_t totalBudget = stateBudget + hashPeak + historyBound;
      size_t       freeRam     = 0;
      for (int i = 0; i < _numaCount; i++)
      {
        long long nodeFree = 0;
        numa_node_size64(i, &nodeFree);
        freeRam += (size_t)nodeFree;
      }
      const size_t usable = (size_t)((double)freeRam * 0.90); // 10% headroom: emulators, OS, fragmentation
      if (totalBudget > usable)
      {
        const double GB = 1024.0 * 1024.0 * 1024.0;
        JAFFAR_THROW_RUNTIME("Configured database budget exceeds available memory:\n"
                             "  State DB ('Max Size (Mb)', fixed pool)                       = %.1f GB\n"
                             "  Hash DB peak (2 x 'Max Store Size' x 'Max Store Count')      = %.1f GB\n"
                             "  Input-history trie (hard node-storage ceiling)               = %.1f GB\n"
                             "  TOTAL                                                        = %.1f GB\n"
                             "  Usable (90%% of %.1f GB free RAM)                             = %.1f GB\n"
                             "Reduce 'State Database/Max Size (Mb)' and/or 'Hash Database/Max Store Size (Mb)'.\n"
                             "NOTE: the hash DB phmap doubles on growth, so a rehash briefly holds the old + new (2x) table; the\n"
                             "Trie input-history is a separate structure that grows up to ~384 GiB. Both are otherwise uncounted.\n",
                             (double)stateBudget / GB, (double)hashPeak / GB, (double)historyBound / GB, (double)totalBudget / GB, (double)freeRam / GB, (double)usable / GB);
      }
    }

    // Initializing State Db
    _stateDb->initialize();

    // Initializing hash database
    if (_hashDbEnabled == true) _hashDb->initialize();

    // Grabbing a runner to do continue initialization
    auto& r = *_runners[0];

    // Auto-tune the base-state pull batch (when "Base State Batch Size" was not set in the config).
    // We time a burst of state advances on the (just-loaded) initial state -- which is representative
    // of the core's per-state cost -- then choose a batch so each batch is ~TARGET_BATCH_NS of work:
    // enough to amortize the per-NUMA queue lock on cheap cores (large batch) while keeping a small
    // batch on heavy/variable cores so end-of-step load imbalance stays low. The measurement state is
    // saved and restored so the search still starts from the exact initial state. Batch size never
    // affects which states are explored, only the work distribution.
    if (_baseStateBatch == 0)
    {
      const size_t      stateSize = r.getStateSize();
      std::vector<char> scratch(stateSize);
      {
        jaffarCommon::serializer::Contiguous s(scratch.data(), stateSize);
        r.serializeState(s);
      }

      const auto                   allowedInputs = r.getAllowedInputs();
      const InputSet::inputIndex_t calInput      = allowedInputs.empty() ? (InputSet::inputIndex_t)0 : allowedInputs[0];

      const size_t CAL_FRAMES = 200;
      const auto   tCal0      = jaffarCommon::timing::now();
      for (size_t i = 0; i < CAL_FRAMES; i++) r.advanceState(calInput);
      const size_t perStateNs = jaffarCommon::timing::timeDeltaNanoseconds(jaffarCommon::timing::now(), tCal0) / CAL_FRAMES;

      // Restore the exact initial state
      {
        jaffarCommon::deserializer::Contiguous d(scratch.data(), stateSize);
        r.deserializeState(d);
      }

      // ~200us of work per batch: SDLPoP (~10us/state) -> ~16, Genesis (~600us/state) -> 1.
      constexpr size_t TARGET_BATCH_NS = 200000;
      size_t           b               = (perStateNs > 0) ? ((TARGET_BATCH_NS + perStateNs / 2) / perStateNs) : BASE_STATE_BATCH_MAX;
      if (b < 1) b = 1;
      if (b > BASE_STATE_BATCH_MAX) b = BASE_STATE_BATCH_MAX;
      _baseStateBatch = b;
      jaffarCommon::logger::log("[J+] Auto-tuned base-state batch size:            %lu (measured %.1f us/state)\n", _baseStateBatch, (double)perStateNs / 1000.0);
    }
    if (_baseStateBatch < 1) _baseStateBatch = 1;

    // Evaluate game rules on the initial state
    r.getGame()->evaluateRules();

    // Determining new game state type
    r.getGame()->updateGameStateType();

    // Running game-specific rule actions
    r.getGame()->runGameSpecificRuleActions();

    // Updating game reward
    r.getGame()->updateReward();

    // Getting reward for the initial state
    const auto reward = r.getGame()->getReward();

    // Getting a free state data pointer to store the state into (serial init path -> thread 0)
    auto stateData = _stateDb->getFreeState(0);

    // Pushing initial state to the next state database
    _stateDb->pushState(reward, r, stateData);

    // Advancing the step in the state database
    _stateDb->advanceStep();

    // Getting memory for the reference state
    _stateSizeInDatabase = _stateDb->getStateSizeInDatabase();

    // Standalone snapshots hold the FULL self-contained state ([hot][history]); the DB slot is hot-only.
    _fullStateSize = _stateDb->getFullStateSize();

    // Allocating memory for the best win state
    _stepBestWinState.stateData = malloc(_fullStateSize);

    // Allocating memory for manual state saving
    _manualSaveSolution.stateData = malloc(_fullStateSize);

    // Getting hash from first state
    const auto hash = r.computeHash();

    // Adding it to the hash DB
    if (_hashDbEnabled == true) _hashDb->insertHash(hash);
  }

  /**
   * @brief Runs a single search step: expands all current base states in parallel and advances the databases.
   *
   * @details Resets the per-thread accumulators and step-best/manual-save state, runs @ref workerFunction
   * across all threads, reduces the per-thread timers and counters into the shared totals, advances the hash
   * and state databases, updates the manual-save last-rule-id tracking, and recomputes per-step and cumulative
   * timing, throughput, and state-count statistics before incrementing the current step.
   */
  void runStep()
  {
    // Computing step time
    const auto tStep = jaffarCommon::timing::now();

    // Clearing step timing for the serially-measured stages (the rest are reduced
    // from the per-thread accumulators after the parallel region)
    _advanceHashDbThreadRawTime  = 0;
    _advanceStateDbThreadRawTime = 0;

    // Resetting per-thread accumulators for this step
    for (ssize_t i = 0; i < _threadCount; i++) _threadAccumulators[i].reset();

    // Clearing win state reward
    _stepBestWinState.reward = -std::numeric_limits<float>::infinity();

    // Clearing manually saved state
    _manualSaveSolution.reward      = -std::numeric_limits<float>::infinity();
    _manualSaveSolution.path        = "";
    _manualSaveSolution.lastRuleIdx = -1;

    // (Manual solution storing) Resetting last active rule id flag
    _manualSaveSolutionUpdatedLastRuleId = false;

    // Performing one computation step in parallel
    JAFFAR_PARALLEL
    workerFunction();

    // Reducing per-thread accumulators into the shared totals (serial, ~threadCount adds).
    // The per-step raw timers and step counters are zeroed here, then summed; the
    // run-long state-type counters keep accumulating across steps.
    _runnerStateAdvanceThreadRawTime = 0;
    _runnerStateLoadThreadRawTime    = 0;
    _runnerStateSaveThreadRawTime    = 0;
    _calculateHashThreadRawTime      = 0;
    _checkHashThreadRawTime          = 0;
    _ruleCheckingThreadRawTime       = 0;
    _getFreeStateThreadRawTime       = 0;
    _returnFreeStateThreadRawTime    = 0;
    _calculateRewardThreadRawTime    = 0;
    _getAllowedInputsThreadRawTime   = 0;
    _getCandidateInputsThreadRawTime = 0;
    _popBaseStateDbThreadRawTime     = 0;
    _stepBaseStatesProcessed         = 0;
    _stepNewStatesProcessed          = 0;
    for (ssize_t i = 0; i < _threadCount; i++)
    {
      const auto& a = _threadAccumulators[i];
      _runnerStateAdvanceThreadRawTime += a.runnerStateAdvance;
      _runnerStateLoadThreadRawTime += a.runnerStateLoad;
      _runnerStateSaveThreadRawTime += a.runnerStateSave;
      _calculateHashThreadRawTime += a.calculateHash;
      _checkHashThreadRawTime += a.checkHash;
      _ruleCheckingThreadRawTime += a.ruleChecking;
      _getFreeStateThreadRawTime += a.getFreeState;
      _returnFreeStateThreadRawTime += a.returnFreeState;
      _calculateRewardThreadRawTime += a.calculateReward;
      _getAllowedInputsThreadRawTime += a.getAllowedInputs;
      _getCandidateInputsThreadRawTime += a.getCandidateInputs;
      _popBaseStateDbThreadRawTime += a.popBaseStateDb;
      _stepBaseStatesProcessed += a.baseStatesProcessed;
      _stepNewStatesProcessed += a.newStatesProcessed;
      _normalStates += a.normalStates;
      _repeatedStates += a.repeatedStates;
      _failedStates += a.failedStates;
      _winStates += a.winStates;
      _droppedStatesNoStorage += a.droppedStatesNoStorage;
      _droppedStatesFailedSerialization += a.droppedStatesFailedSerialization;
      _droppedStatesCheckpoint += a.droppedStatesCheckpoint;
      _droppedStatesBelowReference += a.droppedStatesBelowReference;
    }

    // Advancing hash database state
    const auto t0 = jaffarCommon::timing::now();
    if (_hashDbEnabled == true) _hashDb->advanceStep();
    _advanceHashDbThreadRawTime += jaffarCommon::timing::timeDeltaMicroseconds(jaffarCommon::timing::now(), t0);

    // Swapping next and current state databases
    const auto t1 = jaffarCommon::timing::now();
    _stateDb->advanceStep();
    _advanceStateDbThreadRawTime += jaffarCommon::timing::timeDeltaMicroseconds(jaffarCommon::timing::now(), t1);

    // Updating last active last rule Id
    if (_manualSaveSolutionUpdatedLastRuleId)
    {
      _manualSaveSolutionActiveLastRuleId = _manualSaveSolution.lastRuleIdx;
      _manualSaveSolutionLastPath         = _manualSaveSolution.path;
    }

    // Computing step time
    _currentStepTime = jaffarCommon::timing::timeDeltaMicroseconds(jaffarCommon::timing::now(), tStep);

    // Computing total running time
    _totalRunningTime += _currentStepTime;

    // Getting maximum thread step time
    _maxThreadStepTimeThreadId = 0;
    _maxThreadStepTime         = _threadStepTime[0];
    for (ssize_t i = 0; i < _threadCount; i++)
      if (_threadStepTime[i] > _maxThreadStepTime)
      {
        _maxThreadStepTimeThreadId = i;
        _maxThreadStepTime         = _threadStepTime[i];
      }

    // Processing thread-average step timing
    _runnerStateAdvanceAverageTime = _runnerStateAdvanceThreadRawTime / _threadCount;
    _runnerStateLoadAverageTime    = _runnerStateLoadThreadRawTime / _threadCount;
    _runnerStateSaveAverageTime    = _runnerStateSaveThreadRawTime / _threadCount;
    _calculateHashAverageTime      = _calculateHashThreadRawTime / _threadCount;
    _checkHashAverageTime          = _checkHashThreadRawTime / _threadCount;
    _ruleCheckingAverageTime       = _ruleCheckingThreadRawTime / _threadCount;
    _getFreeStateAverageTime       = _getFreeStateThreadRawTime / _threadCount;
    _returnFreeStateAverageTime    = _returnFreeStateThreadRawTime / _threadCount;
    _calculateRewardAverageTime    = _calculateRewardThreadRawTime / _threadCount;
    _popBaseStateDbAverageTime     = _popBaseStateDbThreadRawTime / _threadCount;
    _getAllowedInputsAverageTime   = _getAllowedInputsThreadRawTime / _threadCount;
    _getCandidateInputsAverageTime = _getCandidateInputsThreadRawTime / _threadCount;
    _advanceHashDbAverageTime      = _advanceHashDbThreadRawTime.load();
    _advanceStateDbAverageTime     = _advanceStateDbThreadRawTime.load();

    // Sub-total thread-average step timing
    _subTotalAverageTime = 0;
    _subTotalAverageTime += _runnerStateAdvanceAverageTime;
    _subTotalAverageTime += _runnerStateLoadAverageTime;
    _subTotalAverageTime += _runnerStateSaveAverageTime;
    _subTotalAverageTime += _calculateHashAverageTime;
    _subTotalAverageTime += _checkHashAverageTime;
    _subTotalAverageTime += _ruleCheckingAverageTime;
    _subTotalAverageTime += _getFreeStateAverageTime;
    _subTotalAverageTime += _returnFreeStateAverageTime;
    _subTotalAverageTime += _calculateRewardAverageTime;
    _subTotalAverageTime += _getAllowedInputsAverageTime;
    _subTotalAverageTime += _getCandidateInputsAverageTime;
    _subTotalAverageTime += _popBaseStateDbAverageTime;
    _subTotalAverageTime += _advanceHashDbAverageTime;
    _subTotalAverageTime += _advanceStateDbAverageTime;

    // Processing cumulative timing
    _runnerStateAdvanceAverageCumulativeTime += _runnerStateAdvanceAverageTime;
    _runnerStateLoadAverageCumulativeTime += _runnerStateLoadAverageTime;
    _runnerStateSaveAverageCumulativeTime += _runnerStateSaveAverageTime;
    _calculateHashAverageCumulativeTime += _calculateHashAverageTime;
    _checkHashAverageCumulativeTime += _checkHashAverageTime;
    _ruleCheckingAverageCumulativeTime += _ruleCheckingAverageTime;
    _getFreeStateAverageCumulativeTime += _getFreeStateAverageTime;
    _returnFreeStateAverageCumulativeTime += _returnFreeStateAverageTime;
    _calculateRewardAverageCumulativeTime += _calculateRewardAverageTime;
    _popBaseStateDbAverageCumulativeTime += _popBaseStateDbAverageTime;
    _getAllowedInputsAverageCumulativeTime += _getAllowedInputsAverageTime;
    _getCandidateInputsAverageCumulativeTime += _getCandidateInputsAverageTime;
    _advanceHashDbAverageCumulativeTime += _advanceHashDbAverageTime;
    _advanceStateDbAverageCumulativeTime += _advanceStateDbAverageTime;

    // Sub-total cumulative time calculation
    _subTotalAverageCumulativeTime = 0;
    _subTotalAverageCumulativeTime += _runnerStateAdvanceAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _runnerStateLoadAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _runnerStateSaveAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _calculateHashAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _checkHashAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _ruleCheckingAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _getFreeStateAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _returnFreeStateAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _calculateRewardAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _popBaseStateDbAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _getAllowedInputsAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _getCandidateInputsAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _advanceHashDbAverageCumulativeTime;
    _subTotalAverageCumulativeTime += _advanceStateDbAverageCumulativeTime;

    // Processing state counters
    _totalBaseStatesProcessed += _stepBaseStatesProcessed;
    _totalNewStatesProcessed += _stepNewStatesProcessed;

    // Checkpoint cohort extinction guard: if this step stored ZERO states at the current
    // checkpoint level, the milestone's surviving lineage is gone -- executing the purge would
    // wipe the whole database for the benefit of nobody. Demote the level so the search can
    // re-achieve the milestone; the next genuine rise re-arms the cutoff.
    if (_checkpointLevel.load(std::memory_order_relaxed) > 0 && _stepMaxLevelStored.load(std::memory_order_relaxed) == 0 && _stateDb->getStateCount() > 0)
    {
      std::lock_guard<std::mutex> lk(_checkpointMutex);
      const auto                  lvl = _checkpointLevel.load(std::memory_order_relaxed);
      if (lvl > 0)
      {
        _checkpointLevel.store(lvl - 1, std::memory_order_release);
        jaffarCommon::logger::log("[J+] Checkpoint level %lu cohort extinct -- demoting to %lu\n", lvl, lvl - 1);
      }
    }
    _stepMaxLevelStored.store(0, std::memory_order_relaxed);

    // Advancing step
    _currentStep++;
  }

  /**
   * @brief Frees the best-win and manual-save state buffers allocated in @ref initialize.
   */
  ~Engine()
  {
    // Free the state buffers allocated in initialize() (raw malloc; see _stateSizeInDatabase use above)
    free(_stepBestWinState.stateData);
    free(_manualSaveSolution.stateData);
  }

  // Relevant data for the driver

  /** @brief Returns a reference to the owned state database. */
  auto& getStateDb() const { return _stateDb; }
  /** @brief Returns a copy of the best win state recorded in the current step. */
  auto getStepBestWinState() const { return _stepBestWinState; }

  /// @brief Number of distinct win solutions collected so far (see setWinStateCollection).
  size_t getWinCollectedCount()
  {
    std::lock_guard<std::mutex> l(_winCollectLock);
    return _winKeysSeen.size();
  }
  /// @brief Whether win collection is enabled and has reached its Max Files cap.
  bool isWinCollectionFull()
  {
    std::lock_guard<std::mutex> l(_winCollectLock);
    return _winCollectEnabled && _winKeysSeen.size() >= _winCollectMax;
  }
  /// @brief Whether win collection is enabled.
  bool isWinCollectionEnabled() const { return _winCollectEnabled; }
  /// @brief Win collection Max Files cap.
  size_t getWinCollectionMax() const { return _winCollectMax; }

  /**
   * @brief Enables win-state collection: every win state's solution is saved to
   *        pathPrefix + hex(dedup property bytes) + ".sol", first-seen per key. Configured by the
   *        driver ("Win State Collection" in Driver Configuration); capture happens in the engine
   *        because the worker runner holds each win state live at the moment it is produced.
   * @param dedupProps Names of the game properties whose concatenated bytes form the dedup key.
   * @param pathPrefix Prefix (path + basename stem) for the saved .sol files and manifest.
   * @param maxFiles Stop collecting (and terminate the run) once this many distinct keys are saved.
   */
  void setWinStateCollection(const std::vector<std::string>& dedupProps, const std::string& pathPrefix, const size_t maxFiles)
  {
    _winDedupPropNames = dedupProps;
    _winCollectPrefix  = pathPrefix;
    _winCollectMax     = maxFiles;
    _winCollectEnabled = true;
  }

  /// @brief Whether "Reference Reward Prune" is enabled in the engine configuration (the driver
  ///        checks this to know it must supply the reference trace).
  bool isReferencePruneRequested() const { return _refPruneRequested; }

  /**
   * @brief Arms reference-reward pruning with the per-depth trace: every produced non-win state
   *        whose floor reward falls below the trace at its depth (beyond the configured Tolerance)
   *        is dropped instead of stored. A greedy polish mode: only lineages decisively at or above
   *        a known solution survive, so the frontier stays small and exploration goes deep. The
   *        pinned reference lineage (see setReferenceStates) is exempt so the reference itself
   *        always survives at tolerance 0. Called by the driver once the trace exists.
   * @param trace Per-depth floor-reward trace of the reference solution (index 0 = the seed state).
   */
  void setReferencePruneTrace(const std::vector<float>& trace)
  {
    _refPruneTrace   = trace;
    _refPruneEnabled = true;
  }

  /** @brief Returns a copy of the most recent manually saved solution. */
  auto getManualSaveSolution() const { return _manualSaveSolution; }
  /** @brief Returns the cumulative number of win states found so far. */
  auto getWinStatesFound() const { return _winStates.load(); }
  /** @brief Returns the number of states currently held in the state database. */
  auto getStateCount() const { return _stateDb->getStateCount(); }

  /** @brief Hard memory ceiling (bytes) of the shared input-history backing; 0 for None/Raw (no ceiling). */
  size_t getInputHistoryMaxMemoryBytes() const { return inputHistory::getSharedBackingMaxMemoryBytes(_inputHistoryBacking); }
  /** @brief Current (approximate) live memory (bytes) of the shared input-history backing; 0 for None/Raw. */
  size_t getInputHistoryApproxMemoryBytes() const { return inputHistory::getSharedBackingApproxMemoryBytes(_inputHistoryBacking); }
  /** @brief True if the shared input-history backing (the Trie) has hit its hard node-storage ceiling. */
  bool isInputHistoryExhausted() const { return inputHistory::isSharedBackingExhausted(_inputHistoryBacking); }

  /**
   * @brief Logs engine status: timing breakdown, throughput, state counts, checkpoints, databases, and detected candidate inputs.
   */
  void printInfo()
  {
    // Compact mode: the handful of lines observers actually use, then out. Grep-stable labels.
    if (_compactLog)
    {
      jaffarCommon::logger::log("[J+]  + Elapsed Time (Step/Total):                   %9.3fs / %9.3fs\n", 1.0e-6 * (double)(_currentStepTime),
                                1.0e-6 * (double)(_totalRunningTime));
      jaffarCommon::logger::log("[J+]  + Checkpoint (Level/Tolerance/Cutoff):         %lu / %lu / %lu\n", _checkpointLevel.load(), _checkpointTolerance.load(),
                                _checkpointCutoff.load());
      jaffarCommon::logger::log("[J+]  + New States Processed:                        %.3f Mstates (Total: %.3f Mstates) @ %.3f Mstates/s\n",
                                1.0e-6 * (double)_stepNewStatesProcessed, 1.0e-6 * (double)_totalNewStatesProcessed,
                                1.0e-6 * (double)_stepNewStatesProcessed / (1.0e-6 * (double)_currentStepTime));
      jaffarCommon::logger::log("[J+]  + States (fail/rep/drop cumulative):           %lu / %lu / %lu\n", _failedStates.load(), _repeatedStates.load(),
                                _droppedStatesNoStorage.load() + _droppedStatesFailedSerialization.load() + _droppedStatesCheckpoint.load() + _droppedStatesBelowReference.load());
      jaffarCommon::logger::log("[J+]  + Win States:                                  %lu (%5.3f%% of New States Processed) \n", _winStates.load(),
                                100.0 * (double)_winStates.load() / (double)_totalNewStatesProcessed);
      if (_winCollectEnabled)
        jaffarCommon::logger::log("[J+]  + Win States Collected:                        %lu/%lu\n", (unsigned long)getWinCollectedCount(), (unsigned long)_winCollectMax);
      if (_refPruneEnabled)
        jaffarCommon::logger::log("[J+]  + Dropped States (Below Reference):            %lu (%5.3f%% of New States Processed, prune tol %.1f) \n",
                                  _droppedStatesBelowReference.load(), 100.0 * (double)_droppedStatesBelowReference.load() / (double)_totalNewStatesProcessed, _refPruneTolerance);
      if (_refPinEnabled)
        jaffarCommon::logger::log("[J+]  + Reference Pin Hits (cumulative):             %lu (deepest %lu / %lu)\n", _refPinHits.load(), _refPinMaxDepthHit.load(),
                                  _refPinHashes.size());
      jaffarCommon::logger::log("[J+]  + State Db States:                             %lu (%.2f / %.2f GB, %.1f%% full)\n", _stateDb->getStateCount(),
                                (double)(_stateDb->getStateCount() * _stateDb->getStateSizeInDatabase()) / (1024.0 * 1024.0 * 1024.0),
                                (double)_stateDb->getMaxBudgetBytes() / (1024.0 * 1024.0 * 1024.0),
                                100.0 * (double)(_stateDb->getStateCount() * _stateDb->getStateSizeInDatabase()) / (double)_stateDb->getMaxBudgetBytes());
      return;
    }
    // Printing information
    jaffarCommon::logger::log("[J+] Thread Count / NUMA Domains:                 %3d / %d\n", _threadCount, _numaCount);
#ifdef JAFFARPLUS_DETAILED_PROFILING
    jaffarCommon::logger::log("[J+] Elapsed Time (Step/Total):                   %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_currentStepTime),
                              100.0 * ((double)(_subTotalAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_totalRunningTime),
                              100.0 * ((double)_subTotalAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Runner State Avance (Step/Total):         %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_runnerStateAdvanceAverageTime),
                              100.0 * ((double)(_runnerStateAdvanceAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_runnerStateAdvanceAverageCumulativeTime),
                              100.0 * ((double)_runnerStateAdvanceAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Runner State Load (Step/Total):           %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_runnerStateLoadAverageTime),
                              100.0 * ((double)(_runnerStateLoadAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_runnerStateLoadAverageCumulativeTime),
                              100.0 * ((double)_runnerStateLoadAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Runner State Save (Step/Total):           %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_runnerStateSaveAverageTime),
                              100.0 * ((double)(_runnerStateSaveAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_runnerStateSaveAverageCumulativeTime),
                              100.0 * ((double)_runnerStateSaveAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Hash Calculation (Step/Total):            %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_calculateHashAverageTime),
                              100.0 * ((double)(_calculateHashAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_calculateHashAverageCumulativeTime),
                              100.0 * ((double)_calculateHashAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Hash Checking (Step/Total):               %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_checkHashAverageTime),
                              100.0 * ((double)(_checkHashAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_checkHashAverageCumulativeTime),
                              100.0 * ((double)_checkHashAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Rule Checking (Step/Total):               %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_ruleCheckingAverageTime),
                              100.0 * ((double)(_ruleCheckingAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_ruleCheckingAverageCumulativeTime),
                              100.0 * ((double)_ruleCheckingAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Get Free State (Step/Total):              %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_getFreeStateAverageTime),
                              100.0 * ((double)(_getFreeStateAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_getFreeStateAverageCumulativeTime),
                              100.0 * ((double)_getFreeStateAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Return Free State (Step/Total):           %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_returnFreeStateAverageTime),
                              100.0 * ((double)(_returnFreeStateAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_returnFreeStateAverageCumulativeTime),
                              100.0 * ((double)_returnFreeStateAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Calculate Reward (Step/Total):            %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_calculateRewardAverageTime),
                              100.0 * ((double)(_calculateRewardAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_calculateRewardAverageCumulativeTime),
                              100.0 * ((double)_calculateRewardAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Popping Base State (Step/Total):          %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_popBaseStateDbAverageTime),
                              100.0 * ((double)(_popBaseStateDbAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_popBaseStateDbAverageCumulativeTime),
                              100.0 * ((double)_popBaseStateDbAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Get Allowed Inputs (Step/Total):          %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_getAllowedInputsAverageTime),
                              100.0 * ((double)(_getAllowedInputsAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_getAllowedInputsAverageCumulativeTime),
                              100.0 * ((double)_getAllowedInputsAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Get Candidate Inputs (Step/Total):        %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_getCandidateInputsAverageTime),
                              100.0 * ((double)(_getCandidateInputsAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_getCandidateInputsAverageCumulativeTime),
                              100.0 * ((double)_getCandidateInputsAverageCumulativeTime) / (double)(_totalRunningTime));
#else
    // Detailed per-operation profiling is compiled out (default). Only the coarse step wall time and
    // the serially-measured DB-advance stages below are available; build -DdetailedProfiling=true for
    // the full per-operation breakdown.
    jaffarCommon::logger::log("[J+] Elapsed Time (Step/Total):                   %9.3fs / %9.3fs   (per-operation breakdown disabled; build -DdetailedProfiling=true)\n",
                              1.0e-6 * (double)(_currentStepTime), 1.0e-6 * (double)(_totalRunningTime));
#endif

    jaffarCommon::logger::log("[J+]  + Advance Hash Db (Step/Total):             %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_advanceHashDbAverageTime),
                              100.0 * ((double)(_advanceHashDbAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_advanceHashDbAverageCumulativeTime),
                              100.0 * ((double)_advanceHashDbAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+]  + Advance State Db (Step/Total):            %9.3fs (%7.3f%%) / %9.3fs (%3.3f%%)\n", 1.0e-6 * (double)(_advanceStateDbAverageTime),
                              100.0 * ((double)(_advanceStateDbAverageTime) / (double)(_currentStepTime)), 1.0e-6 * (double)(_advanceStateDbAverageCumulativeTime),
                              100.0 * ((double)_advanceStateDbAverageCumulativeTime) / (double)(_totalRunningTime));

    jaffarCommon::logger::log("[J+] Checkpoint (Level/Tolerance/Cutoff):         %lu / %lu / %lu\n", _checkpointLevel.load(), _checkpointTolerance.load(),
                              _checkpointCutoff.load());
    jaffarCommon::logger::log("[J+] Base States Processed:                       %.3f Mstates (Total: %.3f Mstates)\n", 1.0e-6 * (double)_stepBaseStatesProcessed,
                              1.0e-6 * (double)_totalBaseStatesProcessed);
    jaffarCommon::logger::log("[J+] New States Processed:                        %.3f Mstates (Total: %.3f Mstates)\n", 1.0e-6 * (double)_stepNewStatesProcessed,
                              1.0e-6 * (double)_totalNewStatesProcessed);
    if (_refPinEnabled)
      jaffarCommon::logger::log("[J+] Reference Pin Hits (cumulative):             %lu (deepest %lu / %lu; ref-depth matches seen %lu)\n", _refPinHits.load(),
                                _refPinMaxDepthHit.load(), _refPinHashes.size(), _refPinSeenPreDedup.load());

    jaffarCommon::logger::log("[J+] Base States Performance:                     %.3f Mstates/s (Average: %.3f Mstates/s)\n",
                              1.0e-6 * (double)_stepBaseStatesProcessed / (1.0e-6 * (double)_currentStepTime),
                              1.0e-6 * (double)_totalBaseStatesProcessed / (1.0e-6 * (double)_totalRunningTime));
    jaffarCommon::logger::log("[J+] New States Performance:                      %.3f Mstates/s (Average: %.3f Mstates/s)\n",
                              1.0e-6 * (double)_stepNewStatesProcessed / (1.0e-6 * (double)_currentStepTime),
                              1.0e-6 * (double)_totalNewStatesProcessed / (1.0e-6 * (double)_totalRunningTime));

    jaffarCommon::logger::log("[J+] Dropped States (No Storage Available):       %lu (%5.3f%% of New States Processed) \n", _droppedStatesNoStorage.load(),
                              100.0 * (double)_droppedStatesNoStorage.load() / (double)_totalNewStatesProcessed);
    jaffarCommon::logger::log("[J+] Dropped States (Failed Serialization):       %lu (%5.3f%% of New States Processed) \n", _droppedStatesFailedSerialization.load(),
                              100.0 * (double)_droppedStatesFailedSerialization.load() / (double)_totalNewStatesProcessed);
    jaffarCommon::logger::log("[J+] Dropped States (Checkpoint):                 %lu (%5.3f%% of New States Processed) \n", _droppedStatesCheckpoint.load(),
                              100.0 * (double)_droppedStatesCheckpoint.load() / (double)_totalNewStatesProcessed);
    if (_refPruneEnabled)
      jaffarCommon::logger::log("[J+] Dropped States (Below Reference):            %lu (%5.3f%% of New States Processed, prune tol %.1f) \n", _droppedStatesBelowReference.load(),
                                100.0 * (double)_droppedStatesBelowReference.load() / (double)_totalNewStatesProcessed, _refPruneTolerance);
    jaffarCommon::logger::log("[J+] Failed States:                               %lu (%5.3f%% of New States Processed) \n", _failedStates.load(),
                              100.0 * (double)_failedStates.load() / (double)_totalNewStatesProcessed);
    jaffarCommon::logger::log("[J+] Repeated States:                             %lu (%5.3f%% of New States Processed) \n", _repeatedStates.load(),
                              100.0 * (double)_repeatedStates.load() / (double)_totalNewStatesProcessed);
    jaffarCommon::logger::log("[J+] Normal States:                               %lu (%5.3f%% of New States Processed) \n", _normalStates.load(),
                              100.0 * (double)_normalStates.load() / (double)_totalNewStatesProcessed);
    jaffarCommon::logger::log("[J+] Win States:                                  %lu (%5.3f%% of New States Processed) \n", _winStates.load(),
                              100.0 * (double)_winStates.load() / (double)_totalNewStatesProcessed);

    if (_winCollectEnabled)
      jaffarCommon::logger::log("[J+] Win States Collected:                        %lu/%lu\n", (unsigned long)getWinCollectedCount(), (unsigned long)_winCollectMax);
    // Print state database information
    jaffarCommon::logger::log("[J+] State Database Information:\n");
    _stateDb->printInfo();

    if (_hashDbEnabled == true)
    {
      jaffarCommon::logger::log("[J+] Hash Database Information:\n");
      _hashDb->printInfo();
    }

    jaffarCommon::logger::log("[J+] Manually Saved Solution:\n");
    jaffarCommon::logger::log("[J+]   + Path:                                    '%s'\n", _manualSaveSolution.path.c_str());
    jaffarCommon::logger::log("[J+]   + Reward:                                  %f\n", _manualSaveSolution.reward);
    jaffarCommon::logger::log("[J+]   + Last Rule Idx:                           %ld (Active: %ld, Path: '%s')\n", _manualSaveSolution.lastRuleIdx,
                              _manualSaveSolutionActiveLastRuleId, _manualSaveSolutionLastPath.c_str());

    // Printing candidate inpts
    jaffarCommon::logger::log("[J+] Candidate Inputs:\n");
    for (const auto& entry : _candidateInputsDetected)
    {
      jaffarCommon::logger::log("[J+]  + Hash:                                     %s\n", jaffarCommon::hash::hashToString(entry.first).c_str());
      for (const auto input : entry.second) jaffarCommon::logger::log("[J+]    + %3lu %s\n", input, _runners[0]->getInputStringFromIndex(input).c_str());
    }
  }

  /** @brief Returns the size, in bytes, of a single state as stored in the database. */
  __INLINE__ size_t getStateSizeInDatabase() const { return _stateSizeInDatabase; }

  /// @brief Full self-contained state size ([hot]+[history]); for standalone snapshots outside the slabs.
  __INLINE__ size_t getFullStateSize() const { return _fullStateSize; }

private:
  /// @brief Whether win-state collection is enabled (see setWinStateCollection).
  bool _winCollectEnabled = false;

  /// @brief Names of the game properties whose bytes form the win-collection dedup key.
  std::vector<std::string> _winDedupPropNames;

  /// @brief Path prefix for saved win-collection .sol files and the manifest.
  std::string _winCollectPrefix;

  /// @brief Maximum number of distinct win solutions to collect before terminating.
  size_t _winCollectMax = 1000;

  /// @brief Dedup keys of the win solutions collected so far.
  std::set<std::string> _winKeysSeen;

  /// @brief Guards _winKeysSeen and the win-collection file writes across worker threads.
  std::mutex _winCollectLock;

  /// @brief Whether "Reference Reward Prune" was enabled in the engine configuration.
  bool _refPruneRequested = false;

  /// @brief Whether reference-reward pruning is armed (requested AND trace supplied).
  bool _refPruneEnabled = false;

  /// @brief Per-depth floor-reward trace of the reference solution for pruning.
  std::vector<float> _refPruneTrace;

  /// @brief Allowed slack below the reference trace before a state is pruned.
  float _refPruneTolerance = 0.0f;

  /// @brief Number of base states a worker pulls from the state-DB queue per lock acquisition (batch size).
  // Number of base states a worker pulls from the shared per-NUMA state-DB queue per lock
  // acquisition (into a thread-local buffer), instead of locking once per state. On a light
  // emulator with dozens of worker threads per NUMA domain, that single-state lock/unlock was the
  // dominant cost ("Popping Base State" ~37% of wall time); batching collapses it to <0.5%.
  //
  // The value is empirically tuned (EPYC 9755, 256 threads, SDLPoP lvl01): throughput is flat-
  // optimal across 4-16 and falls off above it (32 ~ -4%, 64 ~ -10%, 512 ~ -9%). The reason is that
  // once the lock contention is gone, the dominant effect is intra-step load balancing across 256
  // threads -- a larger batch lets one straggler hold many states while others see the queue drain
  // and go idle. 16 sits at the top of the flat region while keeping enough amortization headroom to
  // stay robust for workloads with cheaper per-state work or higher thread counts. (A page-sized
  // batch of 512 pointers was tried, on the theory the buffer would be cache/TLB friendly, but the
  // buffer is tiny and L1-resident at any of these sizes, so the load-imbalance cost dominated.)
  // The batch size is configurable ("Base State Batch Size"); when unset it comes from the
  // emulator's getSuggestedStateBatchSize() (heavy cores 1 for load balance, light cores ~16 to
  // amortize the queue lock). BASE_STATE_BATCH_MAX bounds the thread-local pull buffer; the active
  // count is _baseStateBatch (clamped to [1, MAX]).
  static constexpr size_t BASE_STATE_BATCH_MAX = 16;

  /// @brief Outcome of running a single input on a base state.
  enum inputResult_t
  {
    repeated,                   ///< Resulting state's hash was already seen.
    droppedNoStorage,           ///< No free state slot was available to store the new state.
    droppedFailedSerialization, ///< Pushing the state into the database failed (e.g. serialization error).
    droppedCheckpoint,          ///< State did not meet the current checkpoint level past the cutoff step.
    droppedBelowReference,      ///< State's reward fell below the reference trace at its depth (beyond the prune tolerance).
    failed,                     ///< Resulting state was classified as a loss.
    normal,                     ///< Resulting state was a normal state and was stored.
    win                         ///< Resulting state was a win state.
  };

  /// @brief A reward value paired with a serialized state buffer.
  struct stateInfo_t
  {
    float  reward;              ///< Reward associated with the stored state.
    void*  stateData = nullptr; ///< Raw buffer holding the serialized state, or nullptr if unset.
    size_t stepCount = 0;       ///< Depth (input count) of the saved state, recorded at capture (the count is not serialized per-state).
  };

  /// @brief A manually saved solution: its input path, reward, serialized state, and triggering rule index.
  struct manualSaveSolution_t
  {
    std::string path;                ///< Input sequence (solution path) that reached the saved state.
    float       reward;              ///< Reward of the saved state.
    void*       stateData = nullptr; ///< Raw buffer holding the serialized saved state, or nullptr if unset.
    ssize_t     lastRuleIdx;         ///< Index of the last rule active when the state was saved.
    size_t      stepCount = 0;       ///< Depth (input count) of the saved state, recorded at capture.
  };

  /**
   * @brief Per-thread accumulator for timing and counters.
   *
   * @details All hot-loop work writes only into its own thread's instance (plain, non-atomic
   * adds), and the engine reduces these into the shared totals once per step. This
   * removes the per-input atomic contention that previously serialized high core counts.
   *
   * Aligned (and implicitly sized) to a cache-line multiple so that adjacent threads'
   * accumulators never share a cache line (no false sharing).
   */
  struct alignas(64) threadAccumulator_t
  {
    // Timers (microseconds)
    size_t runnerStateAdvance; ///< Time spent advancing the runner state with an input.
    size_t runnerStateLoad;    ///< Time spent loading states into the runner.
    size_t runnerStateSave;    ///< Time spent saving runner states into the state database.
    size_t calculateHash;      ///< Time spent computing state hashes.
    size_t checkHash;          ///< Time spent checking hashes against the hash database.
    size_t ruleChecking;       ///< Time spent evaluating rules and determining state type.
    size_t getFreeState;       ///< Time spent acquiring free state slots.
    size_t returnFreeState;    ///< Time spent returning state slots to the free queue.
    size_t calculateReward;    ///< Time spent computing state rewards.
    size_t getAllowedInputs;   ///< Time spent querying the runner's allowed inputs.
    size_t getCandidateInputs; ///< Time spent querying the runner's candidate inputs.
    size_t popBaseStateDb;     ///< Time spent popping base-state batches from the state database.

    // Counters
    size_t baseStatesProcessed;              ///< Number of base states this thread expanded.
    size_t newStatesProcessed;               ///< Number of new states this thread produced via inputs.
    size_t normalStates;                     ///< Number of normal states produced.
    size_t repeatedStates;                   ///< Number of states dropped as repeated.
    size_t failedStates;                     ///< Number of states classified as failures.
    size_t winStates;                        ///< Number of win states produced.
    size_t droppedStatesNoStorage;           ///< Number of states dropped for lack of free storage.
    size_t droppedStatesFailedSerialization; ///< Number of states dropped due to failed serialization.
    size_t droppedStatesCheckpoint;          ///< Number of states dropped for not meeting the checkpoint.
    size_t droppedStatesBelowReference;      ///< Number of states dropped for falling below the reference reward trace.

    /// @brief Resets all timers and counters to zero.
    __INLINE__ void reset() { *this = threadAccumulator_t{}; }
  };

  /**
   * @brief Worker body executed in parallel by every thread during a step.
   *
   * @details Repeatedly pops batches of base states from the state database, loads each into the
   * thread's runner, gathers its allowed and candidate inputs, runs each input via @ref runNewInput,
   * deduplicating candidate inputs per base-state input hash, and returns processed base states to
   * the free queue. Records the thread's total step time into @ref _threadStepTime.
   */
  void workerFunction()
  {
    // Getting my thread id
    const auto threadId = jaffarCommon::parallel::getThreadId();

    // Getting my thread-local accumulator (no atomics in the hot path)
    auto& acc = _threadAccumulators[threadId];

    // Starting to measure thread-specific step time
    const auto threadTime0 = jaffarCommon::timing::now();

    // Getting my runner
    auto& r = _runners[threadId];

    // Base states are pulled from the database in batches into this thread-local buffer, so the
    // shared per-NUMA queue lock is acquired once per BASE_STATE_BATCH states instead of once per
    // state. With many worker threads per NUMA domain and cheap per-state work, the single-state
    // lock/unlock dominates wall time ("Popping Base State"); batching amortizes it away.
    void* baseStateBatch[BASE_STATE_BATCH_MAX];
    JAFFAR_PROF_DECL(t);
    size_t batchCount = _stateDb->popStates(baseStateBatch, _baseStateBatch, threadId);
    JAFFAR_PROF_ACC(acc.popBaseStateDb, t);
    size_t batchIdx = 0;

    // While there are still states in the database, keep on grabbing them
    while (batchIdx < batchCount)
    {
      // Taking the next base state from the local batch
      void* baseStateData = baseStateBatch[batchIdx++];

      // Increasing base state counter
      acc.baseStatesProcessed++;

      // Load state into runner via the state database (base states are slab slots: hot slot + cold path)
      JAFFAR_PROF_DECL(t0);
      _stateDb->loadStateFromSlot(*r, baseStateData);
      r->setSearchStep(_currentStep); // base state's depth = current search step (count is not stored per-state)
      JAFFAR_PROF_ACC(acc.runnerStateLoad, t0);

      // Getting allowed inputs
      JAFFAR_PROF_DECL(t1);
      const auto allowedInputs = r->getAllowedInputs();
      JAFFAR_PROF_ACC(acc.getAllowedInputs, t1);

      // Getting candidate inputs (those not already covered by the allowed set). Computed here,
      // before the allowed inputs are tried, while the runner still holds the unperturbed base state.
      JAFFAR_PROF_DECL(t2);
      auto candidateInputs = r->getCandidateInputs();
      JAFFAR_PROF_ACC(acc.getCandidateInputs, t2);

      // Finding unique candidate inputs
      std::vector<InputSet::inputIndex_t> uniqueCandidateInputs;
      for (const auto& input : candidateInputs)
        if (std::find(allowedInputs.begin(), allowedInputs.end(), input) == allowedInputs.end()) uniqueCandidateInputs.push_back(input);

      // Discriminating hash of the *base* state (the situation being expanded), used to dedup
      // candidate-input probing across like states. It must be taken from the base state, so it is
      // captured now -- before the allowed/candidate inputs below advance the runner away from it.
      jaffarCommon::hash::hash_t baseStateInputHash{};
      if (uniqueCandidateInputs.empty() == false) baseStateInputHash = r->getGame()->getStateInputHash();

      // Trying out each possible input in the set
      for (auto inputItr = allowedInputs.begin(); inputItr != allowedInputs.end(); inputItr++) runNewInput(*r, baseStateData, *inputItr, acc, threadId);

      // Run each candidate input, keyed by the base state's discriminating hash
      for (const auto input : uniqueCandidateInputs)
      {
        // Making sure we don't try the input if it was already detected for this type of state
        if (_candidateInputsDetected.contains(baseStateInputHash))
          if (_candidateInputsDetected[baseStateInputHash].contains(input)) continue;

        // Running input
        const auto result = runNewInput(*r, baseStateData, input, acc, threadId);

        // If this is not a repeated state, store it as new candidate input
        if (result != inputResult_t::repeated) _candidateInputsDetected[baseStateInputHash].insert(input);
      }

      // Return base state to the free state queue
      JAFFAR_PROF_DECL(t8);
      _stateDb->returnFreeState(baseStateData, threadId);
      JAFFAR_PROF_ACC(acc.returnFreeState, t8);

      // When the local batch is exhausted, pull the next batch from the database
      if (batchIdx >= batchCount)
      {
        JAFFAR_PROF_DECL(t9);
        batchCount = _stateDb->popStates(baseStateBatch, _baseStateBatch, threadId);
        JAFFAR_PROF_ACC(acc.popBaseStateDb, t9);
        batchIdx = 0;
      }
    }

    // Taking final thread-specific time measurement
    _threadStepTime[threadId] = jaffarCommon::timing::timeDeltaMicroseconds(jaffarCommon::timing::now(), threadTime0);
  }

  /**
   * @brief Re-loads the base state, runs a single input, updates per-outcome counters and checkpoint tracking.
   * @param r             The runner to load the base state into and advance.
   * @param baseStateData Serialized base state to reload before running the input.
   * @param input         Index of the input to run.
   * @param acc           This thread's accumulator to update with the outcome counter.
   * @param threadId      Calling thread's id, used for state-database free/allocation operations.
   * @return The @ref inputResult_t describing the outcome of running the input.
   */
  __INLINE__ inputResult_t runNewInput(Runner& r, const void* baseStateData, const InputSet::inputIndex_t input, threadAccumulator_t& acc, const size_t threadId)
  {
    // Increasing new state counter
    acc.newStatesProcessed++;

    // Re-loading base state (slab slot: hot slot + cold path)
    JAFFAR_PROF_DECL(t0);
    _stateDb->loadStateFromSlot(r, baseStateData);
    r.setSearchStep(_currentStep); // base state's depth = current search step (count is not stored per-state)
    JAFFAR_PROF_ACC(acc.runnerStateLoad, t0);

    // Running input
    const auto result = runInput(r, input, acc, threadId);

    // Update counters depending on the outcomes
    if (result == inputResult_t::normal) acc.normalStates++;
    if (result == inputResult_t::repeated) acc.repeatedStates++;
    if (result == inputResult_t::failed) acc.failedStates++;
    if (result == inputResult_t::win) acc.winStates++;
    if (result == inputResult_t::droppedNoStorage) acc.droppedStatesNoStorage++;
    if (result == inputResult_t::droppedFailedSerialization) acc.droppedStatesFailedSerialization++;
    if (result == inputResult_t::droppedCheckpoint) acc.droppedStatesCheckpoint++;
    if (result == inputResult_t::droppedBelowReference) acc.droppedStatesBelowReference++;

    // Checking whether this state's checkpoint is new. Only states that were actually stored
    // (normal/win) may raise the global level: a failed or repeated state that satisfies a
    // checkpoint rule leaves no surviving lineage at that level, and raising the level from it
    // purges the entire database (guaranteed starvation at tolerance 0).
    if (result == inputResult_t::normal || result == inputResult_t::win)
    {
      const auto stateCheckpointLevel     = r.getGame()->getCheckpointLevel();
      const auto stateCheckpointTolerance = r.getGame()->getCheckpointTolerance();
      if (stateCheckpointLevel >= _checkpointLevel.load(std::memory_order_relaxed)) _stepMaxLevelStored.fetch_add(1, std::memory_order_relaxed);
      if (stateCheckpointLevel > _checkpointLevel.load(std::memory_order_relaxed))
      {
        // Publication order is load-bearing: the new cutoff must become visible BEFORE the new
        // level, or a concurrent thread can observe (new level, stale cutoff) and purge states
        // that are inside the new tolerance window -- wiping the database in a single step.
        std::lock_guard<std::mutex> lk(_checkpointMutex);
        if (stateCheckpointLevel > _checkpointLevel.load(std::memory_order_relaxed))
        {
          _checkpointTolerance.store(stateCheckpointTolerance, std::memory_order_relaxed);
          _checkpointCutoff.store(_currentStep + stateCheckpointTolerance, std::memory_order_relaxed);
          _checkpointLevel.store(stateCheckpointLevel, std::memory_order_release);
        }
      }
    }

    // Returning result
    return result;
  }

  /**
   * @brief Advances the runner by one input and classifies/stores the resulting state.
   *
   * @details Advances the runner with @p input, computes and (when enabled) checks the resulting
   * hash for repeats, evaluates rules, applies the checkpoint cutoff, and determines the state type.
   * Win states update @ref _stepBestWinState when their reward is higher; normal states are pushed
   * into the state database; failed/repeated/dropped states return the corresponding result. Also
   * updates the manual-save solution when the game requests it and the reward improves.
   * @param r        The runner holding the base state, advanced by this call.
   * @param input    Index of the input to apply.
   * @param acc      This thread's accumulator, updated with timing measurements.
   * @param threadId Calling thread's id, used for state-database free/allocation operations.
   * @return The @ref inputResult_t describing the outcome.
   */
  __INLINE__ inputResult_t runInput(Runner& r, const InputSet::inputIndex_t input, threadAccumulator_t& acc, const size_t threadId)
  {
    // Now advancing state with the provided input
    JAFFAR_PROF_DECL(t1);
    r.advanceState(input);
    JAFFAR_PROF_ACC(acc.runnerStateAdvance, t1);

    // Computing runner hash
    JAFFAR_PROF_DECL(t2);
    const auto hash = r.computeHash();
    JAFFAR_PROF_ACC(acc.calculateHash, t2);

    // Checking if hash is repeated (i.e., has been seen before)
    JAFFAR_PROF_DECL(t3);
    bool hashExists = _hashDbEnabled ? _hashDb->checkHashExists(hash) : false;
    JAFFAR_PROF_ACC(acc.checkHash, t3);

    // Reference pinning (decided BEFORE the dedup drop): if this child matches the reference lineage at
    // its own depth (k=0) or a near-future depth (k=1..Lookahead, i.e. it is 1..L frames ahead), claim
    // that reference depth EXACTLY ONCE (compare_exchange) and pin this copy. Claiming before dedup is
    // essential: the same reference RAM is reached by many convergent paths (and can be first reached --
    // and added UNPINNED -- at a shallower depth by a path more than Lookahead frames ahead), so if we
    // waited until after the dedup check the reference lineage would be deduped away before it could be
    // pinned. The single claimed copy bypasses dedup so it is always anchored; all later duplicates are
    // dropped normally. Only the DB-ordering reward is affected (the floor uses the unbiased reward).
    float pinBonus = 0.0f;
    bool  isRefPin = false;
    if (_refPinEnabled)
    {
      const size_t newDepth = _currentStep + 1;
      for (size_t k = 0; k <= _refPinLookahead; k++)
      {
        const size_t idx = newDepth + k;
        if (idx < _refPinHashes.size() && hash == _refPinHashes[idx])
        {
          _refPinSeenPreDedup.fetch_add(1, std::memory_order_relaxed);
          // Hash equality is only a PRE-FILTER: under a quantized dedup hash a whole cell of states
          // shares the reference's hash. When exact reference states are available (floor Solution File
          // replay), verify by byte comparison so only the true reference lineage is pinned.
          bool verified = true;
          if (idx < _refStates.size() && _refStates[idx].size() > 0)
          {
            thread_local std::vector<uint8_t> scratch;
            scratch.resize(_refStates[idx].size());
            jaffarCommon::serializer::Contiguous s(scratch.data(), scratch.size());
            r.serializeState(s);
            verified = true;
            for (size_t i = 0; i < scratch.size(); i++)
              if (_refVolatileMask[i] == 0 && scratch[i] != _refStates[idx][i])
              {
                verified = false;
                break;
              }
            // One-shot diagnostic: report the first differing byte of the first failed verification.
            if (verified == false)
            {
              static std::atomic<int> reported{0};
              int                     expected = 0;
              if (reported.compare_exchange_strong(expected, 1))
              {
                size_t nDiff = 0;
                char   buf[1024];
                int    p         = 0;
                size_t firstDiff = 0;
                for (size_t off = 0; off < scratch.size(); off++)
                  if (_refVolatileMask[off] == 0 && scratch[off] != _refStates[idx][off])
                  {
                    if (nDiff == 0) firstDiff = off;
                    if (nDiff < 32 && p < 900) p += snprintf(buf + p, sizeof(buf) - p, " %lu(%02X!=%02X)", off, scratch[off], _refStates[idx][off]);
                    nDiff++;
                  }
                jaffarCommon::logger::log("[J+] PIN-VERIFY MISMATCH at depth %lu: %lu UNMASKED differing bytes / %lu:%s\n", idx, nDiff, scratch.size(), buf);
                // Hex window around the first difference, both sides, to identify the member by layout
                const size_t w0 = firstDiff >= 32 ? firstDiff - 32 : 0, w1 = std::min(scratch.size(), firstDiff + 32);
                char         cand[256], refb[256];
                int          pc = 0, pr = 0;
                for (size_t off = w0; off < w1; off++)
                {
                  pc += snprintf(cand + pc, sizeof(cand) - pc, "%02X", scratch[off]);
                  pr += snprintf(refb + pr, sizeof(refb) - pr, "%02X", _refStates[idx][off]);
                }
                jaffarCommon::logger::log("[J+]   window [%lu..%lu) cand: %s\n[J+]   window [%lu..%lu) ref:  %s\n", w0, w1, cand, w0, w1, refb);
              }
            }
          }
          if (verified)
          {
            // The dedup bypass (and bonus) applies ONCE per reference depth -- the CAS is the gate,
            // not just a hit counter. Without this, any mode that collapses the frontier onto the
            // reference path (e.g. reference-prune polishing) has EVERY lineage re-create the
            // reference states each step, and the pinned copies -- exempt from dedup -- flood the
            // state DB with duplicates of the same ~L reference states (measured: >80% of a 10 GB
            // DB were copies). Later re-creations are ordinary states: the hash exists, dedup
            // drops them, and the one anchored copy already guarantees the reference survives.
            uint8_t expected = 0;
            if (_refPinnedAtDepth[idx].compare_exchange_strong(expected, 1, std::memory_order_relaxed))
            {
              pinBonus = _refPinBonus + (float)k * _refPinLookaheadBonus;
              isRefPin = true;
              _refPinHits.fetch_add(1, std::memory_order_relaxed);
              _refPinMaxDepthHit.store(std::max(_refPinMaxDepthHit.load(std::memory_order_relaxed), idx), std::memory_order_relaxed);
            }
          }
          break;
        }
      }
    }

    // If state is repeated then we are not interested in it -- UNLESS it is the reference copy we just
    // claimed, which we keep and anchor regardless (bypassing dedup for the pinned lineage).
    if (hashExists == true && isRefPin == false) return inputResult_t::repeated;

    // Evaluating game rules based on the new state
    JAFFAR_PROF_DECL(t4);
    r.getGame()->evaluateRules();

    // Checking whether this state meets checkpoint
    {
      // Read the level with acquire FIRST: seeing a new level guarantees the matching cutoff
      // (written before it under release ordering) is also visible.
      const auto globalCheckpointLevel = _checkpointLevel.load(std::memory_order_acquire);
      const auto stateCheckpointLevel  = r.getGame()->getCheckpointLevel();

      // If state does not meet checkpoint past the cutoff, then do not process it further
      if (stateCheckpointLevel < globalCheckpointLevel && _currentStep > _checkpointCutoff.load(std::memory_order_relaxed)) return inputResult_t::droppedCheckpoint;
    }

    // Determining state type
    r.getGame()->updateGameStateType();

    // Getting state type
    const auto stateType = r.getGame()->getStateType();
    JAFFAR_PROF_ACC(acc.ruleChecking, t4);

    // Now we have determined the state is not repeated, check if it's not a failed state
    if (stateType == Game::stateType_t::fail) return inputResult_t::failed;

    // Now that the state is not failed nor repeated, this is effectively a new state to add
    JAFFAR_PROF_DECL(t5);
    void* newStateData = _stateDb->getFreeState(threadId);
    JAFFAR_PROF_ACC(acc.getFreeState, t5);

    // If couldn't get any memory, simply drop the state
    if (newStateData == nullptr) return inputResult_t::droppedNoStorage;

    // Updating state reward
    JAFFAR_PROF_DECL(t6);
    r.getGame()->updateReward();

    // Getting state reward, plus the reference-pin bonus decided before the dedup check above.
    auto reward = r.getGame()->getReward() + pinBonus;
    JAFFAR_PROF_ACC(acc.calculateReward, t6);

    // Reference-reward pruning (greedy polish mode): drop any non-win child whose floor reward is
    // below the reference trace at its depth beyond the tolerance. Children produced during this
    // step sit at depth _currentStep + 1 (the step counter advances at the end of runStep). Win
    // states are exempt (a win below the trace still ends the search) and so is the pinned
    // reference lineage, which must survive at tolerance 0 despite float equality being its floor.
    if (_refPruneEnabled && stateType != Game::stateType_t::win && isRefPin == false)
    {
      const size_t childDepth = _currentStep + 1;
      if (childDepth < _refPruneTrace.size() && r.getGame()->getFloorReward() < _refPruneTrace[childDepth] - _refPruneTolerance)
      {
        _stateDb->returnFreeState(newStateData, threadId);
        return inputResult_t::droppedBelowReference;
      }
    }

    // If this is a win state, register it and return
    if (stateType == Game::stateType_t::win)
    {
      ///////////// Best win state needs to be stored if its better than any previous one found

      // Check if the new win state is the best and store it in that case
      // Win-state collection: the runner holds the live win state right now -- read the dedup
      // key straight from its game properties and write its input history if the key is new.
      if (_winCollectEnabled)
      {
        std::string key;
        char        hx[4];
        for (const auto& pn : _winDedupPropNames)
        {
          auto* prop = r.getGame()->findProperty(pn);
          if (prop == nullptr) JAFFAR_THROW_LOGIC("[ERROR] Win State Collection dedup property '%s' not registered\n", pn.c_str());
          const auto* bytes = (const uint8_t*)prop->getPointer();
          for (size_t bi = 0; bi < prop->getSize(); bi++)
          {
            snprintf(hx, sizeof(hx), "%02x", bytes[bi]);
            key += hx;
          }
        }
        _winCollectLock.lock();
        if (_winKeysSeen.contains(key) == false && _winKeysSeen.size() < _winCollectMax)
        {
          _winKeysSeen.insert(key);
          const auto solution = r.getInputHistoryString();
          jaffarCommon::file::saveStringToFile(solution, _winCollectPrefix + key + ".sol");
          // No per-capture log line (screen churn): progress is reported as a collected/max
          // counter in the regular per-step block; the manifest records key/step per capture.
          {
            std::ofstream mf(_winCollectPrefix + "manifest.txt", std::ios::app);
            mf << "key " << key << " step " << r.getStepCount() << "\n";
          }
        }
        _winCollectLock.unlock();
      }
      _stepBestWinStateLock.lock();
      if (reward > _stepBestWinState.reward)
      {
        _stateDb->saveStateFromRunner(r, _stepBestWinState.stateData);
        _stepBestWinState.reward    = reward;
        _stepBestWinState.stepCount = r.getStepCount(); // record depth: the count is not serialized per-state
      }
      // Persist the winning input history DIRECTLY, now, when the runner still holds the exact path that
      // reached this win. The driver's post-search "best state" render loads a saved state and rebuilds
      // its history, which is unreliable for terminal win states (a win is never pushed to the DB, so the
      // best-state saver tracks the non-win frontier instead). This writes only on a strictly-better win
      // (rare, under the lock), so the file always holds the highest-reward winning solution found.
      if (reward > _bestWinSolutionReward)
      {
        _bestWinSolutionReward = reward;
        jaffarCommon::file::saveStringToFile(r.getInputHistoryString(), _winSolutionPath);
      }
      _stepBestWinStateLock.unlock();

      // Freeing up the state data
      JAFFAR_PROF_DECL(t7);
      _stateDb->returnFreeState(newStateData, threadId);
      JAFFAR_PROF_ACC(acc.returnFreeState, t7);

      // Returning a win result
      return inputResult_t::win;
    }

    // If this is a normal state and has possible inputs store it in the next state database
    if (stateType == Game::stateType_t::normal)
    {
      // Debug probe (JAFFAR_DEBUG_SAMPLE_RATE=N): log every Nth stored state's identity (depth,
      // floor reward, key game properties) and hard-check the reference-prune invariant on it.
      // Zero overhead unless the env var is set (checked once).
      {
        static const long sampleRate = []
        {
          const char* e = std::getenv("JAFFAR_DEBUG_SAMPLE_RATE");
          return e ? std::atol(e) : 0;
        }();
        if (sampleRate > 0)
        {
          static std::atomic<uint64_t> pushCounter{0};
          const auto                   n  = pushCounter.fetch_add(1, std::memory_order_relaxed);
          const float                  fr = r.getGame()->getFloorReward();
          const size_t                 cd = _currentStep + 1;
          if (_refPruneEnabled && cd < _refPruneTrace.size() && fr < _refPruneTrace[cd] - _refPruneTolerance)
            jaffarCommon::logger::log("[J+] SAMPLE-VIOLATION depth %lu floorReward %.6f < trace %.6f - tol %.1f\n", cd, fr, _refPruneTrace[cd], _refPruneTolerance);
          if (n % (uint64_t)sampleRate == 0)
          {
            char        props[512];
            int         pp      = 0;
            const char* names[] = {"Player Pos X", "Player Pos Y", "RNG State 1", "RNG State 2", "RNG State 3", "RNG State 4"};
            for (const auto* pn : names)
            {
              auto* prop = r.getGame()->findProperty(pn);
              if (prop == nullptr) continue;
              const auto* b = (const uint8_t*)prop->getPointer();
              for (size_t bi = 0; bi < prop->getSize() && pp < 480; bi++) pp += snprintf(props + pp, sizeof(props) - pp, "%02x", b[bi]);
              pp += snprintf(props + pp, sizeof(props) - pp, " ");
            }
            jaffarCommon::logger::log("[J+] SAMPLE depth %lu reward %.6f floor %.6f hash %016lX%016lX props %s\n", cd, reward, fr, hash.first, hash.second, props);
          }
        }
      }
      // If this is a normal state, push into the state database
      JAFFAR_PROF_DECL(t8);
      auto success = _stateDb->pushState(reward, r, newStateData);
      JAFFAR_PROF_ACC(acc.runnerStateSave, t8);

      // If pushing the state failed (e.g. serialization error), drop it and continue, keeping a counter
      if (success == false)
      {
        // Freeing up state memory
        JAFFAR_PROF_DECL(t9);
        _stateDb->returnFreeState(newStateData, threadId);
        JAFFAR_PROF_ACC(acc.returnFreeState, t9);

        // Returning dropped result by failed serialization
        return inputResult_t::droppedFailedSerialization;
      }
    }

    // Checking for manual saved solution is required
    const auto currentLastRuleIdx = r.getGame()->getSaveSolutionCurrentLastRuleIdx();
    if (r.getGame()->isSaveSolution() && currentLastRuleIdx > _manualSaveSolutionActiveLastRuleId)
    {
      // Grab lock
      _manualSaveSolutionLock.lock();

      // Do this only if reward is better
      if (reward > _manualSaveSolution.reward)
      {
        // Store path, data, and reward
        _stateDb->saveStateFromRunner(r, _manualSaveSolution.stateData);
        _manualSaveSolution.path             = r.getGame()->getSaveSolutionPath();
        _manualSaveSolution.reward           = reward;
        _manualSaveSolution.lastRuleIdx      = currentLastRuleIdx;
        _manualSaveSolution.stepCount        = r.getStepCount(); // record depth (the count is not serialized per-state)
        _manualSaveSolutionUpdatedLastRuleId = true;
      }

      // Release lock
      _manualSaveSolutionLock.unlock();
    }

    // If store succeeded, return a normal execution
    return inputResult_t::normal;
  }

  /// @brief Per-base-state-input-hash set of candidate inputs already detected, used to dedup candidate-input probing.
  jaffarCommon::concurrent::HashMap_t<jaffarCommon::hash::hash_t, jaffarCommon::concurrent::HashSet_t<InputSet::inputIndex_t>> _candidateInputsDetected;

  /// @brief Counter for the current step.
  size_t _currentStep = 0;

  /// @brief Collection of runners for the workers to use (one per thread).
  std::vector<std::unique_ptr<Runner>> _runners;

  /// @brief Thread-safe state database holding the current and next step's states.
  std::unique_ptr<jaffarPlus::StateDb> _stateDb;

  /// @brief The one shared input-history backing (e.g. the trie) shared by all worker runners; an opaque
  /// handle owned for the whole search. Null for the raw/none strategies, which have no shared structure.
  std::shared_ptr<void> _inputHistoryBacking;

  /// @brief Whether hashing is enabled. Games that cannot loop skip the hash DB to save memory and computation.
  bool   _hashDbEnabled;
  size_t _baseStateBatch; ///< Active base-state pull batch size ("Base State Batch Size").

  // Reference pinning + lookahead (see constructor). Anchors a reference lineage in the frontier and
  // rewards being 1..Lookahead frames ahead of it.
  bool                                    _refPinEnabled        = false; ///< Whether reference pinning is active.
  float                                   _refPinBonus          = 0.0f;  ///< Reward bonus for matching the reference at the state's own depth.
  size_t                                  _refPinLookahead      = 0;     ///< How many future depths to also match (being ahead).
  float                                   _refPinLookaheadBonus = 0.0f;  ///< Extra bonus per frame ahead of the reference.
  std::vector<jaffarCommon::hash::hash_t> _refPinHashes;                 ///< Reference state hash at each search depth.
  std::atomic<size_t>                     _refPinHits{0};                ///< Cumulative reference-pin hits (diagnostic).
  bool                                    _compactLog = false;           ///< "Log Verbosity": "Compact" collapses the per-step engine block to essentials.
  std::atomic<size_t>                     _refPinMaxDepthHit{0};         ///< Deepest reference depth a pin matched (diagnostic).
  std::vector<std::vector<uint8_t>>       _refStates;                    ///< Per-depth serialized reference states for EXACT pin verification (empty = hash-only pinning).
  std::atomic<size_t>                     _refPinSeenPreDedup{0};        ///< Reference-depth matches seen (diagnostic).
  std::unique_ptr<std::atomic<uint8_t>[]> _refPinnedAtDepth;             ///< Per reference depth: claimed(1)/unclaimed(0), so exactly one copy is pinned.

public:
  /// @brief Supplies per-depth serialized reference states for exact pin verification, and computes the
  ///        volatile-byte mask: driver-side captures carry different instance residue (unrestored buffer
  ///        bytes) than worker-produced states, so the mask is built empirically by re-producing a few
  ///        reference states through a WORKER runner and diffing. Verification compares outside the mask.
  void setReferenceStates(std::vector<std::vector<uint8_t>>&& states, const std::vector<std::string>& refInputs)
  {
    _refStates = std::move(states);
    if (_refStates.size() < 2 || refInputs.size() == 0) return;
    std::vector<uint8_t> scratch(_refStates[0].size());
    _refVolatileMask.assign(_refStates[0].size(), 0);
    // Probe depths spread across the whole span: the volatile region (audio ring residue) slides over
    // time, so distant depths expose different parts of it. Each hit is padded so the union covers the
    // sliding band.
    const size_t maxDepth   = std::min(_refStates.size() - 1, refInputs.size());
    const size_t probeCount = std::min<size_t>(32, maxDepth);
    // Every worker runner instance carries its own residue pattern -- probe across several instances
    // so the mask covers the union.
    const size_t runnersToProbe = std::min<size_t>(16, _runners.size());
    for (size_t ri = 0; ri < runnersToProbe; ri++)
      for (size_t pi = 0; pi < probeCount; pi++)
      {
        auto&        r = *_runners[ri];
        const size_t k = (pi * maxDepth) / probeCount;
        if (refInputs[k].empty()) continue;
        {
          jaffarCommon::deserializer::Contiguous d(_refStates[k].data(), _refStates[k].size());
          r.deserializeState(d);
        }
        r.advanceState(r.getGame()->getEmulator()->registerInput(refInputs[k]));
        {
          jaffarCommon::serializer::Contiguous s(scratch.data(), scratch.size());
          r.serializeState(s);
        }
        for (size_t i = 0; i < scratch.size(); i++)
          if (scratch[i] != _refStates[k + 1][i])
          {
            const size_t lo = i >= 128 ? i - 128 : 0;
            const size_t hi = std::min(i + 128, scratch.size() - 1);
            for (size_t j = lo; j <= hi; j++) _refVolatileMask[j] = 1;
          }
      }
    size_t total = 0;
    for (auto m : _refVolatileMask) total += m;
    jaffarCommon::logger::log("[J+] Reference pin exact-verification: volatile mask covers %lu / %lu bytes\n", total, _refVolatileMask.size());
  }

private:
  std::vector<uint8_t> _refVolatileMask; ///< 1 = byte is instance-residue (excluded from exact pin verification)

  /// @brief Thread-safe hash database used to detect repeated states.
  std::unique_ptr<jaffarPlus::HashDb> _hashDb;

  /// @brief Size of a single state as stored in the database, in bytes.
  size_t _stateSizeInDatabase;

  /// @brief Full self-contained serialized state size ([hot]+[history]) for standalone snapshot buffers.
  size_t _fullStateSize;

  std::mutex  _stepBestWinStateLock; ///< Guards updates to @ref _stepBestWinState.
  stateInfo_t _stepBestWinState;     ///< Best win state (by reward) found during the current step.
  /// Highest-reward win seen across ALL steps, and the file its input history is written to at detection
  /// time (see the win-state handling in computeState). Reliable even for terminal win states.
  float       _bestWinSolutionReward = -std::numeric_limits<float>::infinity();
  std::string _winSolutionPath       = "/tmp/jaffar.winsolution.sol"; ///< File the best win-state's input history is written to (see @ref _bestWinSolutionReward).

  // Storage for manually triggered save solutionm

  std::mutex           _manualSaveSolutionLock;              ///< Guards updates to @ref _manualSaveSolution.
  manualSaveSolution_t _manualSaveSolution;                  ///< Best manually saved solution for the current step.
  bool                 _manualSaveSolutionUpdatedLastRuleId; ///< Whether the manual-save last-rule id changed this step.
  ssize_t              _manualSaveSolutionActiveLastRuleId;  ///< Currently active manual-save last-rule id across steps.
  std::string          _manualSaveSolutionLastPath = "";     ///< Path of the most recently activated manual-save solution.

  // Checkpoint information
  std::atomic<size_t> _checkpointLevel;     ///< Highest checkpoint level reached so far.
  std::atomic<size_t> _checkpointTolerance; ///< Tolerance (in steps) associated with the current checkpoint level.
  std::atomic<size_t> _checkpointCutoff;    ///< Step index after which states below @ref _checkpointLevel are dropped.
  std::mutex          _checkpointMutex;     ///< Serializes checkpoint level rises (rare path).
  std::atomic<size_t> _stepMaxLevelStored;  ///< States stored this step whose level >= the global checkpoint level.

  //////////////// Statistics

  /// @brief Counter for states dropped due to lack of free states.
  std::atomic<size_t> _droppedStatesNoStorage;

  /// @brief Counter for states dropped due to failed serialization.
  std::atomic<size_t> _droppedStatesFailedSerialization;

  /// @brief Counter for states dropped due to not meeting the checkpoint.
  std::atomic<size_t> _droppedStatesCheckpoint;

  /// @brief Number of states dropped for falling below the reference reward trace (reference pruning).
  std::atomic<size_t> _droppedStatesBelowReference;

  /// @brief Counter for repeated states (detected via hash collision).
  std::atomic<size_t> _repeatedStates;

  /// @brief Counter for failed states (reached a point in the game considered a loss).
  std::atomic<size_t> _failedStates;

  /// @brief Counter for win states.
  std::atomic<size_t> _winStates;

  /// @brief Counter for normal states.
  std::atomic<size_t> _normalStates;

  std::atomic<size_t> _stepBaseStatesProcessed;  ///< Base states processed during the current step.
  std::atomic<size_t> _totalBaseStatesProcessed; ///< Base states processed across all steps so far.

  std::atomic<size_t> _stepNewStatesProcessed;  ///< New states processed during the current step.
  std::atomic<size_t> _totalNewStatesProcessed; ///< New states processed across all steps so far.

  //////////////// Timing

  /// @brief Overall running time of the current step, in microseconds.
  size_t _currentStepTime;

  std::vector<size_t> _threadStepTime;            ///< Per-thread running time of the current step, in microseconds.
  size_t              _maxThreadStepTime;         ///< Maximum per-thread step time for the current step.
  size_t              _maxThreadStepTimeThreadId; ///< Id of the thread with the maximum step time.

  /// @brief Per-thread accumulators for hot-loop timing/counters, reduced once per step.
  std::vector<threadAccumulator_t> _threadAccumulators;

  /// @brief Total running time so far, in microseconds.
  size_t _totalRunningTime;

  // Time spent advancing runner state per step
  std::atomic<size_t> _runnerStateAdvanceThreadRawTime;         ///< Summed per-thread runner-advance time for the step.
  std::atomic<size_t> _runnerStateAdvanceAverageTime;           ///< Per-thread-average runner-advance time for the step.
  std::atomic<size_t> _runnerStateAdvanceAverageCumulativeTime; ///< Cumulative per-thread-average runner-advance time.

  // Time spent loading states into the runner
  std::atomic<size_t> _runnerStateLoadThreadRawTime;         ///< Summed per-thread state-load time for the step.
  std::atomic<size_t> _runnerStateLoadAverageTime;           ///< Per-thread-average state-load time for the step.
  std::atomic<size_t> _runnerStateLoadAverageCumulativeTime; ///< Cumulative per-thread-average state-load time.

  // Time spent saving runner states into the state db
  std::atomic<size_t> _runnerStateSaveThreadRawTime;         ///< Summed per-thread state-save time for the step.
  std::atomic<size_t> _runnerStateSaveAverageTime;           ///< Per-thread-average state-save time for the step.
  std::atomic<size_t> _runnerStateSaveAverageCumulativeTime; ///< Cumulative per-thread-average state-save time.

  // Time spent calculating hash
  std::atomic<size_t> _calculateHashThreadRawTime;         ///< Summed per-thread hash-calculation time for the step.
  std::atomic<size_t> _calculateHashAverageTime;           ///< Per-thread-average hash-calculation time for the step.
  std::atomic<size_t> _calculateHashAverageCumulativeTime; ///< Cumulative per-thread-average hash-calculation time.

  // Time spent checking hash
  std::atomic<size_t> _checkHashThreadRawTime;         ///< Summed per-thread hash-checking time for the step.
  std::atomic<size_t> _checkHashAverageTime;           ///< Per-thread-average hash-checking time for the step.
  std::atomic<size_t> _checkHashAverageCumulativeTime; ///< Cumulative per-thread-average hash-checking time.

  // Rule checking time
  std::atomic<size_t> _ruleCheckingThreadRawTime;         ///< Summed per-thread rule-checking time for the step.
  std::atomic<size_t> _ruleCheckingAverageTime;           ///< Per-thread-average rule-checking time for the step.
  std::atomic<size_t> _ruleCheckingAverageCumulativeTime; ///< Cumulative per-thread-average rule-checking time.

  // Get free state time
  std::atomic<size_t> _getFreeStateThreadRawTime;         ///< Summed per-thread get-free-state time for the step.
  std::atomic<size_t> _getFreeStateAverageTime;           ///< Per-thread-average get-free-state time for the step.
  std::atomic<size_t> _getFreeStateAverageCumulativeTime; ///< Cumulative per-thread-average get-free-state time.

  // Return free state time
  std::atomic<size_t> _returnFreeStateThreadRawTime;         ///< Summed per-thread return-free-state time for the step.
  std::atomic<size_t> _returnFreeStateAverageTime;           ///< Per-thread-average return-free-state time for the step.
  std::atomic<size_t> _returnFreeStateAverageCumulativeTime; ///< Cumulative per-thread-average return-free-state time.

  // Reward calculation time
  std::atomic<size_t> _calculateRewardThreadRawTime;         ///< Summed per-thread reward-calculation time for the step.
  std::atomic<size_t> _calculateRewardAverageTime;           ///< Per-thread-average reward-calculation time for the step.
  std::atomic<size_t> _calculateRewardAverageCumulativeTime; ///< Cumulative per-thread-average reward-calculation time.

  // Get allowed inputs time
  std::atomic<size_t> _getAllowedInputsThreadRawTime;         ///< Summed per-thread get-allowed-inputs time for the step.
  std::atomic<size_t> _getAllowedInputsAverageTime;           ///< Per-thread-average get-allowed-inputs time for the step.
  std::atomic<size_t> _getAllowedInputsAverageCumulativeTime; ///< Cumulative per-thread-average get-allowed-inputs time.

  // Get candidate inputs time
  std::atomic<size_t> _getCandidateInputsThreadRawTime;         ///< Summed per-thread get-candidate-inputs time for the step.
  std::atomic<size_t> _getCandidateInputsAverageTime;           ///< Per-thread-average get-candidate-inputs time for the step.
  std::atomic<size_t> _getCandidateInputsAverageCumulativeTime; ///< Cumulative per-thread-average get-candidate-inputs time.

  // Advance Hash DB time
  std::atomic<size_t> _advanceHashDbThreadRawTime;         ///< Serially-measured hash-DB advance time for the step.
  std::atomic<size_t> _advanceHashDbAverageTime;           ///< Hash-DB advance time reported for the step.
  std::atomic<size_t> _advanceHashDbAverageCumulativeTime; ///< Cumulative hash-DB advance time.

  // Advance State DB time
  std::atomic<size_t> _advanceStateDbThreadRawTime;         ///< Serially-measured state-DB advance time for the step.
  std::atomic<size_t> _advanceStateDbAverageTime;           ///< State-DB advance time reported for the step.
  std::atomic<size_t> _advanceStateDbAverageCumulativeTime; ///< Cumulative state-DB advance time.

  // Popping states from the State DB time
  std::atomic<size_t> _popBaseStateDbThreadRawTime;         ///< Summed per-thread base-state pop time for the step.
  std::atomic<size_t> _popBaseStateDbAverageTime;           ///< Per-thread-average base-state pop time for the step.
  std::atomic<size_t> _popBaseStateDbAverageCumulativeTime; ///< Cumulative per-thread-average base-state pop time.

  size_t _subTotalAverageTime;           ///< Sum of all per-operation average times for the current step.
  size_t _subTotalAverageCumulativeTime; ///< Sum of all per-operation cumulative average times.
};

} // namespace jaffarPlus
