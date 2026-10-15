// Micro-benchmark for the hash database, driving it directly (without the engine) with a synthetic
// stream of distinct 128-bit hashes that mimics the engine's access pattern: each "generation"
// performs a batch of concurrent checkHashExists() calls (all worker threads) followed by a single
// advanceStep(). It runs well past the point where the DB fills, so per-generation lookup throughput,
// eviction (advanceStep) cost, and resident memory can be measured at scale.
//
// Usage: hashDbBenchmark <label> <budgetGB> <insertsPerGen> <generations>
//   <label> is free-form (e.g. a config description); it only annotates the output.

// Standard / jaffarCommon headers the engine headers depend on transitively (must precede them here).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <jaffarCommon/exceptions.hpp>
#include <map>
#include <memory>
#include <vector>

#include "hashDb.hpp"
#include "numa.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numa.h>
#include <string>

namespace
{
// Resident set size (bytes) from /proc/self/statm (field 2 = resident pages).
size_t getRssBytes()
{
  FILE* f = fopen("/proc/self/statm", "r");
  if (f == nullptr) return 0;
  long total = 0, resident = 0;
  if (fscanf(f, "%ld %ld", &total, &resident) != 2) resident = 0;
  fclose(f);
  return (size_t)resident * (size_t)sysconf(_SC_PAGESIZE);
}

// Distinct, well-distributed 128-bit hash from a 64-bit counter (splitmix64 x2).
__INLINE__ jaffarCommon::hash::hash_t makeHash(uint64_t n)
{
  uint64_t a = n + 0x9E3779B97F4A7C15ULL;
  a          = (a ^ (a >> 30)) * 0xBF58476D1CE4E5B9ULL;
  a          = (a ^ (a >> 27)) * 0x94D049BB133111EBULL;
  a          = a ^ (a >> 31);
  uint64_t b = n + 0xD1B54A32D192ED03ULL;
  b          = (b ^ (b >> 30)) * 0xBF58476D1CE4E5B9ULL;
  b          = (b ^ (b >> 27)) * 0x94D049BB133111EBULL;
  b          = b ^ (b >> 31);
  return jaffarCommon::hash::hash_t(a, b);
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 5)
  {
    fprintf(stderr, "Usage: %s <label> <budgetGB> <insertsPerGen> <generations>\n", argv[0]);
    return 1;
  }
  const std::string mode          = argv[1];
  const double      budgetGB      = atof(argv[2]);
  const uint64_t    insertsPerGen = strtoull(argv[3], nullptr, 10);
  const size_t      generations   = strtoull(argv[4], nullptr, 10);

  // NUMA topology and thread count from the host (OMP_NUM_THREADS)
  jaffarPlus::initializeNUMA();

  // Build the hash-DB config. Total budget = Max Store Count * Max Store Size; use 2 stores so the
  // windowed scheme has its usual 50%-floor behaviour.
  nlohmann::json cfg;
  cfg["Max Store Count"]     = 2;
  cfg["Max Store Size (Mb)"] = (budgetGB * 1024.0) / 2.0;
  cfg["Enabled"]             = true;

  jaffarPlus::HashDb db(cfg);
  db.initialize();

  printf("# mode=%s budget=%.1f GB insertsPerGen=%lu generations=%lu threads=%d\n", mode.c_str(), budgetGB, (unsigned long)insertsPerGen, generations, jaffarPlus::_threadCount);
  printf("# gen      insert_ms   advance_ms   checks/s(M)   rss_GB\n");

  const uint64_t perThread = insertsPerGen / (uint64_t)jaffarPlus::_threadCount;
  uint64_t       base      = 1;

  double fillRss = 0.0;
  for (size_t g = 0; g < generations; g++)
  {
    const auto t0 = std::chrono::steady_clock::now();
    JAFFAR_PARALLEL
    {
      const uint64_t tid   = (uint64_t)jaffarCommon::parallel::getThreadId();
      const uint64_t start = base + tid * perThread;
      for (uint64_t k = 0; k < perThread; k++) db.checkHashExists(makeHash(start + k));
    }
    const auto t1 = std::chrono::steady_clock::now();
    db.advanceStep();
    const auto t2 = std::chrono::steady_clock::now();

    base += insertsPerGen;

    const double insertMs  = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double advanceMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    const double rssGB     = (double)getRssBytes() / (1024.0 * 1024.0 * 1024.0);
    const double checksM   = insertMs > 0.0 ? ((double)insertsPerGen / (insertMs / 1000.0)) / 1.0e6 : 0.0;
    printf("%6lu   %10.2f   %10.2f   %10.1f   %6.2f\n", (unsigned long)g, insertMs, advanceMs, checksM, rssGB);
    fflush(stdout);
    fillRss = rssGB;
  }
  (void)fillRss;
  return 0;
}
