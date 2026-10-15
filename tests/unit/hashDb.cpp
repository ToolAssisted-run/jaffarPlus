#include "hashDb.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <set>

// ------------------------------------------------------------------
// Two-tier hash database correctness.
//
// The hash DB uses a per-NUMA-domain local L1 store in front of a single global
// authoritative L2 store (workers on several domains), collapsing to one local store
// when the workers run on a single domain. Its correctness guarantee is COMPLETE dedup: a hash seen by
// any domain must be detected as a duplicate by every domain -- behaviourally
// identical to a single global table.
//
// HashDb reads its NUMA topology from the numa.hpp globals (_numaCount,
// _threadCount, _numaDelegateThreadId: only domains with a worker thread get an L1)
// and routes L1 by the thread-local _preferredNumaDomain. By setting
// those directly, these tests simulate an N-domain host on any machine (so the
// multi-domain two-tier path is exercised even on a single-NUMA CI runner), and
// drive checks "from" specific domains by setting _preferredNumaDomain per call.
// ------------------------------------------------------------------

using namespace jaffarPlus;
using jaffarCommon::hash::hash_t;

namespace
{

// Minimal hash-DB config. A large store budget and the fact that these tests never
// call advanceStep() mean the stores never roll, so dedup is exact (no eviction).
nlohmann::json makeConfig()
{
  nlohmann::json j;
  j["Max Store Count"]     = 4;
  j["Max Store Size (Mb)"] = 4096.0;
  return j;
}

// Build a distinct hash from a 64-bit seed using the engine's own hash function (so this
// works regardless of how hash_t is defined). Distinct seeds yield distinct hashes.
hash_t H(uint64_t v) { return jaffarCommon::hash::calculateMetroHash(&v, sizeof(v)); }

// Single-threaded tests: the (dense) thread id is always 0, so any thread count >= 1 works
void simulateTopology(int numaDomains)
{
  _threadCount = 8;
  _numaCount   = numaDomains;
  // Every simulated domain has a worker (delegate) thread, so each one gets an L1
  _numaDelegateThreadId.resize(numaDomains);
  for (int i = 0; i < numaDomains; i++) _numaDelegateThreadId[i] = i;
}

} // namespace

// A hash first seen on one domain must be detected as a duplicate on a *different*
// domain. This is the property the old per-NUMA-group hashing got wrong (it dropped
// cross-domain duplicates); two-tier must catch them via the global L2.
TEST(HashDbTwoTier, CrossDomainDuplicateDetected)
{
  simulateTopology(4);
  HashDb db(makeConfig());
  db.initialize();

  _preferredNumaDomain = 0;
  EXPECT_FALSE(db.checkHashExists(H(42))); // first sighting (also registers it)

  _preferredNumaDomain = 1;
  EXPECT_TRUE(db.checkHashExists(H(42))); // seen by domain 0 -> duplicate for domain 1

  _preferredNumaDomain = 3;
  EXPECT_TRUE(db.checkHashExists(H(42))); // ... and for every other domain
}

// A never-seen hash must never be reported as a duplicate (no false positives, which
// would silently drop a genuinely new state).
TEST(HashDbTwoTier, NoFalsePositives)
{
  simulateTopology(4);
  HashDb db(makeConfig());
  db.initialize();

  _preferredNumaDomain = 2;
  EXPECT_FALSE(db.checkHashExists(H(7)));
  EXPECT_FALSE(db.checkHashExists(H(8))); // distinct, never seen
  EXPECT_FALSE(db.checkHashExists(H(9)));
  EXPECT_TRUE(db.checkHashExists(H(7))); // now seen
}

// Repeats within the same domain are caught by that domain's local L1.
TEST(HashDbTwoTier, WithinDomainDuplicateDetected)
{
  simulateTopology(4);
  HashDb db(makeConfig());
  db.initialize();

  _preferredNumaDomain = 1;
  EXPECT_FALSE(db.checkHashExists(H(99)));
  EXPECT_TRUE(db.checkHashExists(H(99)));
  EXPECT_TRUE(db.checkHashExists(H(99)));
}

// On a single-domain host the L1 tier is skipped (single local store); basic dedup
// must still work.
TEST(HashDbSingleDomain, BasicDedup)
{
  simulateTopology(1);
  HashDb db(makeConfig());
  db.initialize();

  _preferredNumaDomain = 0;
  EXPECT_FALSE(db.checkHashExists(H(5)));
  EXPECT_TRUE(db.checkHashExists(H(5)));
  EXPECT_FALSE(db.checkHashExists(H(6)));
  EXPECT_TRUE(db.checkHashExists(H(6)));
}

// insertHash() (used to register the initial state) must make a hash visible to a
// subsequent check, including across domains.
TEST(HashDbTwoTier, InsertHashThenCheck)
{
  simulateTopology(4);
  HashDb db(makeConfig());
  db.initialize();

  _preferredNumaDomain = 0;
  db.insertHash(H(123));

  _preferredNumaDomain = 2;
  EXPECT_TRUE(db.checkHashExists(H(123)));
}

// The core guarantee: over an arbitrary sequence of checks with hashes reused across
// different domains, the multi-domain two-tier DB returns exactly the same duplicate
// verdicts as a single global table. Also asserts each distinct hash is reported new
// exactly once -- i.e. dedup is complete, with no cross-domain leakage.
TEST(HashDbTwoTier, MatchesGlobalDedupOverSequence)
{
  // Reference: single global table
  simulateTopology(1);
  HashDb global(makeConfig());
  global.initialize();

  // Subject: 4-domain two-tier (initialized after; checkHashExists reads only the
  // per-instance L1/L2 it captured, not the live _numaCount)
  simulateTopology(4);
  HashDb twoTier(makeConfig());
  twoTier.initialize();

  std::set<uint64_t> distinct;
  size_t             twoTierNew = 0;

  for (int i = 0; i < 20000; i++)
  {
    // Small value space => frequent repeats; the same value recurs across domains
    const uint64_t v      = (uint64_t)((i * 2654435761ULL) % 1500);
    const hash_t   h      = H(v);
    const int      domain = (int)((v + (uint64_t)i) % 4);

    // Reference verdict ignores the domain (single store); two-tier uses it for L1 routing
    _preferredNumaDomain = 0;
    const bool g         = global.checkHashExists(h);

    _preferredNumaDomain = domain;
    const bool t         = twoTier.checkHashExists(h);

    ASSERT_EQ(g, t) << "two-tier vs global verdict mismatch at i=" << i << " value=" << v << " domain=" << domain;
    if (t == false) twoTierNew++;
    distinct.insert(v);
  }

  // Complete dedup: each distinct hash is reported "new" exactly once across all domains
  EXPECT_EQ(twoTierNew, distinct.size());
}
