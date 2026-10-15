// Unit tests for the input-history strategies (source/inputHistory/): None, Raw and the shared-prefix
// Trie, plus the strict factory and the underlying jaffarCommon SequenceTrie (sharing, ref-count GC and
// the concurrent sharded free lists). The strategies are exercised through their public interface, which
// is exactly what the StateDb/Runner use.

#include "inputHistory/inputHistoryFactory.hpp"
#include "inputHistory/inputHistoryNone.hpp"
#include "inputHistory/inputHistoryRaw.hpp"
#include "inputHistory/inputHistoryTrie.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace jaffarPlus;
using namespace jaffarPlus::inputHistory;

using idx_t  = InputSet::inputIndex_t;
using nodeId = SequenceInputTrie::nodeId_t;

static constexpr uint32_t MAX_INPUT_INDEX = 5; // input alphabet {0,1,2,3,4}

// A string for each input index, so toString() can render a reconstructed path.
static const std::map<idx_t, std::string> kMap = {{idx_t(0), "a"}, {idx_t(1), "b"}, {idx_t(2), "c"}, {idx_t(3), "d"}, {idx_t(4), "e"}};

// The newline-joined string toString() should produce for a given input sequence.
static std::string expectSeq(std::initializer_list<int> seq)
{
  std::string out;
  for (int i : seq) out += kMap.at(idx_t(i)) + "\n";
  return out;
}

// The Runner (not the strategy) owns the step counter now, so the tests track it too: pushAll writes each
// input at the running step position and returns the resulting step count, which callers thread into
// toString()/deserializeFull() exactly as the Runner does.
static size_t pushAll(InputHistory& h, std::initializer_list<int> seq, size_t start = 0)
{
  size_t count = start;
  for (int i : seq) h.pushInput(count++, idx_t(i));
  return count;
}

// Round-trips a history through serializeCold/deserializeCold into `dst` (sharing any backing).
static void coldRoundTrip(const InputHistory& src, InputHistory& dst)
{
  std::vector<uint8_t>                 buf(src.getColdSize());
  jaffarCommon::serializer::Contiguous s(buf.data(), buf.size());
  src.serializeCold(s);
  EXPECT_EQ(s.getOutputSize(), src.getColdSize());
  jaffarCommon::deserializer::Contiguous d(buf.data(), buf.size());
  dst.deserializeCold(d);
}

static void fullRoundTrip(const InputHistory& src, InputHistory& dst, const size_t count)
{
  std::vector<uint8_t>                 buf(src.getFullSize());
  jaffarCommon::serializer::Contiguous s(buf.data(), buf.size());
  src.serializeFull(s);
  EXPECT_EQ(s.getOutputSize(), src.getFullSize());
  jaffarCommon::deserializer::Contiguous d(buf.data(), buf.size());
  dst.deserializeFull(d, count); // the runner supplies the step count; it is not in the stream
}

// --------------------------------------------------------------------------------------------------
// Factory (strict parsing)
// --------------------------------------------------------------------------------------------------

TEST(InputHistoryFactory, TypeAccessorAndBacking)
{
  EXPECT_EQ(getType(nlohmann::json{{"Type", "None"}}), "None");
  EXPECT_EQ(getType(nlohmann::json{{"Type", "Raw"}}), "Raw");
  EXPECT_EQ(getType(nlohmann::json{{"Type", "Trie"}}), "Trie");

  // Only the trie has a shared backing.
  EXPECT_NE(createSharedBacking(nlohmann::json{{"Type", "Trie"}}, 4), nullptr);
  EXPECT_EQ(createSharedBacking(nlohmann::json{{"Type", "Raw"}}, 4), nullptr);
  EXPECT_EQ(createSharedBacking(nlohmann::json{{"Type", "None"}}, 4), nullptr);
}

TEST(InputHistoryFactory, CreatesEachStrategy)
{
  { // None
    nlohmann::json cfg{{"Type", "None"}};
    auto           h = create(cfg, MAX_INPUT_INDEX, 4, 0, nullptr);
    size_t         n = pushAll(*h, {1, 2});
    EXPECT_EQ(n, 2u);
    EXPECT_EQ(h->toString(kMap, n), "");
  }
  { // Raw
    nlohmann::json cfg{{"Type", "Raw"}, {"Max Size", 50}};
    auto           h = create(cfg, MAX_INPUT_INDEX, 4, 0, nullptr);
    size_t         n = pushAll(*h, {1, 2, 3});
    EXPECT_EQ(h->toString(kMap, n), expectSeq({1, 2, 3}));
  }
  { // Trie
    nlohmann::json cfg{{"Type", "Trie"}, {"Max Size", 50}};
    auto           backing = createSharedBacking(cfg, 4);
    auto           h       = create(cfg, MAX_INPUT_INDEX, 4, 0, backing);
    size_t         n       = pushAll(*h, {1, 2, 3});
    EXPECT_EQ(h->toString(kMap, n), expectSeq({1, 2, 3}));
  }
}

TEST(InputHistoryFactory, RejectsBadConfig)
{
  EXPECT_ANY_THROW(create(nlohmann::json{{"Type", "Bogus"}}, MAX_INPUT_INDEX, 1, 0, nullptr));                              // unknown type
  EXPECT_ANY_THROW(create(nlohmann::json{{"Type", "Raw"}}, MAX_INPUT_INDEX, 1, 0, nullptr));                                // Raw needs Max Size
  EXPECT_ANY_THROW(create(nlohmann::json{{"Type", "Raw"}, {"Max Size", 4}, {"Bogus", 1}}, MAX_INPUT_INDEX, 1, 0, nullptr)); // stray key
  EXPECT_ANY_THROW(create(nlohmann::json{{"Type", "None"}, {"Max Size", 4}}, MAX_INPUT_INDEX, 1, 0, nullptr));              // None takes no Max Size
}

// --------------------------------------------------------------------------------------------------
// None
// --------------------------------------------------------------------------------------------------

TEST(InputHistoryNone, CountOnlyNoSolution)
{
  InputHistoryNone h;
  size_t           n = pushAll(h, {1, 2, 3, 0});
  EXPECT_EQ(n, 4u);                   // the test tracks the count (the strategy no longer does)
  EXPECT_EQ(h.toString(kMap, n), ""); // no inputs recorded
  EXPECT_EQ(h.getColdSize(), h.getFullSize());
  EXPECT_EQ(h.getColdSize(), 0u); // None stores no path bytes; the count lives in the runner

  InputHistoryNone other;
  coldRoundTrip(h, other); // a no-op for None (nothing to round-trip)

  h.reset();

  // Default manager ops are harmless no-ops.
  std::vector<uint8_t> slot(h.getColdSize());
  h.initColdSlot(slot.data());
  h.releaseColdSlot(slot.data(), 0);
  std::vector<uint8_t> full(h.getFullSize());
  std::vector<uint8_t> cold(h.getColdSize());
  h.captureColdToFull(cold.data(), full.data());
}

// --------------------------------------------------------------------------------------------------
// Raw
// --------------------------------------------------------------------------------------------------

TEST(InputHistoryRaw, RecordsAndReconstructs)
{
  InputHistoryRaw h(MAX_INPUT_INDEX, 100);
  size_t          n = pushAll(h, {1, 3, 2, 4, 0});
  EXPECT_EQ(n, 5u);
  EXPECT_EQ(h.toString(kMap, n), expectSeq({1, 3, 2, 4, 0}));
  EXPECT_EQ(h.getColdSize(), h.getFullSize()); // self-contained: cold == full

  InputHistoryRaw other(MAX_INPUT_INDEX, 100);
  coldRoundTrip(h, other);
  EXPECT_EQ(other.toString(kMap, n), expectSeq({1, 3, 2, 4, 0}));

  InputHistoryRaw third(MAX_INPUT_INDEX, 100);
  fullRoundTrip(h, third, n);
  EXPECT_EQ(third.toString(kMap, n), expectSeq({1, 3, 2, 4, 0}));
}

TEST(InputHistoryRaw, CaptureColdToFullEqualsCopy)
{
  InputHistoryRaw                      h(MAX_INPUT_INDEX, 100);
  size_t                               n = pushAll(h, {2, 2, 1});
  std::vector<uint8_t>                 cold(h.getColdSize());
  jaffarCommon::serializer::Contiguous s(cold.data(), cold.size());
  h.serializeCold(s);

  std::vector<uint8_t> full(h.getFullSize());
  h.captureColdToFull(cold.data(), full.data());

  InputHistoryRaw                        restored(MAX_INPUT_INDEX, 100);
  jaffarCommon::deserializer::Contiguous d(full.data(), full.size());
  restored.deserializeFull(d, n);
  EXPECT_EQ(restored.toString(kMap, n), expectSeq({2, 2, 1}));
}

TEST(InputHistoryRaw, TruncatesPastMaxSize)
{
  InputHistoryRaw h(MAX_INPUT_INDEX, 3); // only 3 inputs are recorded
  size_t          n = pushAll(h, {1, 2, 3, 4, 0});
  EXPECT_EQ(n, 5u);                                     // count tracks every push
  EXPECT_EQ(h.toString(kMap, n), expectSeq({1, 2, 3})); // but only the first Max Size are reconstructible
}

// --------------------------------------------------------------------------------------------------
// Trie (the focus)
// --------------------------------------------------------------------------------------------------

TEST(InputHistoryTrie, RecordsAndReconstructs)
{
  SequenceInputTrie trie(4);
  InputHistoryTrie  h(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  size_t            n = pushAll(h, {1, 3, 2, 4, 0});
  EXPECT_EQ(n, 5u);
  EXPECT_EQ(h.toString(kMap, n), expectSeq({1, 3, 2, 4, 0}));
  EXPECT_EQ(h.getColdSize(), sizeof(nodeId)); // just the node id now; the count lives in the runner
}

TEST(InputHistoryTrie, ColdRoundTripSharesTrie)
{
  SequenceInputTrie trie(4);
  InputHistoryTrie  a(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  size_t            n = pushAll(a, {1, 2, 3});

  InputHistoryTrie b(&trie, 1, 3, MAX_INPUT_INDEX, 100); // a different worker shard, same trie
  coldRoundTrip(a, b);
  EXPECT_EQ(n, 3u);
  EXPECT_EQ(b.toString(kMap, n), expectSeq({1, 2, 3}));
}

TEST(InputHistoryTrie, ExpandFromBaseSharesPrefix)
{
  // Mirrors the search: a base state is stored, then re-loaded and extended into several children, which
  // all share the base's prefix nodes.
  SequenceInputTrie trie(4);
  InputHistoryTrie  base(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  size_t            baseN = pushAll(base, {1, 2, 3}); // R R A (base depth 3)

  std::vector<uint8_t>                 slot(base.getColdSize());
  jaffarCommon::serializer::Contiguous s(slot.data(), slot.size());
  base.serializeCold(s); // the stored slot holds the "1 2 3" node

  // Child 1: load the base, append 4 (-> L) at the base's step.
  InputHistoryTrie child1(&trie, 1, 3, MAX_INPUT_INDEX, 100);
  {
    jaffarCommon::deserializer::Contiguous d(slot.data(), slot.size());
    child1.deserializeCold(d);
  }
  child1.pushInput(baseN, idx_t(4));
  EXPECT_EQ(child1.toString(kMap, baseN + 1), expectSeq({1, 2, 3, 4}));

  // Child 2: load the same base, append 0 (-> R). Independent leaf, shared prefix.
  InputHistoryTrie child2(&trie, 2, 3, MAX_INPUT_INDEX, 100);
  {
    jaffarCommon::deserializer::Contiguous d(slot.data(), slot.size());
    child2.deserializeCold(d);
  }
  child2.pushInput(baseN, idx_t(0));
  EXPECT_EQ(child2.toString(kMap, baseN + 1), expectSeq({1, 2, 3, 0}));

  // The stored base is unaffected by either child.
  InputHistoryTrie reload(&trie, 3, 3, MAX_INPUT_INDEX, 100);
  {
    jaffarCommon::deserializer::Contiguous d(slot.data(), slot.size());
    reload.deserializeCold(d);
  }
  EXPECT_EQ(reload.toString(kMap, baseN), expectSeq({1, 2, 3}));
}

TEST(InputHistoryTrie, FullFormIsSelfContained)
{
  SequenceInputTrie trie(4);
  InputHistoryTrie  a(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  size_t            n = pushAll(a, {4, 1, 1, 2});
  EXPECT_EQ(a.getFullSize(), ((100u * jaffarCommon::bitwise::getEncodingBitsForElementCount(MAX_INPUT_INDEX) + 7) / 8)); // bit-packed path only (no count)

  // serializeFull -> deserializeFull reconstructs the path, and the restored cursor can keep extending.
  InputHistoryTrie b(&trie, 1, 3, MAX_INPUT_INDEX, 100);
  fullRoundTrip(a, b, n);
  EXPECT_EQ(b.toString(kMap, n), expectSeq({4, 1, 1, 2}));
  b.pushInput(n, idx_t(3));
  EXPECT_EQ(b.toString(kMap, n + 1), expectSeq({4, 1, 1, 2, 3}));
}

TEST(InputHistoryTrie, CaptureColdToFullForSnapshots)
{
  // captureColdToFull is the best/worst-snapshot path: a stored cold slot -> a self-contained full buffer.
  SequenceInputTrie trie(4);
  InputHistoryTrie  a(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  size_t            n = pushAll(a, {1, 4, 2});

  std::vector<uint8_t>                 cold(a.getColdSize());
  jaffarCommon::serializer::Contiguous s(cold.data(), cold.size());
  a.serializeCold(s);

  std::vector<uint8_t> full(a.getFullSize());
  a.captureColdToFull(cold.data(), full.data());

  InputHistoryTrie                       restored(&trie, 1, 3, MAX_INPUT_INDEX, 100);
  jaffarCommon::deserializer::Contiguous d(full.data(), full.size());
  restored.deserializeFull(d, n);
  EXPECT_EQ(restored.toString(kMap, n), expectSeq({1, 4, 2}));
}

TEST(InputHistoryTrie, InitAndReleaseColdSlot)
{
  SequenceInputTrie trie(4);
  InputHistoryTrie  h(&trie, 0, 3, MAX_INPUT_INDEX, 100);

  std::vector<uint8_t> slot(h.getColdSize());
  h.initColdSlot(slot.data());
  EXPECT_EQ(*reinterpret_cast<nodeId*>(slot.data()), SequenceInputTrie::NONE);
  h.releaseColdSlot(slot.data(), 0); // releasing an empty (NONE) slot is a no-op

  // Store a real node, then release the slot: its trie reference is dropped and the slot reset to NONE.
  pushAll(h, {1, 2});
  jaffarCommon::serializer::Contiguous s(slot.data(), slot.size());
  h.serializeCold(s);
  EXPECT_NE(*reinterpret_cast<nodeId*>(slot.data()), SequenceInputTrie::NONE);
  h.releaseColdSlot(slot.data(), 0);
  EXPECT_EQ(*reinterpret_cast<nodeId*>(slot.data()), SequenceInputTrie::NONE);
}

TEST(InputHistoryTrie, EmptyPath)
{
  SequenceInputTrie trie(4);
  InputHistoryTrie  h(&trie, 0, 3, MAX_INPUT_INDEX, 100);
  EXPECT_EQ(h.toString(kMap, 0), "");
  InputHistoryTrie b(&trie, 1, 3, MAX_INPUT_INDEX, 100);
  coldRoundTrip(h, b);
  EXPECT_EQ(b.toString(kMap, 0), "");
}

TEST(InputHistory, RawAndTrieAgree)
{
  // The two reconstructing strategies must yield identical solutions for the same input sequence.
  SequenceInputTrie trie(2);
  InputHistoryTrie  tr(&trie, 0, 1, MAX_INPUT_INDEX, 100);
  InputHistoryRaw   rw(MAX_INPUT_INDEX, 100);
  size_t            c = 0;
  for (int x : {1, 3, 2, 4, 0, 1, 1, 2, 3})
  {
    tr.pushInput(c, idx_t(x));
    rw.pushInput(c, idx_t(x));
    c++;
  }
  EXPECT_EQ(tr.toString(kMap, c), rw.toString(kMap, c));
}

// --------------------------------------------------------------------------------------------------
// Underlying SequenceTrie: sharing, reference-count GC and concurrent sharded operation
// --------------------------------------------------------------------------------------------------

TEST(SequenceTrie, ExtendAndReconstruct)
{
  SequenceInputTrie t(1);
  nodeId            n = SequenceInputTrie::ROOT;
  for (int x : {1, 2, 3}) n = t.extend(n, idx_t(x), 0);
  EXPECT_EQ(t.getDepth(n), 3u);
  std::vector<idx_t> seq;
  t.reconstruct(n, seq);
  ASSERT_EQ(seq.size(), 3u);
  EXPECT_EQ(seq[0], idx_t(1));
  EXPECT_EQ(seq[1], idx_t(2));
  EXPECT_EQ(seq[2], idx_t(3));
  t.release(n, 0);
}

TEST(SequenceTrie, SiblingsShareParentChain)
{
  // extend() never dedups; sharing comes from extending two children off the SAME parent node.
  SequenceInputTrie t(1);
  nodeId            prefix = t.extend(t.extend(SequenceInputTrie::ROOT, idx_t(1), 0), idx_t(2), 0); // "1 2"
  nodeId            c1     = t.extend(prefix, idx_t(3), 0);
  nodeId            c2     = t.extend(prefix, idx_t(4), 0);
  EXPECT_NE(c1, c2);

  std::vector<idx_t> s1, s2;
  t.reconstruct(c1, s1);
  t.reconstruct(c2, s2);
  ASSERT_EQ(s1.size(), 3u);
  ASSERT_EQ(s2.size(), 3u);
  EXPECT_EQ(s1[0], idx_t(1)); // shared prefix
  EXPECT_EQ(s1[1], idx_t(2));
  EXPECT_EQ(s1[2], idx_t(3));
  EXPECT_EQ(s2[2], idx_t(4));
  t.release(c1, 0);
  t.release(c2, 0);
  t.release(prefix, 0);
}

TEST(SequenceTrie, ReferenceCountRecyclesNodes)
{
  // Repeatedly building and releasing a path must recycle nodes -- memory reaches a steady state and does
  // not grow with more cycles (reported memory tracks the live-node high-water mark, which the recycler
  // keeps flat once a depth-8 path's worth of nodes has been allocated once).
  SequenceInputTrie t(1);
  auto              buildAndFree = [&](int reps)
  {
    for (int i = 0; i < reps; i++)
    {
      nodeId n = SequenceInputTrie::ROOT;
      for (int d = 0; d < 8; d++)
      {
        const nodeId next = t.extend(n, idx_t(d % MAX_INPUT_INDEX), 0);
        if (n != SequenceInputTrie::ROOT) t.release(n, 0); // drop the prior node's owning ref as we advance
        n = next;
      }
      t.release(n, 0); // releasing the leaf cascades up, freeing the whole chain
    }
  };

  buildAndFree(100); // reach the steady-state high-water mark
  const size_t mem = t.getApproxMemoryBytes();
  buildAndFree(100000);                     // 100k more build/free cycles
  EXPECT_EQ(t.getApproxMemoryBytes(), mem); // fully recycled: no growth
}

TEST(SequenceTrie, ConcurrentShardedExtendRelease)
{
  // Hammer the shared trie from many threads, each on its own free-list shard (the contention-free path).
  // Each thread builds and frees independent paths; the result must stay consistent and leak nothing.
  constexpr uint32_t NTHREADS = 8;
  SequenceInputTrie  t(NTHREADS);
  std::atomic<bool>  ok{true};

  std::vector<std::thread> threads;
  for (uint32_t s = 0; s < NTHREADS; s++)
    threads.emplace_back(
        [&, s]()
        {
          for (int i = 0; i < 4000; i++)
          {
            nodeId n = SequenceInputTrie::ROOT;
            for (int d = 0; d < 10; d++)
            {
              const nodeId next = t.extend(n, idx_t((s + d) % MAX_INPUT_INDEX), s);
              if (n != SequenceInputTrie::ROOT) t.release(n, s); // release the prior node as we advance
              n = next;
            }
            std::vector<idx_t> seq;
            t.reconstruct(n, seq);
            if (seq.size() != 10) ok.store(false);
            t.release(n, s); // leaf release cascades up, freeing this thread's whole chain
          }
        });
  for (auto& th : threads) th.join();
  EXPECT_TRUE(ok.load());
}

// Raw moves only the used prefix of its buffer through a slot (the engine passes the path length): a slot
// whose bytes beyond that prefix hold leftovers of a longer, earlier path must still restore exactly the
// zero-padded history, and capturing it into a full snapshot must zero the leftovers too.
TEST(InputHistoryRaw, ColdPrefixIgnoresSlotLeftovers)
{
  InputHistoryRaw      longer(MAX_INPUT_INDEX, 64), shorter(MAX_INPUT_INDEX, 64), reloaded(MAX_INPUT_INDEX, 64), reference(MAX_INPUT_INDEX, 64);
  std::vector<uint8_t> slot(longer.getColdSize());

  // A longer path is stored first and leaves its bytes in the slot
  const size_t longCount = pushAll(longer, {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4});
  {
    jaffarCommon::serializer::Contiguous s(slot.data(), slot.size());
    longer.serializeCold(s, longCount);
  }

  // Then a shorter one overwrites only its own prefix
  const size_t shortCount = pushAll(shorter, {1, 2, 3});
  {
    jaffarCommon::serializer::Contiguous s(slot.data(), slot.size());
    shorter.serializeCold(s, shortCount);
    EXPECT_LT(s.getOutputSize(), slot.size());
  }

  // A cursor that held the long path reloads the short one: same string, same canonical full form
  {
    pushAll(reloaded, {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4});
    jaffarCommon::deserializer::Contiguous d(slot.data(), slot.size());
    reloaded.deserializeCold(d, shortCount);
  }
  EXPECT_EQ(reloaded.toString(kMap, shortCount), expectSeq({1, 2, 3}));
  pushAll(reference, {1, 2, 3});
  std::vector<uint8_t> a(reference.getFullSize()), b(reloaded.getFullSize());
  {
    jaffarCommon::serializer::Contiguous s(a.data(), a.size());
    reference.serializeFull(s);
  }
  {
    jaffarCommon::serializer::Contiguous s(b.data(), b.size());
    reloaded.serializeFull(s);
  }
  EXPECT_EQ(a, b);

  // A snapshot of the slot at the short length is the zero-padded history as well
  std::vector<uint8_t> full(reference.getFullSize(), 0xFF);
  reference.captureColdToFull(slot.data(), full.data(), shortCount);
  EXPECT_EQ(full, a);
}
