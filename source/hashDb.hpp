#pragma once

/**
 * @file hashDb.hpp
 * @brief Two-tier (per-domain L1 + shared global L2) hash database used to deduplicate visited search
 *        states, with a rolling set of hash stores bounded by count and measured memory size.
 */

#include "environment.hpp"
#include "numa.hpp"
#include <atomic>
#include <jaffarCommon/concurrent.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/parallel.hpp>

namespace jaffarPlus
{

/**
 * @brief Two-tier hash database that deduplicates visited states across the whole search.
 *
 * @details When the worker threads span several NUMA domains it uses a two-tier layout: a NUMA-local
 * L1 store per domain (with worker threads) in front of a single global authoritative L2 store. A worker first probes its own domain's L1 (local, fast);
 * only on an L1 miss does it consult L2 (which may be remote / contended). Every globally-new hash is
 * registered in L2 exactly once, so dedup stays COMPLETE -- identical to a single global table --
 * while the frequent within-domain repeats are served locally. When all workers run on a single domain
 * the L1 tier is skipped and a single local store is used, since everything is local anyway.
 *
 * This is automatic; there is no script-level configuration for it.
 */
class HashDb final
{
  /// @brief Number of submaps (log2) of each hash set: with thousands of submaps, many threads rarely
  /// contend for the same submap mutex (measured +37% to +53% engine throughput over 256 submaps at 64
  /// threads on a light core), at about 0.4 MB of fixed memory per generation.
  static constexpr size_t _submapBits = 12;

  /// @brief The hash set type of a store generation.
  using hashSet_t = jaffarCommon::concurrent::HashSet_t<jaffarCommon::hash::hash_t, _submapBits>;

  /// @brief Number of submaps of each hash set.
  static constexpr size_t _submapCount = (size_t)1 << _submapBits;

public:
  /**
   * @brief Constructs the hash database from its configuration block.
   * @param config JSON configuration providing "Max Store Count" and "Max Store Size (Mb)" (the keys are
   *               consumed; the caller checks for leftovers). The environment variable
   *               JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB, when set, overrides the configured maximum
   *               store size (in Mb) for testing.
   * @throws A logic error if the store count is zero or the store size is not positive.
   */
  HashDb(nlohmann::json& config)
  {
    _maxStoreCount  = jaffarCommon::json::popNumber<size_t>(config, "Max Store Count");
    _maxStoreSizeMb = jaffarCommon::json::popNumber<double>(config, "Max Store Size (Mb)");
    if (_maxStoreCount == 0) JAFFAR_THROW_LOGIC("[ERROR] 'Hash Database' > 'Max Store Count' must be at least 1\n");
    if ((_maxStoreSizeMb > 0.0) == false) JAFFAR_THROW_LOGIC("[ERROR] 'Hash Database' > 'Max Store Size (Mb)' must be positive\n");

    // For testing purposes, the maximum size can be overriden by environment variables
    size_t overrideMb = 0;
    if (getEnvironmentUnsigned("JAFFAR_ENGINE_OVERRIDE_MAX_HASHDB_SIZE_MB", overrideMb)) _maxStoreSizeMb = (double)overrideMb;
  }

  /// @brief Configured PEAK hash-DB footprint in bytes = Max Store Size x Max Store Count (the
  /// rolling generations can be live simultaneously). Used by the engine's combined RAM guard.
  __INLINE__ size_t getMaxBudgetBytes() const { return (size_t)(_maxStoreSizeMb * 1024.0 * 1024.0) * _maxStoreCount; }

  /// @brief Destroys the database, deleting all per-domain L1 stores and the global L2 store.
  ~HashDb()
  {
    for (auto* s : _l1) delete s;
    if (_l2 != nullptr) delete _l2;
  }

  /**
   * @brief Allocates the hash stores and splits the configured budget across the L1 and L2 tiers.
   *
   * @details The configured "Max Store Size" is the absolute per-generation budget for the whole hash
   * DB. When worker threads run on several NUMA domains the global L2 receives 3/4 of the budget and
   * the per-domain L1 caches share the remaining 1/4 (split evenly across the domains with worker
   * threads; the others get no L1); when they all run on one domain the lone global store receives
   * the entire budget and no L1 tier is created.
   */
  __INLINE__ void initialize()
  {
    // The configured "Max Store Size" is the ABSOLUTE per-generation budget for the whole hash DB:
    // the global L2 plus all per-domain L1 caches combined never exceed it (so the total footprint is
    // Max Store Size x Max Store Count, independent of NUMA layout -- the tiering does not inflate it).
    // Each generation grows on demand up to the largest table that fits its budget (see getRungSlots) and
    // is rolled before it would have to grow past it.
    const size_t totalBudgetBytes = (size_t)(_maxStoreSizeMb * 1024.0 * 1024.0);

    // Only the domains with worker threads ever probe an L1
    size_t activeNumaCount = 0;
    for (int i = 0; i < _numaCount; i++)
      if (isNumaDomainActive(i)) activeNumaCount++;

    if (activeNumaCount > 1)
    {
      // Multi-domain: split the budget so L2 + all L1 == the configured budget. The global L2 gets 3/4
      // and the per-domain L1 caches share the remaining 1/4. L2 holds the complete dedup set, which
      // benefits directly from more capacity (fewer rolled-out / re-explored states). The L1s only
      // absorb within-domain repeats, whose working set is strongly biased toward spatiotemporally
      // close states -- so a smaller cache still captures most of the local hit rate. Hence the 3:1
      // weighting toward L2.
      _maxStoreSizeBytes   = (totalBudgetBytes * 3) / 4;
      _l1MaxStoreSizeBytes = (totalBudgetBytes / 4) / activeNumaCount;
      _l1RungSlots         = getRungSlots(_l1MaxStoreSizeBytes);
      _l1.assign(_numaCount, nullptr);
      for (int i = 0; i < _numaCount; i++)
        if (isNumaDomainActive(i)) _l1[i] = makeStore();
      _l1ActiveCount = activeNumaCount;
    }
    else
    {
      // Single domain: no L1 tier, so the one global store gets the whole budget.
      _maxStoreSizeBytes = totalBudgetBytes;
    }

    // The single global authoritative store (holds the complete dedup set)
    _l2RungSlots = getRungSlots(_maxStoreSizeBytes);
    _l2          = makeStore();
  }

  /**
   * @brief Logs the database's current state, including per-generation breakdowns of the rolling
   *        stores and aggregated check / collision statistics.
   *
   * @details On a single-domain run it reports L2 as a single store with its rolling-window
   * breakdown. On a two-tier run it aggregates the L1 caches across domains, reports the global L2,
   * and prints the L2 rolling-window breakdown. Each store line exposes its Id and Age so window
   * rolling is observable.
   */
  void printInfo() const
  {
    constexpr double toMb = 1.0 / (1024.0 * 1024.0);

    // Sum a store's measured resident memory across its rolling generations
    auto storeSizeMb = [this](const NUMAStore_t* s)
    {
      size_t bytes = 0;
      for (const auto& h : s->_hashStores) bytes += getStoreSizeBytes(h);
      return (double)bytes * toMb;
    };

    // Print the per-generation breakdown of a rolling store (newest first). Exposes each store's Id
    // and Age, so window rolling is observable (an Id > 0 means a new store was created), plus its
    // entries, size, and check/collision counts (current store from the live per-thread counters,
    // archived stores from their snapshot taken at roll time).
    auto printStores = [this, toMb, &storeSizeMb](const NUMAStore_t* s, const char* label)
    {
      size_t idx = 0;
      for (auto it = s->_hashStores.rbegin(); it != s->_hashStores.rend(); ++it, ++idx)
      {
        const bool   cur = (it == s->_hashStores.rbegin());
        const size_t q   = cur ? s->getQueryCount() : it->queryCount;
        const size_t c   = cur ? s->getCollisionCount() : it->collisionCount;
        jaffarCommon::logger::log(
            "[J+]      + %sStore %lu/%lu - Id: %lu - Created At Step: %lu, Entries: %.3f M, Size: %.1f Mb, Check Count: %lu, Collision Count: %lu (Rate %.3f%%)\n", label, idx + 1,
            _maxStoreCount, it->id, it->age, (double)it->hashSet.size() * 1.0e-6, (double)getStoreSizeBytes(*it) * toMb, q, c, q == 0 ? 0.0 : 100.0 * (double)c / (double)q);
      }
    };

    // Single-domain (no L1 tier): report L2 as a single store, with its rolling-window breakdown
    if (_l1.empty())
    {
      jaffarCommon::logger::log("[J+]   Single-Store Dedup (workers on a single NUMA domain), budget %.1f Mb (%lu x %.1f Mb tables of %.3f M entries):\n",
                                (double)(_maxStoreSizeBytes * _maxStoreCount) * toMb, _maxStoreCount, (double)rungBytes(_l2RungSlots) * toMb,
                                (double)rungEntries(_l2RungSlots) * 1.0e-6);
      printStores(_l2, "");
      return;
    }

    // Two-tier: aggregate L1 across domains + the global L2
    size_t l1q = 0, l1c = 0, l1entries = 0;
    double l1SizeMb = 0.0;
    // Use LIFETIME (since-start) totals for the aggregate ratios so L1 and L2 are compared over the
    // same whole-run window. The per-store getQueryCount()/getCollisionCount() are reset on every roll
    // and the tiers roll at very different rates (L1 budget is a small fraction of L2's, so L1 rolls
    // far more often) -- comparing those current-window counts made l2q/l1q and the dup rate exceed
    // 100%. The lifetime totals make every ratio below a true fraction bounded by 100%.
    for (auto* const g : _l1)
    {
      if (g == nullptr) continue;
      l1q += g->getTotalQueryCount();
      l1c += g->getTotalCollisionCount();
      for (const auto& s : g->_hashStores) l1entries += s.hashSet.size();
      l1SizeMb += storeSizeMb(g);
    }
    const size_t l2q       = _l2->getTotalQueryCount();
    const size_t l2c       = _l2->getTotalCollisionCount();
    size_t       l2entries = 0;
    for (const auto& s : _l2->_hashStores) l2entries += s.hashSet.size();

    // Budgets: each L1 rolls at _l1MaxStoreSizeBytes and keeps up to _maxStoreCount generations
    // (times numaCount domains); L2 rolls at _maxStoreSizeBytes. The two budgets sum to the
    // configured Max Store Size x Max Store Count (3/4 to L2, 1/4 shared across the L1 caches).
    const double l1BudgetMb = (double)(_l1MaxStoreSizeBytes * _maxStoreCount * _l1ActiveCount) * toMb;
    const double l2BudgetMb = (double)(_maxStoreSizeBytes * _maxStoreCount) * toMb;

    jaffarCommon::logger::log("[J+]   Two-Tier Dedup (%lu NUMA-local L1 + global L2):\n", _l1ActiveCount);
    jaffarCommon::logger::log("[J+]      + L1 (local):                           Checks: %lu, Local Hits: %lu (%.3f%%), Entries: %.3f M, Size: %.1f / %.1f Mb\n", l1q, l1c,
                              l1q == 0 ? 0.0 : 100.0 * (double)l1c / (double)l1q, (double)l1entries * 1.0e-6, l1SizeMb, l1BudgetMb);
    jaffarCommon::logger::log(
        "[J+]      + L2 (global):                          Checks (L1 misses): %lu (%.3f%% of all), Cross-Domain Hits: %lu, Entries: %.3f M, Size: %.1f / %.1f Mb\n", l2q,
        l1q == 0 ? 0.0 : 100.0 * (double)l2q / (double)l1q, l2c, (double)l2entries * 1.0e-6, storeSizeMb(_l2), l2BudgetMb);
    jaffarCommon::logger::log("[J+]      + Served locally (no L2 access):        %.3f%% | Total dup rate: %.3f%%\n", l1q == 0 ? 0.0 : 100.0 * (double)l1c / (double)l1q,
                              l1q == 0 ? 0.0 : 100.0 * (double)(l1c + l2c) / (double)l1q);
    // L2 rolling-window breakdown (Id/Age make window rolling observable)
    printStores(_l2, "L2 ");
  }

  /**
   * @brief Checks whether the given hash is already present in the database, inserting it if new.
   *
   * @details On a single-domain run it probes the lone global store. On a two-tier run it first
   * probes (and caches into) the calling thread's domain-local L1 store; on an L1 hit it returns true
   * without touching L2. On an L1 miss it probes (and inserts into) the global L2, which makes the
   * hash globally visible. Per-thread query and collision counters are updated on each probe.
   * @param hash The state hash to look up.
   * @return true if the hash was already present (a duplicate state), false if it was newly inserted.
   */
  __INLINE__ bool checkHashExists(const jaffarCommon::hash::hash_t hash)
  {
    const auto threadId = jaffarCommon::parallel::getThreadId();

    // Single-domain: just the one local store
    if (_l1.empty())
    {
      _l2->_threadQueryCount[threadId].value++;
      const bool collision = probeStore(_l2, hash);
      if (collision) _l2->_threadCollisionCount[threadId].value++;
      return collision;
    }

    // L1: this domain's local store. By construction every hash in any L1 was also inserted into L2,
    // so an L1 hit is a genuine duplicate -- answered locally, with no L2 (possibly remote) access.
    // probeStore() also inserts the hash into L1's current store (caching it locally).
    auto* const l1 = _l1[_preferredNumaDomain];
    l1->_threadQueryCount[threadId].value++;
    if (probeStore(l1, hash))
    {
      l1->_threadCollisionCount[threadId].value++;
      return true;
    }

    // L1 miss -> consult the authoritative global L2. probeStore() inserts the hash into L2's current
    // store, making it globally visible (so other domains detect it as a duplicate). This is what
    // preserves complete, global dedup: every globally-new hash is registered in L2 exactly once, by
    // whichever domain first encounters it.
    _l2->_threadQueryCount[threadId].value++;
    const bool collision = probeStore(_l2, hash);
    if (collision) _l2->_threadCollisionCount[threadId].value++;
    return collision;
  }

  /**
   * @brief Removes a hash that @ref checkHashExists just registered as new, for a state that was then not kept.
   * @param hash The state hash.
   * @details Erases it from the current generation of the global L2 store and of the calling thread's
   * domain-local L1 (where checkHashExists inserted it), so a later copy of the same state is new again.
   */
  __INLINE__ void eraseHash(const jaffarCommon::hash::hash_t hash)
  {
    if (_l1.empty() == false) _l1[_preferredNumaDomain]->_hashStores.rbegin()->hashSet.erase(hash);
    _l2->_hashStores.rbegin()->hashSet.erase(hash);
  }

  /**
   * @brief Inserts a hash into the current (newest) store(s) without checking for collisions.
   *
   * @details On a two-tier run the hash is inserted into the calling thread's domain-local L1 current
   * store; in all cases it is inserted into the global L2 current store.
   * @param hash The state hash to insert.
   */
  __INLINE__ void insertHash(const jaffarCommon::hash::hash_t hash)
  {
    if (_l1.empty() == false) _l1[_preferredNumaDomain]->_hashStores.rbegin()->hashSet.insert(hash);
    _l2->_hashStores.rbegin()->hashSet.insert(hash);
  }

  /**
   * @brief Signals that a new search step has started, rolling and ageing every store.
   *
   * @details A store rolls (creates a fresh current generation, discarding the oldest when the window
   * is at capacity) when its current generation could not take another step like the last one without
   * growing past the largest table within its budget. The global L2 rolls on its own; each per-domain L1 rolls on its
   * own and also whenever L2 rolls, so an L1 never remembers a hash longer than L2 (otherwise its
   * verdicts would depend on which domain probes). Every store's age is advanced.
   */
  __INLINE__ void advanceStep()
  {
    const bool l2Rolled = rollStore(_l2, _maxStoreSizeBytes, _l2RungSlots, false);
    for (auto* const g : _l1)
      if (g != nullptr) rollStore(g, _l1MaxStoreSizeBytes, _l1RungSlots, l2Rolled);
  }

private:
  /**
   * @brief One generation of a rolling store: a hash set of previously found states plus its
   *        identity, age and snapshotted statistics.
   *
   * @details A store holds a window of these generations; the oldest are discarded first when the
   * window is at capacity.
   */
  struct hashStore_t
  {
    const size_t id; ///< The store id.

    const size_t age; ///< The step at which this generation was created.

    size_t queryCount; ///< How many queries were made (snapshot of summed per-thread counts at roll time).

    size_t collisionCount; ///< How many collisions were detected (snapshot of summed per-thread counts at roll time).

    hashSet_t hashSet = {}; ///< The internal set of hashes for this store generation.
  };

  /**
   * @brief A query/collision counter padded to a full cache line to avoid false sharing.
   *
   * @details The hot-path hash check is the most frequent operation in the engine, so these
   * statistics are accumulated per-thread (each thread touches only its own slot) instead of through
   * a single shared atomic, whose cache line would otherwise bounce between all worker cores on every
   * check. The padding prevents two threads' slots from landing on the same cache line.
   */
  struct alignas(64) paddedCounter_t
  {
    size_t value = 0; ///< The counter's accumulated value.
  };

  /**
   * @brief A single (L1 or L2) hash store: a rolling window of @ref hashStore_t generations together
   *        with its rolling state and per-thread statistics counters.
   */
  struct NUMAStore_t
  {
    size_t _currentHashStoreId = 0; ///< Next id to assign to a newly created hash store generation.

    /**
     * @brief Number of steps elapsed since this store began, assigned as the age of newly created
     *        generations.
     */
    size_t _currentAge = 0;

    /**
     * @brief The rolling window of hash store generations (oldest at front, current/newest at back).
     *
     * @details The current (newest) generation is read/write: probing it both checks for a collision
     * and inserts the hash. The older generations are read-only and are only consulted for collisions.
     */
    std::deque<hashStore_t> _hashStores;

    std::vector<paddedCounter_t> _threadQueryCount; ///< Per-thread query counts for the current store, indexed by thread id.

    std::vector<paddedCounter_t> _threadCollisionCount; ///< Per-thread collision counts for the current store, indexed by thread id.

    size_t _sizeAtLastStep = 0; ///< Entries of the current generation at the previous advanceStep (the last step's inserts are the difference).

    /// @brief Lifetime query/collision totals from all ALREADY-ROLLED generations (the per-thread
    /// counters above are reset on every roll and the oldest generations are eventually discarded, so
    /// these accumulators preserve the since-start totals for normalized, bounded-by-100% telemetry).
    size_t _lifetimeQueryCount     = 0;
    size_t _lifetimeCollisionCount = 0; ///< Lifetime collision total; companion to _lifetimeQueryCount (same since-start window).

    // Aggregating helpers (called only at reporting / store-rollover time, never in the hot path)

    /**
     * @brief Sums the per-thread query counts.
     * @return The total number of queries accumulated across all threads.
     */
    __INLINE__ size_t getQueryCount() const
    {
      size_t total = 0;
      for (const auto& c : _threadQueryCount) total += c.value;
      return total;
    }
    /**
     * @brief Sums the per-thread collision counts.
     * @return The total number of collisions accumulated across all threads.
     */
    __INLINE__ size_t getCollisionCount() const
    {
      size_t total = 0;
      for (const auto& c : _threadCollisionCount) total += c.value;
      return total;
    }
    /// @brief Resets all per-thread query and collision counters to zero.
    __INLINE__ void resetCounts()
    {
      for (auto& c : _threadQueryCount) c.value = 0;
      for (auto& c : _threadCollisionCount) c.value = 0;
    }
    /// @brief Lifetime (since-start) totals = already-rolled generations + the current window. Used for
    /// the normalized aggregate telemetry, which compares L1 and L2 over the SAME (whole-run) window.
    __INLINE__ size_t getTotalQueryCount() const { return _lifetimeQueryCount + getQueryCount(); }
    /// @brief Lifetime (since-start) collision total = already-rolled generations + the current window
    /// (the collision companion to getTotalQueryCount(), for the normalized aggregate telemetry).
    __INLINE__ size_t getTotalCollisionCount() const { return _lifetimeCollisionCount + getCollisionCount(); }
  };

  /**
   * @brief Creates a fresh store with a single (empty) hash set generation and per-thread counters.
   * @return A pointer to the newly allocated store (owned by the caller).
   */
  __INLINE__ NUMAStore_t* makeStore()
  {
    auto* s = new NUMAStore_t;
    pushGeneration(s);
    s->_threadQueryCount.resize(_threadCount);
    s->_threadCollisionCount.resize(_threadCount);
    return s;
  }

  /**
   * @brief Appends a new, empty current generation to a store.
   * @param s The store.
   *
   * @details The table grows on demand (doubling), which keeps it compact while it fills; the store rolls
   * before it would have to grow past the largest table within the budget (see @ref rollStore). Reserving
   * that table up front instead made every insert touch a fresh page of a budget-sized table: measured
   * 7-10% slower on a light core at 256 threads.
   */
  __INLINE__ void pushGeneration(NUMAStore_t* const s)
  {
    s->_hashStores.push_back(hashStore_t({.id = s->_currentHashStoreId++, .age = s->_currentAge, .queryCount = 0, .collisionCount = 0}));
    s->_sizeAtLastStep = 0;
  }

  /**
   * @brief The largest table, in slots per submap, whose memory fits a generation's budget.
   * @param budgetBytes The generation's memory budget.
   * @return The slots per submap (a power of two minus one, the sizes phmap tables take), or 0 if the
   *         budget is smaller than one slot per submap.
   *
   * @details A phmap table only grows by doubling, so a generation sized by its measured memory (as it
   * once was) stopped at the first doubling past the budget -- up to 1.9x over it, at 45-50% load.
   * Rolling before the table must grow past this size keeps the memory within the budget at a load of
   * up to 7/8.
   */
  __INLINE__ size_t getRungSlots(const size_t budgetBytes) const
  {
    if (budgetBytes <= _hashStoreFixedOverhead) return 0;
    const size_t maxSlots = (budgetBytes - _hashStoreFixedOverhead) / _bytesPerSlot / _submapCount;
    size_t       slots    = 0;
    while (slots * 2 + 1 <= maxSlots) slots = slots * 2 + 1;
    return slots;
  }

  /// @brief Entries a generation with @p rungSlots slots per submap holds before a submap must grow (7/8 load).
  __INLINE__ size_t rungEntries(const size_t rungSlots) const { return _submapCount * (rungSlots - rungSlots / 8); }

  /// @brief Memory of a generation with @p rungSlots slots per submap.
  __INLINE__ size_t rungBytes(const size_t rungSlots) const { return _submapCount * rungSlots * _bytesPerSlot + _hashStoreFixedOverhead; }

  /**
   * @brief Reverse-scans a store's hash sets (newest first), inserting into the current set and
   *        checking-only the archived sets.
   *
   * @details Inserting-while-checking on the newest set means a single probe both registers the hash
   * and detects a same-step duplicate.
   * @param store The store to probe and insert into.
   * @param hash  The hash to look up / insert.
   * @return true if the hash was already present in any of the store's generations (a collision).
   */
  __INLINE__ bool probeStore(NUMAStore_t* const store, const jaffarCommon::hash::hash_t hash)
  {
    auto   itr             = store->_hashStores.rbegin();
    size_t curHashStoreIdx = 0;
    while (itr != store->_hashStores.rend())
    {
      bool collisionFound = false;
      if (curHashStoreIdx == 0)
        collisionFound = itr->hashSet.insert(hash).second == false;
      else
        collisionFound = itr->hashSet.contains(hash);
      if (collisionFound == true) return true;
      itr++;
      curHashStoreIdx++;
    }
    return false;
  }

  /**
   * @brief Rolls a single store if needed and advances its age.
   *
   * @details The current (newest) generation rolls when another step inserting as many hashes as the
   * last one would take it past the entries the largest table within its budget holds (or when it
   * already outgrew its budget), or when @p force is set. Rolling snapshots the per-thread counters into
   * the current generation and resets them, discards the oldest generation if the window is already at
   * @ref _maxStoreCount, and pushes a fresh current generation. The store's age is then
   * advanced in all cases. A single step that inserts more hashes than a whole generation holds
   * overshoots the budget (the generation's table grows); this is reported once.
   * @param g         The store to roll.
   * @param maxBytes  The per-generation memory budget.
   * @param rungSlots The slots per submap of the largest table within the budget.
   * @param force     Roll regardless of the fill (L1 follows L2).
   * @return true if the store rolled.
   */
  __INLINE__ bool rollStore(NUMAStore_t* const g, const size_t maxBytes, const size_t rungSlots, const bool force)
  {
    auto&        currentHashStore = *g->_hashStores.rbegin();
    const size_t size             = currentHashStore.hashSet.size();
    const size_t stepInserts      = size - std::min(size, g->_sizeAtLastStep);
    const size_t limit            = rungSlots > 0 ? rungEntries(rungSlots) : 0;
    const bool   outgrown         = getStoreSizeBytes(currentHashStore) > maxBytes;
    const bool   wouldOverflow    = size > 0 && size + stepInserts > limit;
    g->_sizeAtLastStep            = size;

    if (stepInserts > limit && _overflowReported == false)
    {
      _overflowReported = true;
      jaffarCommon::logger::log("[J+] Warning: one step inserted %lu hashes, more than a hash store generation holds (%lu): the hash database exceeds its budget and "
                                "deduplicates only against the last steps. Raise 'Hash Database' > 'Max Store Size (Mb)'.\n",
                                stepInserts, limit);
    }

    const bool rolled = force || outgrown || wouldOverflow;
    if (rolled)
    {
      // Snapshot the (summed per-thread) counters into the current store before any structural
      // change, then reset. This must precede pop_front(): when maxStoreCount == 1 the front store
      // IS the current (back) store, so popping it first would leave currentHashStore dangling.
      currentHashStore.queryCount     = g->getQueryCount();
      currentHashStore.collisionCount = g->getCollisionCount();
      // Fold this window's counts into the lifetime accumulators BEFORE the reset, so the since-start
      // totals survive both the reset and the eventual discard (pop_front) of this generation.
      g->_lifetimeQueryCount += currentHashStore.queryCount;
      g->_lifetimeCollisionCount += currentHashStore.collisionCount;
      g->resetCounts();

      // If we already reached the maximum hash stores, discard the oldest one
      if (g->_hashStores.size() == _maxStoreCount) g->_hashStores.pop_front();

      // Now create the new one, by pushing it from the back
      pushGeneration(g);
    }

    // Increasing age
    g->_currentAge++;
    return rolled;
  }

  /**
   * @brief Computes the memory of a hash store generation from the underlying set's allocated capacity
   *        (number of slots) plus the fixed overhead.
   * @param store The hash store generation to measure.
   * @return The estimated memory of the store, in bytes.
   */
  __INLINE__ size_t getStoreSizeBytes(const hashStore_t& store) const { return store.hashSet.capacity() * _bytesPerSlot + _hashStoreFixedOverhead; }

  /**
   * @brief Per-slot memory cost of the underlying phmap flat hash set.
   *
   * @details Each element is stored inline (one slot = sizeof(value)) plus one control byte per slot.
   */
  static constexpr size_t _bytesPerSlot = sizeof(jaffarCommon::hash::hash_t) + sizeof(uint8_t);

  /**
   * @brief Fixed overhead of a hash store, independent of the number of entries.
   *
   * @details The parallel set holds @ref _submapCount submaps, each with its own table bookkeeping and
   * mutex, allocated even when empty.
   */
  static constexpr size_t _hashStoreFixedOverhead = sizeof(hashSet_t);

  bool _overflowReported = false; ///< Whether the one-step overflow warning was printed (once per run).

  size_t _l2RungSlots = 0; ///< Slots per submap of the largest L2 table within its budget.

  size_t _l1RungSlots = 0; ///< Slots per submap of the largest L1 table within its budget.

  size_t _maxStoreCount; ///< Maximum number of store generations to keep at any time.

  double _maxStoreSizeMb; ///< Maximum (global L2) store size, in megabytes.

  size_t _maxStoreSizeBytes; ///< Maximum global L2 store size, in bytes (derived from @ref _maxStoreSizeMb).

  size_t _l1MaxStoreSizeBytes = 0; ///< Per-domain L1 store budget, in bytes (derived: a quarter of the budget / number of domains with worker threads).

  size_t _l1ActiveCount = 0; ///< Number of L1 caches (domains with worker threads); 0 on a single-domain run.

  std::vector<NUMAStore_t*> _l1; ///< Per-domain NUMA-local L1 caches (null for domains without worker threads). Empty on a single-domain run.

  NUMAStore_t* _l2 = nullptr; ///< The single global authoritative store (holds the complete dedup set).
};

} // namespace jaffarPlus
