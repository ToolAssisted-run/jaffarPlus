#pragma once

/**
 * @file stateDb.hpp
 * @brief Per-NUMA-domain database of serialized game states, with reward-ordered queues that feed
 *        the search one step at a time and a free-slot pool the workers draw from.
 */

#include "inputHistory/inputHistory.hpp"
#include "numa.hpp"
#include <cstdlib>
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/concurrent.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/parallel.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <memory>
#include <utmpx.h>

/// @brief Alignment boundary (in bytes) each stored state is padded up to, for vectorized access and false-sharing avoidance.
#define _JAFFAR_STATE_PADDING_BYTES 16

namespace jaffarPlus
{

/**
 * @brief Stores serialized game states across the machine's NUMA domains and serves them to the
 *        search engine in reward-ordered, per-step batches.
 *
 * @details Each NUMA domain owns a contiguous slab of state slots (allocated on that node), a
 * free-slot queue of unused slots, a reward-ordered "next" queue that collects states produced
 * during a step, and a "current" queue that the workers drain during the following step. The queue
 * lifecycle is: workers @ref pushState into the next queue; @ref advanceStep moves the next queue's
 * pointers (best-reward first) into the current queue and clears the next queue; workers then
 * @ref popState / @ref popStates the current queue. Slots are obtained with @ref getFreeState and
 * recycled with @ref returnFreeState, both fronted by a per-thread free-slot cache. The per-domain
 * queues are only allocated for NUMA domains that own a delegate worker thread; other domains hold
 * null queue pointers, which the access paths skip.
 */
class StateDb
{
public:
  /**
   * @brief Constructs the state database and reads its maximum size from configuration.
   * @param r      The runner whose states will be serialized into and deserialized out of the database.
   * @param config Configuration object; the "Max Size (Mb)" field sets the database's maximum size.
   *
   * @details Reads "Max Size (Mb)" from @p config. If the environment variable
   * `JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB` is set, its value overrides that maximum.
   */
  StateDb(Runner& r, const nlohmann::json& config) : _runner(&r)
  {
    // Initialization message
    jaffarCommon::logger::log("[J+] Initializing State Database...\n");

    ///////// Getting original state sizes from the runner

    // Checking maximum db sizes per each numa group
    _maxSizeMb = jaffarCommon::json::getNumber<size_t>(config, "Max Size (Mb)");

    // Overriding if provided
    if (auto* value = std::getenv("JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB")) _maxSizeMb = std::stoul(value);
  }

  /// @brief Configured maximum state-DB footprint in bytes (used by the engine's combined RAM guard).
  __INLINE__ size_t getMaxBudgetBytes() const { return _maxSizeMb * 1024UL * 1024UL; }

  /**
   * @brief Frees the per-NUMA current- and next-state queues allocated in @ref initialize().
   *
   * @details Iterates over both queue vectors and deletes each entry. Only delegate domains hold a
   * non-null queue; the remaining entries are nullptr, which delete ignores.
   */
  ~StateDb()
  {
    // Free the per-NUMA queues allocated with `new` in initialize() (one per delegate domain; the
    // other entries are nullptr, which delete safely ignores).
    for (auto* queue : _numaCurrentStateQueues) delete queue;
    for (auto* queue : _numaNextStateQueues) delete queue;
  }

  /**
   * @brief Allocates and initializes the per-NUMA state slabs, queues, and per-thread caches.
   *
   * @details Queries the runner's state size and pads it up to @ref _JAFFAR_STATE_PADDING_BYTES.
   * Allocates per-thread statistics counters and free-slot caches. Splits the configured maximum
   * size evenly across NUMA domains, verifies each domain has enough free memory (throwing
   * otherwise), and derives the per-domain maximum state counts. For each NUMA domain that owns a
   * delegate thread it creates the current-state DrainBuffer (reserved to the domain's state count)
   * and the next-state reward-ordered multimap. It then allocates the per-domain contiguous state
   * slab on that node, touches one byte per page to fault it in, and fills the domain's free-state
   * queue with pointers to every slot.
   * @throws A runtime error if a domain's requested memory exceeds its free space, if a domain's
   *         state count exceeds the DrainBuffer's 32-bit capacity limit, or if a NUMA slab
   *         allocation fails.
   */
  __INLINE__ void initialize()
  {
    // Getting game state size. The hot slot holds only the game+emulator state the search hashes and
    // advances each step; the per-state path (input history + step counter) goes to a parallel cold slab.
    _ih           = _runner->getInputHistory(); // reference strategy for per-slot manager ops
    _stateSizeRaw = _runner->getHotStateSize();
    _histSize     = _runner->getHistorySize();
    // Standalone buffers hold the FULL self-contained serialize ([hot]+[full history]). In trie mode the
    // cold slot's history (a node id) is smaller than the full history (the reconstructed bit-packed
    // buffer), so the full size is the runner's own getStateSize(), not _stateSizeRaw + _histSize.
    _fullStateSizeBytes = _runner->getStateSize();

    // States are stored raw (uncompressed). We align each state to a padding boundary to favor
    // vectorized access and prevent false sharing.
    _stateSize = _stateSizeRaw;

    // Padding state size to enforce alignment and prevent false sharing as much as possible
    const size_t baseStateSize = _stateSize;
    if (_stateSize % _JAFFAR_STATE_PADDING_BYTES != 0) _stateSize = ((baseStateSize / _JAFFAR_STATE_PADDING_BYTES) + 1) * _JAFFAR_STATE_PADDING_BYTES;

    // Padding is the difference between the aligned state size and the raw one
    _stateSizePadding = _stateSize - baseStateSize;

    // Allocating/zeroing the per-thread statistics counters. These were previously global atomics
    // bumped on every pop/get-free; at high thread count that single-cache-line ping-pong cost more
    // than the work it measured. They are now plain per-thread counters (indexed by the dense OpenMP
    // thread id) reduced only when printInfo() is called.
    _statCounters.assign(_threadCount, statCounters_t{});

    // Allocating/zeroing the per-thread free-slot caches (empty at start)
    _freeStateCache.assign(_threadCount, freeStateCache_t{});

    // Splitting state database equally among numa domains
    size_t stateDbSizePerNUMA = std::ceil((_maxSizeMb * 1024 * 1024) / _numaCount);
    for (int i = 0; i < _numaCount; i++) _maxSizePerNuma.push_back(stateDbSizePerNUMA);
    // (local, jaffanator2 shared with another job: env JAFFAR_NUMA_WEIGHTS="w0,w1,..." splits the database by those
    // weights instead, e.g. a domain another process has filled gets a token share)
    if (const char* w = std::getenv("JAFFAR_NUMA_WEIGHTS"))
    {
      std::vector<double> ws; std::string str(w); size_t pos = 0;
      while (pos <= str.size()) { size_t e = str.find(',', pos); if (e == std::string::npos) e = str.size(); ws.push_back(std::stod(str.substr(pos, e - pos))); pos = e + 1; }
      if ((int)ws.size() == _numaCount)
      {
        double sum = 0; for (auto x : ws) sum += x;
        for (int i = 0; i < _numaCount; i++) _maxSizePerNuma[i] = (size_t)((double)_maxSizeMb * 1024.0 * 1024.0 * ws[i] / sum);
      }
    }

    // Getting maximum allocatable memory in each NUMA domain
    std::vector<size_t> maxFreeMemoryPerNuma(_numaCount);
    for (int i = 0; i < _numaCount; i++)
    {
      size_t freeMemory = 0;
      numa_node_size64(i, (long long*)&freeMemory);
      maxFreeMemoryPerNuma[i] = freeMemory;
    }

    // Checking if this is enough memory to satisfy requirement
    for (int i = 0; i < _numaCount; i++)
      if (_maxSizePerNuma[i] > (size_t)maxFreeMemoryPerNuma[i])
        JAFFAR_THROW_RUNTIME("The requested memory (%lu) for NUMA domain %d exceeds its available free space (%lu)\n", _maxSizePerNuma[i], i, maxFreeMemoryPerNuma[i]);

    // Getting maximum number of states for each NUMA domain
    _maxStatesPerNuma.resize(_numaCount);
    _currentStatesPerNuma.resize(_numaCount);
    // The per-state memory footprint is the padded hot slot plus its cold history slot, so the budget
    // covers both slabs and the state count stays ~unchanged versus storing them together.
    for (int i = 0; i < _numaCount; i++) _maxStatesPerNuma[i] = std::max(_maxSizePerNuma[i] / (_stateSize + _histSize), 1ul);

    // Getting totals for statistics
    _maxSize   = 0;
    _maxStates = 0;
    for (int i = 0; i < _numaCount; i++)
    {
      _maxSize += _maxSizePerNuma[i];
      _maxStates += _maxStatesPerNuma[i];
    }

    // Allocating state databases in the correponding NUMA domains
    _numaCurrentStateQueues.resize(_numaCount);
    _numaNextStateQueues.resize(_numaCount);
    // The current-state queue is a DrainBuffer: it is filled single-threaded in advanceStep() and
    // then drained concurrently from both ends by the workers, never both at once (the step barrier
    // separates the phases). Its capacity is the NUMA domain's full state count -- it can never hold
    // more current states than that. The claim counters are 32-bit, so guard the capacity.
    for (int i = 0; i < _numaCount; i++)
      if (_maxStatesPerNuma[i] > (size_t)UINT32_MAX)
        JAFFAR_THROW_RUNTIME("State count per NUMA domain (%lu) exceeds the DrainBuffer capacity limit (%u)\n", _maxStatesPerNuma[i], UINT32_MAX);

    JAFFAR_PARALLEL
    {
      if (_myThreadId == _numaDelegateThreadId[_preferredNumaDomain])
      {
        auto* currentQueue = new jaffarCommon::concurrent::DrainBuffer<void*>();
        currentQueue->reserve(_maxStatesPerNuma[_preferredNumaDomain]);
        _numaCurrentStateQueues[_preferredNumaDomain] = currentQueue;
        _numaNextStateQueues[_preferredNumaDomain]    = new jaffarCommon::concurrent::concurrentMultimap_t<float, void*>();
      }
    }

    // Getting number of bytes to reserve for each NUMA domain
    _allocableBytesPerNuma.resize(_numaCount);
    for (int i = 0; i < _numaCount; i++) _allocableBytesPerNuma[i] = _maxStatesPerNuma[i] * _stateSize;

    // Creating internal buffers, one per NUMA domain
    _internalBuffersStart.resize(_numaCount);
    _internalBuffersEnd.resize(_numaCount);
    for (int i = 0; i < _numaCount; i++)
    {
      _internalBuffersStart[i] = (uint8_t*)numa_alloc_onnode(_allocableBytesPerNuma[i], i);
      if (_internalBuffersStart[i] == NULL) JAFFAR_THROW_RUNTIME("Error trying to allocate memory for numa domain %d\n", i);
      _internalBuffersEnd[i] = &_internalBuffersStart[i][_allocableBytesPerNuma[i]];
    }

    // Creating the parallel history (cold) slabs, one per NUMA domain, on the same node. Mirrors the
    // state-slab grid: the cold slot for state index i is at _histBuffersStart[numa] + i*_histSize.
    // _histSize is 0 for the "None" strategy (no path stored, and the step counter is now runner-owned, not
    // per-state): there is no cold slab to allocate, and all cold-slot ops are no-ops over 0 bytes.
    _histBuffersStart.resize(_numaCount, nullptr);
    if (_histSize > 0)
      for (int i = 0; i < _numaCount; i++)
      {
        _histBuffersStart[i] = (uint8_t*)numa_alloc_onnode(_maxStatesPerNuma[i] * _histSize, i);
        if (_histBuffersStart[i] == NULL) JAFFAR_THROW_RUNTIME("Error trying to allocate history slab for numa domain %d\n", i);
      }

    // Getting system's page size (typically 4K but it may change in the future)
    const size_t pageSize = sysconf(_SC_PAGESIZE);

    // Initializing the internal buffers (touch one byte per page to fault them in on the owning node)
    for (int numaNodeIdx = 0; numaNodeIdx < _numaCount; numaNodeIdx++) JAFFAR_PARALLEL_FOR
    for (size_t i = 0; i < _allocableBytesPerNuma[numaNodeIdx]; i += pageSize) _internalBuffersStart[numaNodeIdx][i] = 1;

    // Likewise fault in the history slabs (one byte per page).
    for (int numaNodeIdx = 0; numaNodeIdx < _numaCount; numaNodeIdx++) JAFFAR_PARALLEL_FOR
    for (size_t i = 0; i < _maxStatesPerNuma[numaNodeIdx] * _histSize; i += pageSize) _histBuffersStart[numaNodeIdx][i] = 1;

    // Let the input-history strategy initialize each fresh cold slot (e.g. the trie marks its node id as
    // empty so a recycled slot is never mistaken for holding a node). No-op for raw/none. Skipped entirely
    // when there is no cold slab (_histSize == 0, the "None" strategy).
    if (_histSize > 0)
      for (int numaNodeIdx = 0; numaNodeIdx < _numaCount; numaNodeIdx++) JAFFAR_PARALLEL_FOR
    for (size_t s = 0; s < _maxStatesPerNuma[numaNodeIdx]; s++) _ih->initColdSlot(&_histBuffersStart[numaNodeIdx][s * _histSize]);

    // Adding the state pointers to the free state queues
    _freeStateQueues.resize(_numaCount);
    JAFFAR_PARALLEL
    {
      if (_myThreadId == _numaDelegateThreadId[_preferredNumaDomain])
      {
        _freeStateQueues[_preferredNumaDomain] = std::make_unique<jaffarCommon::concurrent::atomicQueue_t<void*>>(_maxStatesPerNuma[_preferredNumaDomain]);
        for (size_t i = 0; i < _maxStatesPerNuma[_preferredNumaDomain]; i++)
          _freeStateQueues[_preferredNumaDomain]->try_push((void*)&_internalBuffersStart[_preferredNumaDomain][i * _stateSize]);
      }
    }
  }

  /**
   * @brief Serializes the runner's state (raw, uncompressed) into the given slot.
   * @param r        The runner whose state is serialized.
   * @param statePtr Destination slot the state is written into.
   * @return The number of bytes written.
   */
  __INLINE__ size_t saveStateFromRunner(Runner& r, void* statePtr)
  {
    // Full, self-contained serialization ([hot][history]) into a standalone buffer (NOT a slab slot):
    // used for the best/worst/win/manual snapshots kept outside the NUMA slabs, which have no cold mirror.
    jaffarCommon::serializer::Contiguous s(statePtr, _fullStateSizeBytes);
    r.serializeState(s);
    return s.getOutputSize();
  }

  /**
   * @brief Serializes the runner into a state-database slab slot: hot state into the slot, path (input
   * history + step counter) into the parallel cold slot. Use only with slab-slot pointers.
   */
  __INLINE__ size_t saveStateToSlot(Runner& r, void* statePtr)
  {
    jaffarCommon::serializer::Contiguous s(statePtr, _stateSizeRaw);
    r.serializeHotState(s);
    jaffarCommon::serializer::Contiguous h(getHistoryPtr(statePtr), _histSize);
    r.serializeHistory(h);
    return s.getOutputSize();
  }

  /**
   * @brief Gathers a slab slot's hot state and its cold history mirror into a contiguous full-state
   * buffer ([hot][history], matching @ref saveStateFromRunner's layout). Used to snapshot the best/worst
   * state out of the slabs into standalone storage that @ref loadStateIntoRunner can later restore.
   */
  __INLINE__ void captureSlotToBuffer(const void* slotPtr, void* buffer)
  {
    // Hot state copies verbatim; the input-history strategy converts its cold slot into the buffer's
    // self-contained full history region (raw/none: copy; trie: reconstruct the path from its node id).
    memcpy(buffer, slotPtr, _stateSizeRaw);
    _ih->captureColdToFull(getHistoryPtr(const_cast<void*>(slotPtr)), (uint8_t*)buffer + _stateSizeRaw);
  }

  /// @brief Full self-contained serialized state size ([hot][history]); for standalone state buffers.
  __INLINE__ size_t getFullStateSize() const { return _fullStateSizeBytes; }

  /**
   * @brief Serializes the runner's state into the given slot and inserts it into the next-state queue.
   * @param reward   The reward used to order the state within its NUMA domain's next-state queue.
   * @param r        The runner whose state is serialized.
   * @param statePtr The slot to serialize into and insert; determines the target NUMA domain.
   * @return true if the state was serialized and inserted; false if serialization threw a runtime error.
   * @throws A runtime error if @p statePtr is null (interpreted as having run out of free states).
   *
   * @details Determines the slot's owning NUMA domain via @ref getStateNumaDomain and inserts the
   * {reward, statePtr} pair into that domain's next-state queue.
   */
  __INLINE__ bool pushState(const float reward, Runner& r, void* statePtr)
  {
    // Check that we got a free state (we did not overflow state memory)
    if (statePtr == nullptr) JAFFAR_THROW_RUNTIME("Provided a null state -- probably ran out of free states\n");

    // Encoding internal runner state into the state pointer (slab slot: hot into slot, path into cold slab)
    try
    {
      // Getting state from the runner
      saveStateToSlot(r, statePtr);
    }
    catch (const std::runtime_error& x)
    {
      // If failed return false
      return false;
    }

    // Inserting new state into the next state database
    // Getting the corresponding numa domain for this state
    int targetNumaIdx = getStateNumaDomain(statePtr);

    // Inserting new state into the next state database
    _numaNextStateQueues[targetNumaIdx]->insert({reward, statePtr});

    // If succeeded, return true
    return true;
  }

  /**
   * @brief Deserializes a stored state (raw, uncompressed) from a slot into the runner.
   * @param r        The runner the state is loaded into.
   * @param statePtr The slot the state is read from.
   */
  __INLINE__ void loadStateIntoRunner(Runner& r, const void* statePtr)
  {
    // Full, self-contained deserialization ([hot][history]) from a standalone buffer (NOT a slab slot).
    jaffarCommon::deserializer::Contiguous d(statePtr, _fullStateSizeBytes);
    r.deserializeState(d);
  }

  /**
   * @brief Deserializes a state-database slab slot into the runner: hot state from the slot, path from
   * the parallel cold slot. Use only with slab-slot pointers.
   */
  __INLINE__ void loadStateFromSlot(Runner& r, const void* statePtr)
  {
    jaffarCommon::deserializer::Contiguous d(statePtr, _stateSizeRaw);
    r.deserializeHotState(d);
    jaffarCommon::deserializer::Contiguous h(getHistoryPtr(statePtr), _histSize);
    r.deserializeHistory(h);
  }

  /**
   * @brief Logs database occupancy, sizing, per-NUMA-domain figures, and reduced statistics counters.
   *
   * @details Logs current vs. maximum state counts and sizes, raw and padded per-state sizes, and the
   * first and last NUMA domains' state counts and sizes. Reduces the per-thread statistics counters
   * into totals and logs database-pop NUMA locality rates, free-slot cache hit/absorb counts, free-state
   * acquisition rates (including state stealing), and the average NUMA distance per access.
   */
  // Function to print relevant information
  __INLINE__ void printInfo() const
  {
    const size_t currentStateCount = getStateCount();
    const size_t currentStateBytes = currentStateCount * (_stateSize + _histSize); // hot slab + cold history slab

    jaffarCommon::logger::log("[J+]  + Current State Count:                      %lu (%f Mstates) /  %lu (%f Mstates) Max / %5.2f%% Full\n", currentStateCount,
                              (double)currentStateCount * 1.0e-6, _maxStates, (double)_maxStates * 1.0e-6, 100.0 * (double)currentStateCount / (double)_maxStates);
    jaffarCommon::logger::log("[J+]  + Current State Size:                       %.3f Mb (%.6f Gb) / %.3f Mb (%.6f Gb) Max\n", (double)currentStateBytes / (1024.0 * 1024.0),
                              (double)currentStateBytes / (1024.0 * 1024.0 * 1024.0), (double)_maxSize / (1024.0 * 1024.0), (double)_maxSize / (1024.0 * 1024.0 * 1024.0));
    jaffarCommon::logger::log("[J+]  + State Size Raw:                           %lu bytes (hot %lu + cold/history %lu)\n", _stateSizeRaw + _histSize, _stateSizeRaw, _histSize);
    jaffarCommon::logger::log("[J+]  + State Size in DB:                         %lu bytes (hot %lu + %lu padding to %u, cold %lu)\n", _stateSize + _histSize, _stateSize,
                              _stateSizePadding, _JAFFAR_STATE_PADDING_BYTES, _histSize);
    const size_t historyMem = _ih->getApproxMemoryBytes(); // shared-structure memory (e.g. the trie); 0 for raw/none
    if (historyMem > 0)
    {
      const double MB        = 1024.0 * 1024.0;
      const double sharedMb  = historyMem / MB;                                                        // shared structure (trie nodes)
      const double coldMb    = currentStateCount * (double)_histSize / MB;                             // per-state cold slot (node id)
      const double bitpackMb = currentStateCount * (double)(_fullStateSizeBytes - _stateSizeRaw) / MB; // raw bit-packed equivalent
      jaffarCommon::logger::log("[J+]  + Input History (shared):                   %.1f Mb shared + %.1f Mb cold slots = %.1f Mb total (raw would be %.1f Mb)\n", sharedMb, coldMb,
                                sharedMb + coldMb, bitpackMb);
    }

    // Only print the first and the last
    for (int i = 0; i < _numaCount; i++)
      if (i == 0 || i == _numaCount - 1)
        jaffarCommon::logger::log("[J+]  + NUMA Domain %3d                States: %lu (Max: %lu), Size: %.3f Mb (%.6f Gb)\n", i, _currentStatesPerNuma[i], _maxStatesPerNuma[i],
                                  (double)_maxSizePerNuma[i] / (1024.0 * 1024.0), (double)_maxSizePerNuma[i] / (1024.0 * 1024.0 * 1024.0));

    // Reducing the per-thread statistics counters into totals (off the hot path, only when printing)
    size_t localDatabaseState = 0, nonLocalDatabaseState = 0, databaseStateNotFound = 0;
    size_t localFreeState = 0, nonLocalFreeState = 0, stealingFreeState = 0, freeStateNotFound = 0;
    size_t distanceAccumulator = 0;
    size_t freeStateCacheHit = 0, freeStateCacheReturn = 0;
    for (const auto& sc : _statCounters)
    {
      localDatabaseState += sc.localDatabaseState;
      nonLocalDatabaseState += sc.nonLocalDatabaseState;
      databaseStateNotFound += sc.databaseStateNotFound;
      localFreeState += sc.localFreeState;
      nonLocalFreeState += sc.nonLocalFreeState;
      stealingFreeState += sc.stealingFreeState;
      freeStateNotFound += sc.freeStateNotFound;
      distanceAccumulator += sc.distanceAccumulator;
      freeStateCacheHit += sc.freeStateCacheHit;
      freeStateCacheReturn += sc.freeStateCacheReturn;
    }

    size_t totalDatabaseStatesRequested = nonLocalDatabaseState + localDatabaseState + databaseStateNotFound;
    jaffarCommon::logger::log("[J+] + Database Popping State Rates:\n");
    jaffarCommon::logger::log("[J+]  + Numa Locality Success Rate:               %5.3f%%\n", 100.0 * (double)localDatabaseState / (double)totalDatabaseStatesRequested);
    jaffarCommon::logger::log("[J+]  + Numa Locality Fail Rate:                  %5.3f%%\n", 100.0 * (double)nonLocalDatabaseState / (double)totalDatabaseStatesRequested);
    jaffarCommon::logger::log("[J+]  + Numa No DB State Found Rate:              %5.3f%%\n", 100.0 * (double)databaseStateNotFound / (double)totalDatabaseStatesRequested);

    // Thread-local free-slot cache effectiveness: fraction of all getFreeState / returnFreeState
    // calls served locally (and thus kept off the shared free-state queue).
    const size_t totalGets    = freeStateCacheHit + nonLocalFreeState + localFreeState + stealingFreeState + freeStateNotFound;
    const size_t totalReturns = freeStateCacheReturn + /* shared-queue spills are not separately counted */ 0;
    jaffarCommon::logger::log("[J+] + Free-Slot Cache:\n");
    jaffarCommon::logger::log("[J+]  + Get Cache Hit Rate:                       %5.3f%% (%lu hits)\n",
                              totalGets == 0 ? 0.0 : 100.0 * (double)freeStateCacheHit / (double)totalGets, freeStateCacheHit);
    jaffarCommon::logger::log("[J+]  + Return Cache Absorb Count:                %lu\n", freeStateCacheReturn);
    (void)totalReturns;

    size_t totalFreeStatesRequested = nonLocalFreeState + localFreeState + freeStateNotFound + stealingFreeState;
    jaffarCommon::logger::log("[J+] + Get Free State Rates (shared-queue only):\n");
    jaffarCommon::logger::log("[J+]  + Numa Locality Success Rate:               %5.3f%%\n", 100.0 * (double)localFreeState / (double)totalFreeStatesRequested);
    jaffarCommon::logger::log("[J+]  + Numa Locality Fail Rate:                  %5.3f%%\n", 100.0 * (double)nonLocalFreeState / (double)totalFreeStatesRequested);
    jaffarCommon::logger::log("[J+]  + State DB Stealing Rate:                   %5.3f%%\n", 100.0 * (double)stealingFreeState / (double)totalFreeStatesRequested);
    jaffarCommon::logger::log("[J+]  + Numa No Free State Found Rate:            %5.3f%%\n", 100.0 * (double)freeStateNotFound / (double)totalFreeStatesRequested);

    size_t NUMAAccessCount = nonLocalDatabaseState + localDatabaseState + nonLocalFreeState + localFreeState + stealingFreeState;
    jaffarCommon::logger::log("[J+]  + Average NUMA Distance:                    %lu / %lu = %5.3f\n", distanceAccumulator, NUMAAccessCount,
                              (double)distanceAccumulator / (double)NUMAAccessCount);
  }

  /**
   * @brief Obtains a free state slot for the calling thread to write a new state into.
   * @param threadId The calling thread's dense OpenMP id, used to index its cache and statistics counters.
   * @return A pointer to a free slot, or nullptr if none could be obtained (the state is then discarded).
   *
   * @details First serves from this thread's free-slot cache (LIFO) when non-empty. Otherwise it
   * scans NUMA domains in this thread's preference order and tries to pop from each domain's
   * free-state queue. If all free-state queues are empty, it scans again and steals a slot from the
   * back (worst state) of each domain's current-state queue. Updates the thread's NUMA-distance and
   * locality/stealing statistics on success.
   */
  __INLINE__ void* getFreeState(const size_t threadId)
  {
    auto& sc = _statCounters[threadId];

    // First, try this thread's own free-slot cache (a "magazine"). A slot this worker freed earlier
    // is served straight back to it with no shared-queue atomic and no NUMA lookup -- and it is the
    // hottest, most NUMA-local memory available (LIFO). This recycling is where the win comes from:
    // a worker frees ~1 slot and requests several per base state, so each free typically satisfies a
    // subsequent request locally, removing it from both the get and the return shared-queue traffic.
    auto& cache = _freeStateCache[threadId];
    if (cache.count > 0)
    {
      sc.freeStateCacheHit++;
      return cache.slots[--cache.count];
    }

    // Storage for the new free state space
    void* stateSpace;

    // Trying to get free space for a new state
    for (size_t i = 0; i < (size_t)_numaCount; i++)
    {
      const auto numaIdx = _numaPreferenceMatrix[_preferredNumaDomain][i];
      // Skip NUMA domains with no worker thread: their queues are never allocated (see initialize()).
      if (_freeStateQueues[numaIdx] == nullptr) continue;
      bool success = _freeStateQueues[numaIdx]->try_pop(stateSpace);

      // If successful, return the pointer immediately
      if (success == true)
      {
        sc.distanceAccumulator += _numaDistanceMatrix[_preferredNumaDomain][numaIdx];
        if (numaIdx == (size_t)_preferredNumaDomain)
          sc.localFreeState++;
        else
          sc.nonLocalFreeState++;
        return stateSpace;
      }
    }

    // If failed, then try to get it from the back of my numa-specific queues. Looking for the worst state possible

    // Trying to get free space for a new state
    for (size_t i = 0; i < (size_t)_numaCount; i++)
    {
      const auto numaIdx = _numaPreferenceMatrix[_preferredNumaDomain][i];
      if (_numaCurrentStateQueues[numaIdx] == nullptr) continue;
      bool success = _numaCurrentStateQueues[numaIdx]->pop_back_get(stateSpace);

      // If successful, return the pointer immediately
      if (success == true)
      {
        sc.distanceAccumulator += _numaDistanceMatrix[_preferredNumaDomain][numaIdx];
        sc.stealingFreeState++;
        return stateSpace;
      }
    }

    // Otherwise, return a null pointer. The state will be discarded
    sc.freeStateNotFound++;
    return nullptr;
  }

  /**
   * @brief Finds the NUMA domain whose slab contains the given state slot.
   * @param statePtr The state slot to locate.
   * @return The index of the owning NUMA domain.
   * @throws A runtime error if the pointer does not fall within any domain's slab.
   */
  __INLINE__ int getStateNumaDomain(void* const statePtr)
  {
    for (int i = 0; i < _numaCount; i++)
      if (isStateInNumaDomain(statePtr, i)) return i;

    // Check for error
    JAFFAR_THROW_RUNTIME("Did not find the corresponding numa domain for the provided state pointer. This must be a bug in Jaffar\n");
  }

  /**
   * @brief Returns the cold history slot mirroring the given hot state slot (same NUMA domain + index).
   * @param statePtr A hot state-slab slot pointer.
   * @return Pointer into the parallel history slab for the same slot index.
   * @details The hot slabs are fixed grids of @ref _stateSize slots, so the slot index is plain pointer
   * arithmetic and the cold slab is addressed at the same index with @ref _histSize stride.
   */
  __INLINE__ void* getHistoryPtr(const void* const statePtr)
  {
    const int    numa = getStateNumaDomain(const_cast<void*>(statePtr));
    const size_t idx  = ((const uint8_t*)statePtr - _internalBuffersStart[numa]) / _stateSize;
    return _histBuffersStart[numa] + idx * _histSize;
  }

  /**
   * @brief Tests whether a state slot lies within a given NUMA domain's slab.
   * @param statePtr     The state slot to test.
   * @param numaDomainId The NUMA domain whose slab is checked.
   * @return true if the pointer falls within the domain's [start, end] slab range, false otherwise.
   */
  __INLINE__ bool isStateInNumaDomain(void* const statePtr, const int numaDomainId)
  {
    return statePtr >= _internalBuffersStart[numaDomainId] && statePtr <= _internalBuffersEnd[numaDomainId];
  }

  /**
   * @brief Returns a state slot to the free pool for later reuse.
   * @param statePtr The slot being freed.
   * @param threadId The calling thread's dense OpenMP id, used to index its cache and statistics counters.
   * @throws A runtime error if the slot fails to push onto its owning domain's free-state queue.
   *
   * @details Stores the slot in this thread's free-slot cache when it has room (capped at
   * @ref FREE_STATE_CACHE_CAPACITY). When the cache is full, it spills the slot to the shared
   * free-state queue of the slot's owning NUMA domain (found via @ref getStateNumaDomain).
   */
  __INLINE__ void returnFreeState(void* const statePtr, const size_t threadId)
  {
    // Let the input-history strategy release any shared resource this slot held (the trie frees its path
    // node, recycling into this worker's own contention-free shard). No-op for raw/none.
    _ih->releaseColdSlot(getHistoryPtr(statePtr), threadId);

    // Return the slot to this thread's own free-slot cache when there is room, so the next
    // getFreeState() on this thread can reuse it without touching the shared queue (no atomic, no
    // NUMA scan). The cache is bounded so a worker can never hoard more than a negligible number of
    // slots from the shared pool (FREE_STATE_CACHE_CAPACITY * threadCount total).
    auto& cache = _freeStateCache[threadId];
    if (cache.count < FREE_STATE_CACHE_CAPACITY)
    {
      _statCounters[threadId].freeStateCacheReturn++;
      cache.slots[cache.count++] = statePtr;
      return;
    }

    // Cache full: spill to the shared free-state queue of the slot's owning NUMA domain
    const auto numaIdx = getStateNumaDomain(statePtr);

    // Pushing the state in the corresponding queue
    bool success = _freeStateQueues[numaIdx]->try_push(statePtr);

    // Check for success
    if (success == false) JAFFAR_THROW_RUNTIME("Failed on pushing free state back. This must be a bug in Jaffar\n");
  }

  /**
   * @brief Moves each NUMA domain's next-state queue into its current-state queue, best reward first,
   *        and clears the next-state queue.
   *
   * @details Runs in parallel; in each domain only the delegate thread acts. It records the domain's
   * next-state count, clears the current-state buffer (drained during the previous step), then
   * traverses the reward-ordered next-state map in descending-reward order, appending each pointer to
   * the current-state buffer. After draining it clears (frees) the whole next-state map at once. It
   * then updates the cross-NUMA best and worst state pointers under @ref _workMutex from the first
   * (best) and last (worst) entries seen.
   */
  __INLINE__ void advanceStep()
  {
    // // Resetting NUMA distance accumulator
    // _numaDistanceAccumulator = 0;

    // Calculation of best and worst states
    float bestStateReward  = std::numeric_limits<float>::lowest();
    float worstStateReward = std::numeric_limits<float>::max();

    // Copying state pointers into the numa-specific queues
    JAFFAR_PARALLEL
    {
      // Only process if I am the delegate
      if (_myThreadId == _numaDelegateThreadId[_preferredNumaDomain])
      {
        auto& nextMap    = *_numaNextStateQueues[_preferredNumaDomain];
        auto* currentBuf = _numaCurrentStateQueues[_preferredNumaDomain];

        // Updating state count per numa
        _currentStatesPerNuma[_preferredNumaDomain] = nextMap.size();

        // Resetting the current-state buffer for this step's fill (it was drained by the workers in
        // the previous step; the step barrier guarantees no drain is in flight here)
        currentBuf->clear();

        // Drain the (reward-ordered) next-state map into the current buffer in a single forward
        // traversal, then free the whole map at once. This replaces the previous N-iteration loop of
        // begin()+unsafe_extract (each of which unlinks and frees one node): a plain traversal is
        // cheaper per element, and a single bulk clear() deallocates all nodes without the per-node
        // extract bookkeeping. The map iterates in descending-reward order (std::greater), so the
        // first element seen is the best and the last is the worst -- same ordering as before.
        float firstReward = 0.0f, lastReward = 0.0f;
        void *firstPtr = nullptr, *lastPtr = nullptr;
        for (const auto& entry : nextMap)
        {
          if (firstPtr == nullptr)
          {
            firstReward = entry.first;
            firstPtr    = entry.second;
          }
          currentBuf->push_back_no_lock(entry.second);
          lastReward = entry.first;
          lastPtr    = entry.second;
        }

        // Releasing all next-map nodes in one go
        nextMap.clear();

        // Updating the cross-NUMA best/worst states (shared, hence locked) once per domain
        if (firstPtr != nullptr)
        {
          _workMutex.lock();
          if (firstReward > bestStateReward)
          {
            bestStateReward = firstReward;
            _bestState      = firstPtr;
          }
          if (lastReward < worstStateReward)
          {
            worstStateReward = lastReward;
            _worstState      = lastPtr;
          }
          _workMutex.unlock();
        }
      }
    }
  }

  /**
   * @brief Pops a single base state from the current-state database, preferring NUMA-local domains.
   * @return A pointer to a base state, or nullptr if every domain's current-state queue is drained.
   *
   * @details Scans NUMA domains in the calling thread's preference order (skipping unallocated null
   * queues) and pops from the front of the first non-empty current-state queue. Updates the calling
   * thread's NUMA-distance and locality statistics on success.
   */
  __INLINE__ void* popState()
  {
    auto& sc = _statCounters[jaffarCommon::parallel::getThreadId()];

    // Pointer to return
    void* statePtr;

    // Trying to next state
    for (size_t i = 0; i < (size_t)_numaCount; i++)
    {
      const auto numaIdx = _numaPreferenceMatrix[_preferredNumaDomain][i];
      // Skip NUMA domains with no worker thread: their queues are never allocated (see initialize()).
      if (_numaCurrentStateQueues[numaIdx] == nullptr) continue;
      bool success = _numaCurrentStateQueues[numaIdx]->pop_front_get(statePtr);

      // If successful, return the pointer immediately
      if (success == true)
      {
        sc.distanceAccumulator += _numaDistanceMatrix[_preferredNumaDomain][numaIdx];
        if (numaIdx == (size_t)_preferredNumaDomain)
          sc.localDatabaseState++;
        else
          sc.nonLocalDatabaseState++;
        return statePtr;
      }
    }

    // If no success at all, just return a nullptr
    sc.databaseStateNotFound++;
    return nullptr;
  }

  /**
   * @brief Pops a batch of base states from the current-state database, preferring NUMA-local domains.
   * @param elements Output buffer the popped state pointers are written into.
   * @param maxCount Maximum number of states to retrieve.
   * @param threadId The calling thread's dense OpenMP id, used to index its statistics counters.
   * @return The number of states actually retrieved (0 if every domain's current-state queue is drained).
   *
   * @details Batched counterpart of @ref popState that scans NUMA domains in the calling thread's
   * preference order (skipping unallocated null queues) and takes the whole batch from the first
   * non-empty current-state queue under a single lock acquisition. Accumulates NUMA-distance and
   * locality statistics once per batch.
   */
  __INLINE__ size_t popStates(void** elements, const size_t maxCount, const size_t threadId)
  {
    auto& sc = _statCounters[threadId];

    // Trying the NUMA domains in preference order; take the whole batch from the first non-empty one
    for (size_t i = 0; i < (size_t)_numaCount; i++)
    {
      const auto numaIdx = _numaPreferenceMatrix[_preferredNumaDomain][i];
      // Skip NUMA domains with no worker thread: their queues are never allocated (see initialize()).
      if (_numaCurrentStateQueues[numaIdx] == nullptr) continue;

      const size_t count = _numaCurrentStateQueues[numaIdx]->pop_front_get_batch(elements, maxCount);

      // If we got anything, account for it once for the whole batch and return
      if (count > 0)
      {
        sc.distanceAccumulator += _numaDistanceMatrix[_preferredNumaDomain][numaIdx] * count;
        if (numaIdx == (size_t)_preferredNumaDomain)
          sc.localDatabaseState += count;
        else
          sc.nonLocalDatabaseState += count;
        return count;
      }
    }

    // If no success at all, the databases are drained for this step
    sc.databaseStateNotFound++;
    return 0;
  }

  /**
   * @brief Returns the total number of states currently held in the current-state database.
   * @return The sum of the sizes of all allocated per-NUMA current-state queues.
   *
   * @details Skips unallocated (null) per-domain queues so the count is safe when threads do not
   * cover every NUMA domain.
   */
  __INLINE__ size_t getStateCount() const
  {
    size_t stateCount = 0;
    // Per-NUMA current-state queues are only allocated for domains that have a delegate thread.
    // Skip any unallocated (null) queue so this (informational) count can't dereference a null
    // pointer when threads don't cover every NUMA domain.
    for (int i = 0; i < _numaCount; i++)
      if (_numaCurrentStateQueues[i] != nullptr) stateCount += _numaCurrentStateQueues[i]->wasSize();
    return stateCount;
  }

  /** @brief Returns a pointer to the highest-reward state recorded by the last @ref advanceStep. */
  __INLINE__ void* getBestState() const { return _bestState; }

  /** @brief Returns a pointer to the lowest-reward state recorded by the last @ref advanceStep. */
  __INLINE__ void* getWorstState() const { return _worstState; }

  /** @brief Returns the per-state size (including padding) as stored in the database. */
  __INLINE__ size_t getStateSizeInDatabase() const { return _stateSize; }

private:
  void* _bestState  = nullptr; ///< Pointer to the best (highest-reward) state from the last advanceStep().
  void* _worstState = nullptr; ///< Pointer to the worst (lowest-reward) state from the last advanceStep().

  Runner* const _runner; ///< The runner used to serialize states into and deserialize states out of the database.

  /// @brief Size occupied by each stored state, including alignment padding.
  size_t _stateSize;

  /// @brief Raw (unpadded) serialized size of a single state.
  size_t _stateSizeRaw;

  /// @brief Number of padding bytes added to the raw size to reach @ref _stateSize.
  size_t _stateSizePadding;

  /// @brief Total maximum size (bytes) the state database may grow to across all NUMA domains.
  size_t _maxSize;

  /// @brief Total maximum number of states the database can hold across all NUMA domains.
  size_t _maxStates;

  /**
   * @brief Per-thread statistics counters for state-DB popping and free-state acquisition.
   *
   * @details Cache-line aligned to avoid false sharing between adjacent threads' counters; reduced
   * into totals only when @ref printInfo runs (off the hot path).
   */
  struct alignas(64) statCounters_t
  {
    size_t localDatabaseState    = 0; ///< Base states popped from the thread's own (preferred) NUMA domain.
    size_t nonLocalDatabaseState = 0; ///< Base states popped from a non-preferred NUMA domain.
    size_t databaseStateNotFound = 0; ///< Pop attempts that found every current-state queue drained.
    size_t localFreeState        = 0; ///< Free slots obtained from the preferred NUMA domain's free queue.
    size_t nonLocalFreeState     = 0; ///< Free slots obtained from a non-preferred domain's free queue.
    size_t stealingFreeState     = 0; ///< Free slots stolen from the back of a current-state queue.
    size_t freeStateNotFound     = 0; ///< getFreeState attempts that found no slot anywhere.
    size_t distanceAccumulator   = 0; ///< Sum of NUMA distances over all accesses, for the average-distance metric.
    size_t freeStateCacheHit     = 0; ///< getFreeState requests served from the thread-local free-slot cache.
    size_t freeStateCacheReturn  = 0; ///< returnFreeState calls absorbed by the thread-local free-slot cache.
  };
  std::vector<statCounters_t> _statCounters; ///< Per-thread (OpenMP-thread-indexed) statistics counters.

  /**
   * @brief Capacity of each worker's thread-local free-slot cache ("magazine").
   *
   * @details Kept small so the total number of slots parked in caches (this * threadCount) stays a
   * negligible fraction of the state database.
   */
  static constexpr size_t FREE_STATE_CACHE_CAPACITY = 32;

  /**
   * @brief Per-thread (OpenMP-thread-indexed) cache of free state slots fronting the shared free-state queues.
   *
   * @details Cache-line aligned so adjacent threads' caches never share a line (no false sharing).
   */
  struct alignas(64) freeStateCache_t
  {
    size_t count = 0;                        ///< Number of slots currently held in @ref slots.
    void*  slots[FREE_STATE_CACHE_CAPACITY]; ///< Cached free-slot pointers (used as a LIFO stack).
  };
  std::vector<freeStateCache_t> _freeStateCache; ///< Per-thread free-slot caches, indexed by OpenMP thread id.

  /// @brief User-provided maximum megabytes to use for the entire state database.
  size_t _maxSizeMb;

  /// @brief Maximum size (bytes) of the state database in each NUMA domain.
  std::vector<size_t> _maxSizePerNuma;

  /// @brief Calculated maximum number of states the state database can hold in each NUMA domain.
  std::vector<size_t> _maxStatesPerNuma;

  /// @brief Number of current states held in each NUMA domain (updated each advanceStep()).
  std::vector<size_t> _currentStatesPerNuma;

  /// @brief Mutex guarding the cross-NUMA best/worst-state updates in advanceStep().
  std::mutex _workMutex;

  /// @brief Per-NUMA-domain current-state queues drained by the workers during a step (null for non-delegate domains).
  std::vector<jaffarCommon::concurrent::DrainBuffer<void*>*> _numaCurrentStateQueues;

  /// @brief Per-NUMA-domain reward-ordered queues collecting the next step's states (null for non-delegate domains).
  std::vector<jaffarCommon::concurrent::concurrentMultimap_t<float, void*>*> _numaNextStateQueues;

  /// @brief Per-NUMA-domain queues holding pointers to all currently free state slots.
  std::vector<std::unique_ptr<jaffarCommon::concurrent::atomicQueue_t<void*>>> _freeStateQueues;

  /// @brief Start pointer of each NUMA domain's contiguous state slab.
  std::vector<uint8_t*> _internalBuffersStart;

  /// @brief End pointer of each NUMA domain's contiguous state slab.
  std::vector<uint8_t*> _internalBuffersEnd;

  /// @brief Number of bytes allocated for the state slab in each NUMA domain.
  std::vector<size_t> _allocableBytesPerNuma;

  /// @brief Unpadded size of one state's cold "path" data (bit-packed input history + step counter).
  /// Stored in a parallel slab (@ref _histBuffersStart) rather than in the hot state slot, so the data
  /// the search hashes/advances every step stays dense. Written once at state creation, read once when
  /// a solution is reconstructed.
  size_t _histSize = 0;

  /// @brief Start pointer of each NUMA domain's parallel history (cold) slab. Indexed identically to the
  /// state slab: the history for the hot slot at index i lives at _histBuffersStart[numa] + i*_histSize.
  std::vector<uint8_t*> _histBuffersStart;

  /// @brief The reference runner's input-history strategy, used for the per-slot manager operations
  /// (initColdSlot / releaseColdSlot / captureColdToFull). Polymorphic: no-ops for raw/none, GC for trie.
  InputHistory* _ih = nullptr;

  /// @brief Full self-contained serialized state size ([hot]+[full bit-packed history]) for standalone
  /// snapshot buffers. May exceed _stateSizeRaw + _histSize in trie mode (cold slot stores only a node id).
  size_t _fullStateSizeBytes = 0;
};

} // namespace jaffarPlus
