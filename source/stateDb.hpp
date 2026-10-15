#pragma once

/**
 * @file stateDb.hpp
 * @brief Per-NUMA-domain database of serialized game states, with reward-ordered queues that feed
 *        the search one step at a time and a free-slot pool the workers draw from.
 */

#include "inputHistory/inputHistory.hpp"
#include "numa.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/concurrent.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/parallel.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <limits>
#include <memory>
#include <utmpx.h>
#include <vector>
#if defined(__GLIBCXX__) && defined(_OPENMP)
#include <parallel/algorithm>
#endif

/// @brief Alignment boundary (in bytes) each stored state is padded up to, for vectorized access and false-sharing avoidance.
#define _JAFFAR_STATE_PADDING_BYTES 16

namespace jaffarPlus
{

/**
 * @brief Stores serialized game states across the machine's NUMA domains and serves them to the
 *        search engine in reward-ordered, per-step batches.
 *
 * @details Each NUMA domain owns a contiguous slab of state slots (allocated on that node), a
 * free-slot queue of unused slots and a "current" queue that the workers drain during a step. The
 * states produced during a step are collected per worker thread, unordered, in a private "next"
 * buffer of {reward, slot} entries. The lifecycle is: workers @ref pushState into their own next
 * buffer; @ref advanceStep gathers all next entries by owning NUMA domain, sorts each domain's
 * entries best-reward first in parallel, and lays the slot pointers into that domain's current
 * queue; workers then @ref popState / @ref popStates the current queue. Slots are obtained with
 * @ref getFreeState and recycled with @ref returnFreeState, both fronted by a per-thread free-slot
 * cache. When no slot is free and no unexpanded current state is left to steal, a worker may
 * replace the worst state of its OWN next buffer with a better new one (@ref evictWorseNextState);
 * for that the buffer is turned into a private min-heap the first time it is needed in a step.
 * The per-domain queues are only allocated for NUMA domains that own a delegate worker thread;
 * other domains hold null queue pointers, which the access paths skip.
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
   * @brief Frees the per-NUMA state slabs, history slabs, current-state queues and next-state sort
   *        buffers allocated in @ref initialize().
   *
   * @details Only delegate domains hold slabs, a queue and a sort buffer; the remaining entries are
   * nullptr and are skipped.
   */
  ~StateDb()
  {
    // Free the per-NUMA queues allocated with `new` in initialize() (one per delegate domain; the
    // other entries are nullptr, which delete safely ignores).
    for (auto* queue : _numaCurrentStateQueues) delete queue;
    for (size_t i = 0; i < _numaSortBuffers.size(); i++)
      if (_numaSortBuffers[i] != nullptr) numa_free(_numaSortBuffers[i], _maxStatesPerNuma[i] * sizeof(nextEntry_t));
    for (size_t i = 0; i < _internalBuffersStart.size(); i++)
      if (_internalBuffersStart[i] != nullptr) numa_free(_internalBuffersStart[i], _allocableBytesPerNuma[i]);
    for (size_t i = 0; i < _histBuffersStart.size(); i++)
      if (_histBuffersStart[i] != nullptr) numa_free(_histBuffersStart[i], _maxStatesPerNuma[i] * _histSize);
  }

  /**
   * @brief Allocates and initializes the per-NUMA state slabs, queues, and per-thread caches.
   *
   * @details Queries the runner's state size and pads it up to @ref _JAFFAR_STATE_PADDING_BYTES.
   * Allocates per-thread statistics counters and free-slot caches. Splits the configured maximum
   * size evenly across the NUMA domains that own a worker (delegate) thread -- domains without one
   * get no storage --, verifies each domain has enough free memory for its slabs and per-slot queue
   * structures (throwing otherwise), and derives the per-domain maximum state counts. For each NUMA
   * domain that owns a delegate thread it creates the current-state DrainBuffer (reserved to the
   * domain's state count) and the next-state sort buffer (one entry per slot); it also creates the
   * empty per-thread next-state buffers. It then allocates the per-domain contiguous state slab on
   * that node, touches one byte per page of the slabs and sort buffers to fault them in, and fills
   * the domain's free-state queue with pointers to every slot.
   * @throws A runtime error if a domain's requested memory exceeds its free space, if a domain's
   *         state count exceeds the free-slot queue's capacity limit (2^30), or if a NUMA
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

    // Splitting state database equally among the numa domains that have worker threads. The others are
    // never written to (no thread allocates or pushes there), so they get no storage at all.
    size_t activeNumaCount = 0;
    for (int i = 0; i < _numaCount; i++)
      if (isNumaDomainActive(i)) activeNumaCount++;
    if (activeNumaCount == 0) JAFFAR_THROW_LOGIC("No NUMA domain has a worker thread. This must be a bug in Jaffar\n");
    const size_t stateDbSizePerNUMA = (_maxSizeMb * 1024 * 1024) / activeNumaCount;
    _maxSizePerNuma.assign(_numaCount, 0);
    for (int i = 0; i < _numaCount; i++)
      if (isNumaDomainActive(i)) _maxSizePerNuma[i] = stateDbSizePerNUMA;

    // Getting maximum number of states for each NUMA domain
    _maxStatesPerNuma.assign(_numaCount, 0);
    _currentStatesPerNuma.assign(_numaCount, 0);
    // The per-state memory footprint is the padded hot slot plus its cold history slot, so the budget
    // covers both slabs and the state count stays ~unchanged versus storing them together.
    for (int i = 0; i < _numaCount; i++)
      if (isNumaDomainActive(i)) _maxStatesPerNuma[i] = std::max(_maxSizePerNuma[i] / (_stateSize + _histSize), 1ul);

    // Checking that each domain's free memory holds its slabs plus the per-slot queue structures
    // allocated on it (the next-state sort buffer and the current-state buffer)
    for (int i = 0; i < _numaCount; i++)
    {
      if (_maxStatesPerNuma[i] == 0) continue;
      long long freeMemory = 0;
      numa_node_size64(i, &freeMemory);
      const size_t required = _maxSizePerNuma[i] + _maxStatesPerNuma[i] * (sizeof(nextEntry_t) + sizeof(void*));
      if (required > (size_t)freeMemory) JAFFAR_THROW_RUNTIME("The requested memory (%lu) for NUMA domain %d exceeds its available free space (%lld)\n", required, i, freeMemory);
    }

    // Getting totals for statistics
    _maxSize   = 0;
    _maxStates = 0;
    for (int i = 0; i < _numaCount; i++)
    {
      _maxSize += _maxSizePerNuma[i];
      _maxStates += _maxStatesPerNuma[i];
    }

    // Allocating state databases in the correponding NUMA domains
    _numaCurrentStateQueues.assign(_numaCount, nullptr);
    // The current-state queue is a DrainBuffer: it is filled single-threaded in advanceStep() and
    // then drained concurrently from both ends by the workers, never both at once (the step barrier
    // separates the phases). Its capacity is the NUMA domain's full state count -- it can never hold
    // more current states than that. Its claim counters are 32-bit, and the free-slot queue (atomic_queue's
    // AtomicQueueB, which rounds its capacity up to a power of two and compares it as a signed 32-bit
    // value) only works up to 2^30 elements, so guard the count.
    constexpr size_t maxStatesPerNumaLimit = 1ul << 30;
    for (int i = 0; i < _numaCount; i++)
      if (_maxStatesPerNuma[i] > maxStatesPerNumaLimit)
        JAFFAR_THROW_RUNTIME("State count per NUMA domain (%lu) exceeds the free-slot queue limit (%lu). Reduce 'Max Size (Mb)' or use larger states\n", _maxStatesPerNuma[i],
                             maxStatesPerNumaLimit);

    JAFFAR_PARALLEL
    {
      if ((ssize_t)jaffarCommon::parallel::getThreadId() == _numaDelegateThreadId[_preferredNumaDomain])
      {
        auto* currentQueue = new jaffarCommon::concurrent::DrainBuffer<void*>();
        currentQueue->reserve(_maxStatesPerNuma[_preferredNumaDomain]);
        _numaCurrentStateQueues[_preferredNumaDomain] = currentQueue;
      }
    }

    // Per-thread next-state buffers (empty; they grow on demand and keep their capacity across steps)
    _nextBuffers.clear();
    _nextBuffers.resize(_threadCount);

    // Per-domain sort buffers for advanceStep(): a domain can never hold more next states than slots,
    // so its buffer is sized to the domain's slot count. Allocated on the domain's node (faulted in below,
    // with the slabs). Only domains with a delegate thread ever hold slots in use.
    _numaSortBuffers.assign(_numaCount, nullptr);
    for (int i = 0; i < _numaCount; i++)
      if (_numaCurrentStateQueues[i] != nullptr)
      {
        _numaSortBuffers[i] = (nextEntry_t*)numa_alloc_onnode(_maxStatesPerNuma[i] * sizeof(nextEntry_t), i);
        if (_numaSortBuffers[i] == nullptr) JAFFAR_THROW_RUNTIME("Error trying to allocate the next-state sort buffer for numa domain %d\n", i);
      }

    // Getting number of bytes to reserve for each NUMA domain
    _allocableBytesPerNuma.resize(_numaCount);
    for (int i = 0; i < _numaCount; i++) _allocableBytesPerNuma[i] = _maxStatesPerNuma[i] * _stateSize;

    // Creating internal buffers, one per NUMA domain with worker threads (null for the others)
    _internalBuffersStart.assign(_numaCount, nullptr);
    _internalBuffersEnd.assign(_numaCount, nullptr);
    for (int i = 0; i < _numaCount; i++)
    {
      if (_allocableBytesPerNuma[i] == 0) continue;
      _internalBuffersStart[i] = (uint8_t*)numa_alloc_onnode(_allocableBytesPerNuma[i], i);
      if (_internalBuffersStart[i] == NULL) JAFFAR_THROW_RUNTIME("Error trying to allocate memory for numa domain %d\n", i);
      _internalBuffersEnd[i] = &_internalBuffersStart[i][_allocableBytesPerNuma[i]];
    }

    // Creating the parallel history (cold) slabs, one per NUMA domain, on the same node. Mirrors the
    // state-slab grid: the cold slot for state index i is at _histBuffersStart[numa] + i*_histSize.
    // _histSize is 0 for the "None" strategy (no path stored, and the step counter is now runner-owned, not
    // per-state): there is no cold slab to allocate, and all cold-slot ops are no-ops over 0 bytes.
    _histBuffersStart.assign(_numaCount, nullptr);
    if (_histSize > 0)
      for (int i = 0; i < _numaCount; i++)
      {
        if (_maxStatesPerNuma[i] == 0) continue;
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

    // And the sort buffers, so they are resident (and checked against the node's free memory above) from
    // the start instead of being faulted in when the database first fills up.
    for (int numaNodeIdx = 0; numaNodeIdx < _numaCount; numaNodeIdx++)
      if (_numaSortBuffers[numaNodeIdx] != nullptr)
      {
        auto* const bytes = (uint8_t*)_numaSortBuffers[numaNodeIdx];
        JAFFAR_PARALLEL_FOR
        for (size_t i = 0; i < _maxStatesPerNuma[numaNodeIdx] * sizeof(nextEntry_t); i += pageSize) bytes[i] = 0;
      }

    // Let the input-history strategy initialize each fresh cold slot (e.g. the trie marks its node id as
    // empty so a recycled slot is never mistaken for holding a node). No-op for raw/none. Skipped entirely
    // when there is no cold slab (_histSize == 0, the "None" strategy).
    if (_histSize > 0)
      for (int numaNodeIdx = 0; numaNodeIdx < _numaCount; numaNodeIdx++) JAFFAR_PARALLEL_FOR
    for (size_t s = 0; s < _maxStatesPerNuma[numaNodeIdx]; s++) _ih->initColdSlot(&_histBuffersStart[numaNodeIdx][s * _histSize]);

    // Adding the state pointers to the free state queues
    _freeStateQueues.resize(_numaCount);
    std::atomic<size_t> rejectedSlots{0};
    JAFFAR_PARALLEL
    {
      if ((ssize_t)jaffarCommon::parallel::getThreadId() == _numaDelegateThreadId[_preferredNumaDomain])
      {
        _freeStateQueues[_preferredNumaDomain] = std::make_unique<jaffarCommon::concurrent::atomicQueue_t<void*>>(_maxStatesPerNuma[_preferredNumaDomain]);
        for (size_t i = 0; i < _maxStatesPerNuma[_preferredNumaDomain]; i++)
          if (_freeStateQueues[_preferredNumaDomain]->try_push((void*)&_internalBuffersStart[_preferredNumaDomain][i * _stateSize]) == false) rejectedSlots++;
      }
    }
    if (rejectedSlots > 0) JAFFAR_THROW_RUNTIME("The free-slot queues rejected %lu state slots. This must be a bug in Jaffar\n", rejectedSlots.load());
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
   * @brief Serializes the runner's state into the given slot and appends it to the calling thread's
   *        next-state buffer.
   * @param reward   The reward used to order the state at the next @ref advanceStep.
   * @param r        The runner whose state is serialized.
   * @param statePtr The slot to serialize into and insert; determines the target NUMA domain.
   * @param threadId The calling thread's dense OpenMP id (selects its private next-state buffer).
   * @param checkpointLevel The state's checkpoint level (counted by @ref advanceStep for the engine's
   *        checkpoint cohort guard; saturates at 65535).
   * @return true if the state was serialized and inserted; false if serialization threw a runtime error.
   * @throws A runtime error if @p statePtr is null (interpreted as having run out of free states).
   *
   * @details Appends {reward, owning NUMA domain, checkpoint level, statePtr} to the thread's own
   * buffer: no lock, no shared structure. A NaN reward is stored as -infinity so the ordering stays a strict weak order.
   * Once the buffer has been turned into a min-heap in this step (see @ref evictWorseNextState), the
   * heap property is maintained on every append.
   */
  __INLINE__ bool pushState(const float reward, Runner& r, void* statePtr, const size_t threadId, const size_t checkpointLevel)
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

    // Appending the new state to this thread's private next-state buffer
    auto&      nb    = _nextBuffers[threadId];
    const auto level = (uint16_t)std::min(checkpointLevel, (size_t)std::numeric_limits<uint16_t>::max());
    nb.entries.push_back({sanitizeReward(reward), (uint16_t)getStateNumaDomain(statePtr), level, statePtr});
    if (nb.isHeap) std::push_heap(nb.entries.begin(), nb.entries.end(), isBetterEntry);

    // If succeeded, return true
    return true;
  }

  /**
   * @brief Frees the slot of the worst state in the calling thread's next-state buffer, if that state
   *        is worse than a new state of reward @p reward, so the new state can take its place.
   * @param reward   The reward of the new state that found no free slot.
   * @param threadId The calling thread's dense OpenMP id (selects its private next-state buffer).
   * @return The freed slot (its input-history resources already released), or nullptr if the buffer
   *         is empty or holds no state strictly worse than the new one.
   *
   * @details This is the last resort of a worker once no slot is free and no unexpanded state of the
   * current step is left to steal: instead of dropping a better new state, it replaces its own worst
   * one, so the database keeps (approximately, per worker) the best states produced in the step. The
   * buffer is turned into a min-heap (worst first) on the first call of the step; later appends keep
   * the heap property (see @ref pushState) until @ref advanceStep empties the buffer.
   */
  __INLINE__ void* evictWorseNextState(const float reward, const size_t threadId)
  {
    auto& nb = _nextBuffers[threadId];
    if (nb.entries.empty()) return nullptr;

    // First eviction attempt of the step: heapify (O(n) once); from now on appends are push_heap
    if (nb.isHeap == false)
    {
      std::make_heap(nb.entries.begin(), nb.entries.end(), isBetterEntry);
      nb.isHeap = true;
    }

    // The heap's front is this thread's worst next state; replace it only by a strictly better one
    if ((sanitizeReward(reward) > nb.entries.front().reward) == false) return nullptr;
    std::pop_heap(nb.entries.begin(), nb.entries.end(), isBetterEntry);
    void* const slot = nb.entries.back().statePtr;
    nb.entries.pop_back();

    // The evicted state's path resources go (the trie releases its node); the slot is then rewritten
    _ih->releaseColdSlot(getHistoryPtr(slot), threadId);
    _statCounters[threadId].nextStateReplaced++;
    return slot;
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
    size_t freeStateCacheHit = 0, freeStateCacheReturn = 0, nextStateReplaced = 0;
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
      nextStateReplaced += sc.nextStateReplaced;
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
    jaffarCommon::logger::log("[J+]  + Next States Replaced By Better Ones:      %lu\n", nextStateReplaced);
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
   * free-state queue. If all free-state queues are empty, it scans again and steals from the back
   * (worst states) of the first non-empty current-state queue: up to @ref STEAL_BATCH_SIZE unexpanded
   * states in one claim, whose input-history resources are released; the first slot is returned and
   * the rest go to this thread's (then empty) free-slot cache, so the contended claim word is touched
   * once per batch instead of once per slot. Updates the thread's NUMA-distance and
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
      void*        stolen[STEAL_BATCH_SIZE];
      const size_t stolenCount = _numaCurrentStateQueues[numaIdx]->pop_back_get_batch(stolen, STEAL_BATCH_SIZE);

      // If successful, return the first slot and keep the rest in this thread's cache
      if (stolenCount > 0)
      {
        // The stolen states are discarded unexpanded: release their input-history resources (the trie
        // frees their path nodes), exactly as returnFreeState does for an expanded base state.
        for (size_t j = 0; j < stolenCount; j++) _ih->releaseColdSlot(getHistoryPtr(stolen[j]), threadId);
        for (size_t j = 1; j < stolenCount; j++) cache.slots[cache.count++] = stolen[j];
        sc.distanceAccumulator += _numaDistanceMatrix[_preferredNumaDomain][numaIdx] * stolenCount;
        sc.stealingFreeState += stolenCount;
        return stolen[0];
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
   * @return true if the pointer falls within the domain's [start, end) slab range, false otherwise
   *         (always false for a domain without a slab).
   *
   * @details The end is exclusive: slabs of different domains can be adjacent in the address space,
   * and the end of one is then the first slot of the next.
   */
  __INLINE__ bool isStateInNumaDomain(void* const statePtr, const int numaDomainId)
  {
    return statePtr >= _internalBuffersStart[numaDomainId] && statePtr < _internalBuffersEnd[numaDomainId];
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
   * @brief Turns the states produced during the step (the per-thread next-state buffers) into the
   *        next step's current-state queues, best reward first, and empties the next-state buffers.
   * @param checkpointLevel The engine's global checkpoint level at the end of the step.
   * @return The number of states that enter the database with a checkpoint level >= @p checkpointLevel.
   *         They are counted here, from the entries that survived the step, because a state can be
   *         evicted after it was produced (see @ref evictWorseNextState).
   *
   * @details Four phases, all but the prefix sums parallel: (1) every thread counts its own entries
   * per owning NUMA domain; (2) per domain, exclusive prefix sums over the threads give each thread a
   * disjoint destination range in the domain's sort buffer; (3) every thread scatters its entries into
   * those ranges and empties its buffer (keeping its capacity); (4) each domain's entries are sorted
   * by descending reward with a parallel sort, and then the domain's delegate thread lays the slot
   * pointers into its current-state buffer in that order, so the first element popped is the best
   * and the last (the first to be stolen) the worst. The cross-NUMA best and worst states are reset
   * and then updated under @ref _workMutex from each domain's first and last entries (they stay null
   * when no state was produced). The relative order of states with
   * equal rewards is unspecified (it already depended on thread scheduling).
   * @throws A runtime error if a domain received more next states than it has slots, or states for a
   *         domain without a current-state queue (both would be bugs in Jaffar).
   */
  __INLINE__ size_t advanceStep(const size_t checkpointLevel)
  {
    const size_t numaCount = (size_t)_numaCount;

    // (1) Per-thread, per-domain entry counts, and the count of entries at the checkpoint level
    std::vector<size_t> threadDomainOffset((size_t)_threadCount * numaCount, 0);
    std::vector<size_t> threadAtLevel((size_t)_threadCount, 0);
    JAFFAR_PARALLEL
    {
      const size_t threadId = jaffarCommon::parallel::getThreadId();
      if (threadId < (size_t)_threadCount)
      {
        size_t* const counts  = &threadDomainOffset[threadId * numaCount];
        size_t        atLevel = 0;
        for (const auto& e : _nextBuffers[threadId].entries)
        {
          counts[e.numaIdx]++;
          if (e.level >= checkpointLevel) atLevel++;
        }
        threadAtLevel[threadId] = atLevel;
      }
    }
    size_t statesAtCheckpointLevel = 0;
    for (const auto c : threadAtLevel) statesAtCheckpointLevel += c;

    // (2) Exclusive prefix sums per domain: thread t writes domain d's entries from threadDomainOffset[t][d]
    std::vector<size_t> domainCount(numaCount, 0);
    for (size_t d = 0; d < numaCount; d++)
      for (size_t t = 0; t < (size_t)_threadCount; t++)
      {
        const size_t c                        = threadDomainOffset[t * numaCount + d];
        threadDomainOffset[t * numaCount + d] = domainCount[d];
        domainCount[d] += c;
      }
    for (size_t d = 0; d < numaCount; d++)
    {
      if (domainCount[d] > 0 && _numaSortBuffers[d] == nullptr)
        JAFFAR_THROW_RUNTIME("Next states found for NUMA domain %lu, which has no state queues. This must be a bug in Jaffar\n", d);
      if (domainCount[d] > _maxStatesPerNuma[d])
        JAFFAR_THROW_RUNTIME("NUMA domain %lu received %lu next states for %lu slots. This must be a bug in Jaffar\n", d, domainCount[d], _maxStatesPerNuma[d]);
    }

    // (3) Scatter every thread's entries into the per-domain sort buffers, then empty the thread buffers
    JAFFAR_PARALLEL
    {
      const size_t threadId = jaffarCommon::parallel::getThreadId();
      if (threadId < (size_t)_threadCount)
      {
        auto&         nb      = _nextBuffers[threadId];
        size_t* const offsets = &threadDomainOffset[threadId * numaCount];
        for (const auto& e : nb.entries) _numaSortBuffers[e.numaIdx][offsets[e.numaIdx]++] = e;
        nb.entries.clear();
        nb.isHeap = false;
      }
    }

    // (4a) Sort each domain's entries, best reward first (parallel sort over all threads)
    for (size_t d = 0; d < numaCount; d++)
      if (domainCount[d] > 1) sortEntries(_numaSortBuffers[d], _numaSortBuffers[d] + domainCount[d]);

    // (4b) Each delegate fills its domain's current-state buffer in that order and reports best/worst.
    // The first domain to report sets both unconditionally, so states with infinite rewards are
    // reported too, and a step without states leaves both null (never a recycled slot).
    _bestState             = nullptr;
    _worstState            = nullptr;
    float bestStateReward  = 0.0f;
    float worstStateReward = 0.0f;
    JAFFAR_PARALLEL
    {
      if ((ssize_t)jaffarCommon::parallel::getThreadId() == _numaDelegateThreadId[_preferredNumaDomain])
      {
        const size_t       d          = (size_t)_preferredNumaDomain;
        auto* const        currentBuf = _numaCurrentStateQueues[d];
        const size_t       n          = domainCount[d];
        const nextEntry_t* entries    = _numaSortBuffers[d];

        // Updating state count per numa
        _currentStatesPerNuma[d] = n;

        // Resetting the current-state buffer for this step's fill (it was drained by the workers in
        // the previous step; the step barrier guarantees no drain is in flight here)
        currentBuf->clear();
        for (size_t i = 0; i < n; i++) currentBuf->push_back_no_lock(entries[i].statePtr);

        // Updating the cross-NUMA best/worst states (shared, hence locked) once per domain
        if (n > 0)
        {
          _workMutex.lock();
          if (_bestState == nullptr || entries[0].reward > bestStateReward)
          {
            bestStateReward = entries[0].reward;
            _bestState      = entries[0].statePtr;
          }
          if (_worstState == nullptr || entries[n - 1].reward < worstStateReward)
          {
            worstStateReward = entries[n - 1].reward;
            _worstState      = entries[n - 1].statePtr;
          }
          _workMutex.unlock();
        }
      }
    }

    return statesAtCheckpointLevel;
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

  /** @brief Returns a pointer to the highest-reward state recorded by the last @ref advanceStep (null if it produced none). */
  __INLINE__ void* getBestState() const { return _bestState; }

  /** @brief Returns a pointer to the lowest-reward state recorded by the last @ref advanceStep (null if it produced none). */
  __INLINE__ void* getWorstState() const { return _worstState; }

  /** @brief Returns the per-state size (including padding) as stored in the database. */
  __INLINE__ size_t getStateSizeInDatabase() const { return _stateSize; }

  /** @brief Returns the memory one state takes in the database: its padded hot slot plus its cold history slot. */
  __INLINE__ size_t getSlotSize() const { return _stateSize + _histSize; }

  /** @brief Returns the number of states the database can hold. */
  __INLINE__ size_t getMaxStates() const { return _maxStates; }

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
    size_t nextStateReplaced     = 0; ///< Next states evicted by evictWorseNextState for a better new state.
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
   * @brief Number of unexpanded current states a worker steals at once when no slot is free.
   *
   * @details One claim on the current-state queue per batch instead of per slot: with many threads per
   * NUMA domain the per-slot claim serialized the full-database phase (measured 3.1x throughput with a
   * near-free emulator, ~1% at 10 us per state). Must not exceed @ref FREE_STATE_CACHE_CAPACITY.
   */
  static constexpr size_t STEAL_BATCH_SIZE = 8;
  static_assert(STEAL_BATCH_SIZE <= FREE_STATE_CACHE_CAPACITY, "Stolen slots must fit the (empty) free-slot cache");

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

  /// @brief One state produced during the current step: its ordering reward, owning NUMA domain,
  /// checkpoint level and slot (16 bytes).
  struct nextEntry_t
  {
    float    reward;   ///< Ordering reward (NaN stored as -infinity, see @ref sanitizeReward).
    uint16_t numaIdx;  ///< NUMA domain owning the slot (decides which current-state queue receives it).
    uint16_t level;    ///< The state's checkpoint level (saturated at 65535), counted by @ref advanceStep.
    void*    statePtr; ///< The state slot.
  };
  static_assert(sizeof(nextEntry_t) == 16, "nextEntry_t is expected to take 16 bytes");

  /// @brief A worker's private collection of the states it produced in the current step.
  struct alignas(64) nextBuffer_t
  {
    std::vector<nextEntry_t> entries;        ///< Entries in push order, or a min-heap (worst first) when @ref isHeap.
    bool                     isHeap = false; ///< Whether @ref entries was heapified by @ref evictWorseNextState this step.
  };

  /// @brief Per-thread (OpenMP-thread-indexed) next-state buffers, merged and sorted by @ref advanceStep.
  std::vector<nextBuffer_t> _nextBuffers;

  /// @brief Per-NUMA-domain scratch arrays (one entry per slot) where @ref advanceStep gathers and sorts
  /// the domain's next states. Null for domains without a delegate thread.
  std::vector<nextEntry_t*> _numaSortBuffers;

  /// @brief Strict weak order "a is better than b" (higher reward first); also the heap comparator,
  /// which makes the heap's front the worst entry.
  static __INLINE__ bool isBetterEntry(const nextEntry_t& a, const nextEntry_t& b) { return a.reward > b.reward; }

  /// @brief Maps NaN to -infinity so rewards always form a strict weak order (a NaN would break the sort).
  static __INLINE__ float sanitizeReward(const float reward) { return std::isnan(reward) ? -std::numeric_limits<float>::infinity() : reward; }

  /// @brief Sorts [first, last) best reward first: libstdc++'s OpenMP parallel sort when available.
  static __INLINE__ void sortEntries(nextEntry_t* const first, nextEntry_t* const last)
  {
#if defined(__GLIBCXX__) && defined(_OPENMP)
    __gnu_parallel::sort(first, last, isBetterEntry);
#else
    std::sort(first, last, isBetterEntry);
#endif
  }

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
