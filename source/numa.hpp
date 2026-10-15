#pragma once

/**
 * @file numa.hpp
 * @brief NUMA topology detection: distance/preference matrices and per-domain delegate-thread
 *        selection, identifying threads by OpenMP thread id and using sched_getcpu only for the
 *        NUMA domain lookup.
 */

#include <cstdlib>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/parallel.hpp>
#include <map>
#include <mutex>
#include <numa.h>
#include <omp.h>
#include <sched.h>
#include <vector>

namespace jaffarPlus
{

inline int _numaCount = 0; ///< Number of NUMA domains detected (numa_max_node() + 1, whatever the thread count).

inline int _threadCount = 0; ///< Number of threads to use.

inline std::vector<std::vector<size_t>> _numaDistanceMatrix; ///< NUMA distance matrix; entry [i][j] is the reported distance from domain i to domain j.

inline std::vector<std::vector<size_t>>
    _numaPreferenceMatrix; ///< NUMA preference matrix; row i lists domains ordered by ascending distance from domain i (ties rotated per source domain).

inline std::vector<ssize_t> _numaDelegateThreadId; ///< Delegate thread (OpenMP thread id) per NUMA domain, or -1 if none assigned.

inline thread_local int _preferredNumaDomain = 0; ///< Thread-local preferred NUMA domain (the NUMA node of the CPU the thread ran on at initialization).

inline thread_local int _myThreadId = 0; ///< Thread-local dense OpenMP thread id of the current thread.

/**
 * @brief Tells whether a NUMA domain owns a delegate (worker) thread, i.e. whether it holds state storage.
 * @param numaDomain The NUMA domain index.
 * @return true if at least one worker thread runs on that domain.
 */
__INLINE__ bool isNumaDomainActive(const int numaDomain) { return _numaDelegateThreadId[numaDomain] >= 0; }

/**
 * @brief Initializes NUMA / core-affinity state.
 * @details Checks for libnuma support, applies the JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT environment
 * variable (if set), fixes the OpenMP team size (dynamic adjustment off, so every parallel region runs
 * exactly the same threads and the thread-local identities below stay valid), then detects the thread
 * count and NUMA domain count, builds the NUMA distance and preference matrices, and selects one
 * delegate thread per NUMA domain that has worker threads. Each worker identifies itself by its OpenMP
 * thread id and derives its preferred NUMA domain from the CPU it runs on.
 * @throws A runtime error if the system does not provide NUMA detection support, or if the thread
 *         count override is not a positive number.
 */
__INLINE__ void initializeNUMA()
{
  // Checking whether the numa library calls are available (before any other libnuma call)
  if (numa_available() < 0) JAFFAR_THROW_RUNTIME("The system does not provide NUMA detection support.\n");

  // Overriding thread count, if requested. Applied first, so every count below derives from it.
  if (auto* value = std::getenv("JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT"))
  {
    char*               end   = nullptr;
    const unsigned long count = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0' || count == 0 || value[0] == '-')
      JAFFAR_THROW_RUNTIME("JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT must be a positive number (got '%s')\n", value);
    jaffarCommon::parallel::setThreadCount(count);
  }

  // Every parallel region must run the full team: per-thread data is indexed by OpenMP thread id and
  // the thread-local identities below are set once, here.
  omp_set_dynamic(0);

  // Getting number of threads
  _threadCount = jaffarCommon::parallel::getMaxThreadCount();

  // Getting number of numa domains. All of them are listed even when the threads run on fewer (or on a
  // single one, which need not be node 0): domains without threads simply hold no storage.
  _numaCount = numa_max_node() + 1;

  // Check NUMA distances
  _numaDistanceMatrix.assign(_numaCount, std::vector<size_t>(_numaCount, 0));
  for (ssize_t i = 0; i < _numaCount; i++)
    for (ssize_t j = 0; j < _numaCount; j++) _numaDistanceMatrix[i][j] = (size_t)numa_distance(i, j);

  // Establishing NUMA preference matrix
  _numaPreferenceMatrix.assign(_numaCount, std::vector<size_t>());
  for (ssize_t i = 0; i < _numaCount; i++)
  {
    // Putting NUMA domains in the same group based on distance
    std::map<size_t, std::vector<int>> _localPreferenceMap;
    for (ssize_t j = 0; j < _numaCount; j++) _localPreferenceMap[_numaDistanceMatrix[i][j]].push_back(j);

    // Assigning NUMA domains based on local preference
    for (const auto& entry : _localPreferenceMap)
    {
      const auto& localPreferenceGroup = entry.second;
      for (ssize_t j = 0; j < (ssize_t)localPreferenceGroup.size(); j++)
      {
        const size_t offset = (j + i) % localPreferenceGroup.size();
        _numaPreferenceMatrix[i].push_back(localPreferenceGroup[offset]);
      }
    }
  }

  //// Setting one delegate thread per NUMA domain for administrative (allocation) purposes
  // Initializing numa delegate thread storage
  _numaDelegateThreadId.assign(_numaCount, -1);

  // Detecting thread-specific NUMA information
  std::mutex workMutex;
  JAFFAR_PARALLEL
  {
    // Identify this worker by its dense OpenMP thread id (unique 0..N-1), NOT sched_getcpu(): the CPU
    // id collides whenever threads share a core -- under oversubscription, or simply unpinned
    // scheduling on CI runners that don't set OMP_PROC_BIND. A colliding id corrupts the per-NUMA
    // delegate selection below (and StateDb's delegate-gated queue allocation), leaving a domain with
    // no queues so the search immediately "runs out of states". The preferred NUMA domain still comes
    // from the actual CPU the thread is currently on (node 0 if the kernel cannot tell).
    _myThreadId          = jaffarCommon::parallel::getThreadId();
    const int node       = numa_node_of_cpu(sched_getcpu());
    _preferredNumaDomain = (node >= 0 && node < _numaCount) ? node : 0;

    // Setting myself as numa delegate, if I'm the first thread seen in this numa domain
    workMutex.lock();
    if (_numaDelegateThreadId[_preferredNumaDomain] == -1) _numaDelegateThreadId[_preferredNumaDomain] = _myThreadId;
    workMutex.unlock();
  }

  // Unpinned threads migrate between nodes, which makes the NUMA placement above approximate
  if (_numaCount > 1 && std::getenv("OMP_PROC_BIND") == nullptr && std::getenv("GOMP_CPU_AFFINITY") == nullptr)
    jaffarCommon::logger::log("[J+] Note: OMP_PROC_BIND is not set, so worker threads may migrate between NUMA nodes (e.g. OMP_PROC_BIND=spread OMP_PLACES=cores pins them)\n");
}

} // namespace jaffarPlus