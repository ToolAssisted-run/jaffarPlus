#pragma once
#include <cstring>
#include <cstdio>

/**
 * @file numa.hpp
 * @brief NUMA topology detection: distance/preference matrices and per-domain delegate-thread
 *        selection, identifying threads by OpenMP thread id and using sched_getcpu only for the
 *        NUMA domain lookup.
 */

#include "environment.hpp"
#include <algorithm>
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
 * delegate thread per NUMA domain that has worker threads. Unless OMP_PROC_BIND or GOMP_CPU_AFFINITY is
 * set, each worker is bound to the (allowed) CPUs of one node, the threads spread over the nodes in
 * proportion to their CPUs. Each worker identifies itself by its OpenMP thread id; its preferred NUMA
 * domain is the node it is bound to, or else the node of the CPU it runs on.
 * @throws A runtime error if the system does not provide NUMA detection support, or if the thread
 *         count override is not a positive number.
 */
__INLINE__ void initializeNUMA()
{
  // Checking whether the numa library calls are available (before any other libnuma call)
  if (numa_available() < 0) JAFFAR_THROW_RUNTIME("The system does not provide NUMA detection support.\n");

  // Overriding thread count, if requested. Applied first, so every count below derives from it.
  size_t threadCountOverride = 0;
  if (getEnvironmentUnsigned("JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT", threadCountOverride))
  {
    if (threadCountOverride == 0) JAFFAR_THROW_RUNTIME("[ERROR] JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT must be at least 1\n");
    jaffarCommon::parallel::setThreadCount(threadCountOverride);
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

  // Unless the user chose a placement (OMP_PROC_BIND / GOMP_CPU_AFFINITY), bind each worker thread to the
  // CPUs of one NUMA node -- spreading the threads over the nodes in proportion to the CPUs the process may
  // use on each -- so threads stay on the node their states live on instead of migrating (measured: ~20% of
  // thread time on another node unpinned). The process's own CPU set (numactl, taskset) is respected.
  const bool             userPlacement = std::getenv("OMP_PROC_BIND") != nullptr || std::getenv("GOMP_CPU_AFFINITY") != nullptr;
  std::vector<cpu_set_t> nodeCpus;   // allowed CPUs of each node
  std::vector<size_t>    threadNode; // node assigned to each thread
  if (userPlacement == false && _numaCount > 1)
  {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) == 0)
    {
      nodeCpus.resize(_numaCount);
      for (auto& set : nodeCpus) CPU_ZERO(&set);
      std::vector<size_t> nodeCpuCount(_numaCount, 0);
      size_t              totalCpus = 0;
      for (int cpu = 0; cpu < CPU_SETSIZE; cpu++)
        if (CPU_ISSET(cpu, &allowed))
        {
          const int node = numa_node_of_cpu(cpu);
          if (node < 0 || node >= _numaCount) continue;
          CPU_SET(cpu, &nodeCpus[node]);
          nodeCpuCount[node]++;
          totalCpus++;
        }
      // Thread t goes to the node whose share of the CPUs covers position (t + 1/2) / threadCount
      if (totalCpus > 0)
      {
        threadNode.resize(_threadCount);
        for (int t = 0; t < _threadCount; t++)
        {
          const double position   = ((double)t + 0.5) / (double)_threadCount * (double)totalCpus;
          size_t       cumulative = 0, node = 0;
          for (node = 0; node < (size_t)_numaCount; node++)
          {
            cumulative += nodeCpuCount[node];
            if (position < (double)cumulative) break;
          }
          threadNode[t] = std::min(node, (size_t)_numaCount - 1);
        }
      }
    }
  }
  const bool bindThreads = threadNode.empty() == false;

  // Detecting thread-specific NUMA information
  std::mutex workMutex;
  JAFFAR_PARALLEL
  {
    // Binding this thread to its assigned node (it may still be running elsewhere until rescheduled, so
    // its domain below is the assigned node, not the CPU it is on right now)
    int boundNode = -1;
    if (bindThreads)
    {
      const auto t = jaffarCommon::parallel::getThreadId();
      if (sched_setaffinity(0, sizeof(cpu_set_t), &nodeCpus[threadNode[t]]) == 0) boundNode = (int)threadNode[t];
    }

    // Identify this worker by its dense OpenMP thread id (unique 0..N-1), NOT sched_getcpu(): the CPU
    // id collides whenever threads share a core -- under oversubscription, or simply unpinned
    // scheduling on CI runners that don't set OMP_PROC_BIND. A colliding id corrupts the per-NUMA
    // delegate selection below (and StateDb's delegate-gated queue allocation), leaving a domain with
    // no queues so the search immediately "runs out of states". The preferred NUMA domain still comes
    // from the actual CPU the thread is currently on (node 0 if the kernel cannot tell).
    _myThreadId          = jaffarCommon::parallel::getThreadId();
    const int node       = boundNode >= 0 ? boundNode : numa_node_of_cpu(sched_getcpu());
    _preferredNumaDomain = (node >= 0 && node < _numaCount) ? node : 0;

    // Setting myself as numa delegate, if I'm the first thread seen in this numa domain
    workMutex.lock();
    if (_numaDelegateThreadId[_preferredNumaDomain] == -1) _numaDelegateThreadId[_preferredNumaDomain] = _myThreadId;
    workMutex.unlock();
  }

  // Reporting the placement
  if (bindThreads) jaffarCommon::logger::log("[J+] Worker threads bound to their NUMA node, spread over the nodes (set OMP_PROC_BIND to choose the placement yourself)\n");
}


/**
 * @brief A NUMA node's free memory for the databases' guards: numa_node_size64's free pages, plus (with
 *        JAFFAR_NUMA_FREE_INCLUDES_CACHE set in the environment) the node's inactive file pages, the page cache
 *        the kernel reclaims for an allocation (a box whose nodes are filled by another program's cached files).
 * @param node The node.
 * @return The bytes the guards may count on.
 */
inline long long numaNodeFreeForGuards(const int node)
{
  long long freeMemory = 0;
  numa_node_size64(node, &freeMemory);
  if (std::getenv("JAFFAR_NUMA_FREE_INCLUDES_CACHE") == nullptr) return freeMemory;
  char path[96];
  std::snprintf(path, sizeof path, "/sys/devices/system/node/node%d/meminfo", node);
  FILE* f = std::fopen(path, "r");
  if (f == nullptr) return freeMemory;
  char      line[256];
  long long kb = 0;
  while (std::fgets(line, sizeof line, f))
    if (std::strstr(line, "Inactive(file):") != nullptr)
    {
      const char* p = std::strstr(line, "Inactive(file):") + 15;
      kb            = std::atoll(p);
      break;
    }
  std::fclose(f);
  return freeMemory + kb * 1024;
}

} // namespace jaffarPlus