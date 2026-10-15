#pragma once

/**
 * @file inputHistory.hpp
 * @brief Abstract interface for how a search remembers the sequence of inputs ("path") that produced
 *        each state. The rest of JaffarPlus only ever talks to this base class; concrete strategies
 *        (none / raw bit-packed / shared prefix trie) live in their own headers and are selected from
 *        the .jaffar config. See inputHistoryFactory.hpp for construction.
 *
 * Two groups of operations live on this one interface:
 *  - per-runner "cursor" ops (pushInput, (de)serialize, toString): each Runner owns an instance and
 *    advances it as it explores.
 *  - "manager" ops on stored cold-slot bytes (initColdSlot, releaseColdSlot, captureColdToFull, sizes):
 *    these only use the shared backing (e.g. the trie), so the StateDb calls them on a reference instance.
 *
 * Cold vs full: the StateDb stores a compact "cold" representation in each state slot; standalone
 * snapshot buffers (best/worst/win, player initial state) hold a self-contained "full" representation.
 * For raw/none the two coincide; for the trie the cold form is an 8-byte node id while the full form is a
 * reconstructed, self-contained copy.
 */

#include "../inputSet.hpp"
#include <cstdint>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <map>
#include <memory>
#include <string>

namespace jaffarPlus
{

/**
 * @brief Abstract strategy for remembering the input path that produced each search state.
 *
 * Concrete strategies (none / raw bit-packed / shared-prefix trie) implement the per-runner cursor ops
 * and the manager ops on stored cold-slot bytes; see the file header and inputHistoryFactory.hpp.
 */
class InputHistory
{
public:
  /// @brief Virtual destructor for the strategy interface.
  virtual ~InputHistory() = default;

  // ---- per-runner cursor ---------------------------------------------------------------------------
  //
  // NOTE: the step counter is NOT owned here. The Runner owns the count (it is what implements Hash Step
  // Tolerance); it is not serialized at all: whoever restores a state sets it first (every state of a search
  // step has the same depth, so the engine sets it from the step number; snapshots record it). Strategies
  // that need the current step (raw's write position and used length, the trie's reconstruction length)
  // receive it as a parameter; they never store or serialize it themselves.

  /// @brief Resets the cursor to the empty path.
  virtual void reset() = 0;

  /// @brief Records one applied input at path position @p stepCount (the runner's current step counter).
  virtual void pushInput(const size_t stepCount, const InputSet::inputIndex_t input) = 0;

  /// @brief Writes the compact "cold" path representation (stored in each StateDb slot), excluding the count.
  /// @param s The serializer to write into.
  /// @param stepCount The path length: a strategy may write only the part it uses (raw writes the used
  ///        prefix of its buffer); the default writes the whole representation.
  virtual void serializeCold(jaffarCommon::serializer::Base& s, const size_t stepCount = SIZE_MAX) const = 0;
  /// @brief Restores the cursor from a "cold" representation (the count is set by the runner).
  /// @param d The deserializer to read from.
  /// @param stepCount The path length (as given to serializeCold); the default reads the whole representation.
  virtual void deserializeCold(jaffarCommon::deserializer::Base& d, const size_t stepCount = SIZE_MAX) = 0;

  /// @brief Writes the self-contained "full" path representation (for standalone snapshot buffers), excluding the count.
  virtual void serializeFull(jaffarCommon::serializer::Base& s) const = 0;
  /// @brief Restores the cursor from a "full" representation. @p stepCount is the runner's already-restored
  /// step counter, needed by strategies (the trie) that rebuild their cursor from the path length.
  virtual void deserializeFull(jaffarCommon::deserializer::Base& d, const size_t stepCount) = 0;

  /// @brief Reconstructs the path as a newline-separated string of input strings (the solution). @p stepCount
  /// is the runner's step counter, bounding how many recorded steps are rendered.
  virtual std::string toString(const std::map<InputSet::inputIndex_t, std::string>& inputStringMap, const size_t stepCount) const = 0;

  // ---- manager ops on stored bytes (use the shared backing only) -----------------------------------

  /// @brief Size, in bytes, of the cold path representation in a StateDb slot (EXCLUDING the runner's count).
  virtual size_t getColdSize() const = 0;
  /// @brief Size, in bytes, of the full (self-contained) path representation in a snapshot buffer (EXCLUDING the count).
  virtual size_t getFullSize() const = 0;

  /// @brief Prepares a fresh/recycled cold slot (e.g. marks it as holding no trie node). Default: no-op.
  virtual void initColdSlot(void* cold) const { (void)cold; }
  /// @brief Releases any shared resource a freed cold slot was holding (trie GC). @p shard is the
  /// freeing thread's id, used to recycle into a contention-free per-thread pool. Default: no-op.
  virtual void releaseColdSlot(void* cold, const size_t shard) const
  {
    (void)cold;
    (void)shard;
  }
  /// @brief Converts a stored cold path into a self-contained full one (best/worst snapshot). Operates on the
  /// path only. @p stepCount is the path length (as given to serializeCold; the default: the whole cold form).
  virtual void captureColdToFull(const void* cold, void* full, const size_t stepCount = SIZE_MAX) const = 0;

  /// @brief Approximate resident memory of any shared structure (e.g. the trie), in bytes. Default: 0.
  virtual size_t getApproxMemoryBytes() const { return 0; }
};

} // namespace jaffarPlus
