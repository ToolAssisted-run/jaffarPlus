#pragma once

/**
 * @file inputHistoryTrie.hpp
 * @brief Input-history strategy that stores each state's path as a single node id into a shared,
 *        reference-counted prefix trie (jaffarCommon::sequenceTrie). Sibling states share their common
 *        prefix, so per-state cold storage is just an 8-byte node id and total path memory is far smaller
 *        than the raw bit-packed history. Selected with `{"Type": "Trie", "Max Size": N}` ("Max Size"
 *        bounds only the reconstructed snapshot/solution buffer; the live search path is unbounded), plus
 *        the optional "Max Memory (Mb)" cap on the trie's node storage.
 *
 * Cold form (StateDb slot): [node id]. Full form (self-contained snapshot): the path reconstructed into a
 * bit-packed buffer, so snapshots need no reference back into the trie. The runner's step counter is stored
 * separately (prefixed by the runner), not by this strategy.
 *
 * UNBOUNDED GROWTH + CEILING (watch this): unlike Raw, the trie is a SEPARATE shared pool NOT counted
 * in the State DB budget; it grows ~ live-states x depth (16 bytes per node) and is capped at its node
 * ceiling ("Max Memory (Mb)", or by default the RAM the engine's guard finds left after the databases;
 * SequenceTrie::getMaxMemoryBytes). When that ceiling is reached the
 * trie soft-fails (extend() returns NONE, isExhausted() latches) rather than throwing inside the parallel
 * region, and the driver stops the run gracefully (exit reason inputHistoryNearCapacity). Because prefix
 * sharing fades with depth, Raw (per-state cost fixed by Max Size, bounded by the State DB) can beat Trie
 * on very deep searches -- compare the per-step "Input History (shared) ... raw would be ..." figures.
 */

#include "inputHistory.hpp"
#include <atomic>
#include <cstring>
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/sequenceTrie.hpp>
#include <vector>

namespace jaffarPlus
{

/// @brief The shared trie type backing every InputHistoryTrie instance of one search.
/// The trie stores each path element as a 16-bit input index (not the full size_t inputIndex_t), which with
/// the 16-bit refCount and the 8-byte parent id makes a node 16 bytes. Valid because a search registers far
/// fewer than 65536 distinct inputs (the constructor enforces this).
using SequenceInputTrie = jaffarCommon::sequenceTrie::SequenceTrie<uint16_t>;

/// @brief Stores each state's path as one node id into a shared, reference-counted prefix trie; sibling
/// states share their common prefix, so per-state cold storage is just an 8-byte node id. `{"Type":"Trie"}`.
class InputHistoryTrie final : public InputHistory
{
public:
  using nodeId_t = SequenceInputTrie::nodeId_t; ///< Trie node identifier type.

  /// @brief Binds this instance to the shared trie and sizes the bit-packed snapshot (full) form.
  /// @param trie          The shared trie (created once per search; outlives all instances).
  /// @param shardId       This runner's contention-free free-list shard (its worker thread id).
  /// @param managerShard  A shard reserved for StateDb/driver-thread operations (captureColdToFull).
  /// @param maxInputIndex One past the highest input index (sets the snapshot's per-step bit width).
  /// @param maxSize       Maximum steps held in the reconstructed snapshot (full) form.
  InputHistoryTrie(SequenceInputTrie* trie, const uint32_t shardId, const uint32_t managerShard, const uint32_t maxInputIndex, const uint32_t maxSize)
      : _trie(trie), _shardId(shardId), _managerShard(managerShard), _maxSize(maxSize)
  {
    // The trie node stores the input index as a 16-bit element (see SequenceInputTrie). maxInputIndex is one
    // past the highest input index, so it must fit in 16 bits. Real searches register a few hundred inputs
    // at most; if a game ever exceeds this, use Type "Raw" instead.
    if (maxInputIndex > 0x10000u) JAFFAR_THROW_RUNTIME("Trie input-history supports at most 65536 distinct inputs (got %u); use Store Input History Type \"Raw\".", maxInputIndex);
    _bits         = jaffarCommon::bitwise::getEncodingBitsForElementCount(maxInputIndex);
    _bitpackBytes = (_maxSize * _bits + 7) / 8;
    _scratch.resize(_bitpackBytes, 0);
  }

  void reset() override
  {
    if (_ownNode) _trie->release(_node, _shardId);
    _node    = SequenceInputTrie::ROOT;
    _ownNode = false;
  }

  void pushInput(const size_t /*stepCount*/, const InputSet::inputIndex_t input) override
  {
    // Append as a new owned node; the prior owned node (e.g. a frameskip sub-frame) is kept alive by the
    // new node's child edge, so drop this runner's transient reference to it. The trie is unbounded.
    const nodeId_t next = _trie->extend(_node, (uint16_t)input, _shardId); // input < maxInputIndex <= 65536 (ctor-checked)
    if (_ownNode) _trie->release(_node, _shardId);
    _node    = next;
    _ownNode = true;
  }

  void serializeCold(jaffarCommon::serializer::Base& s) const override
  {
    // _node may be NONE if the trie hit its capacity ceiling (extend returned NONE); acquiring NONE would
    // index out of bounds. Skip it -- releaseColdSlot() already skips NONE too, so the refcount stays
    // balanced. (The run is stopped gracefully by the driver's capacity guard right after this anyway.)
    if (_node != SequenceInputTrie::NONE) _trie->acquire(_node); // the destination slot becomes a holder of this path node
    s.pushContiguous(&_node, sizeof(_node));
  }
  void deserializeCold(jaffarCommon::deserializer::Base& d) override
  {
    if (_ownNode) _trie->release(_node, _shardId); // drop a just-discarded candidate node, if any
    d.popContiguous(&_node, sizeof(_node));
    _ownNode = false; // borrowed from the slot (kept alive until the slot is freed)
  }

  void serializeFull(jaffarCommon::serializer::Base& s) const override
  {
    reconstructIntoBuffer(_node, _scratch.data()); // self-contained bit-packed copy (no trie reference)
    s.pushContiguous(_scratch.data(), _bitpackBytes);
  }
  void deserializeFull(jaffarCommon::deserializer::Base& d, const size_t stepCount) override
  {
    d.popContiguous(_scratch.data(), _bitpackBytes);
    rebuildNodeFromBuffer(_scratch.data(), stepCount); // so a restored runner (e.g. the player) can keep extending
  }

  std::string toString(const std::map<InputSet::inputIndex_t, std::string>& inputStringMap, const size_t /*stepCount*/) const override
  {
    std::vector<InputSet::inputIndex_t> seq;
    _trie->reconstruct(_node, seq); // the node fully encodes its path, so seq.size() is the true step count
    warnIfTruncated(seq.size());
    std::string out;
    for (size_t i = 0; i < seq.size() && i < _maxSize; i++)
    {
      if (inputStringMap.contains(seq[i]) == false) JAFFAR_THROW_RUNTIME("Move Index %lu not found in runner\n", (unsigned long)seq[i]);
      out += inputStringMap.at(seq[i]) + std::string("\n");
    }
    return out;
  }

  size_t getColdSize() const override { return sizeof(_node); }
  size_t getFullSize() const override { return _bitpackBytes; }

  void initColdSlot(void* cold) const override { *reinterpret_cast<nodeId_t*>(cold) = SequenceInputTrie::NONE; }

  size_t getApproxMemoryBytes() const override { return _trie->getApproxMemoryBytes(); }

  void releaseColdSlot(void* cold, const size_t shard) const override
  {
    auto* node = reinterpret_cast<nodeId_t*>(cold);
    if (*node != SequenceInputTrie::NONE)
    {
      _trie->release(*node, (uint32_t)shard);
      *node = SequenceInputTrie::NONE;
    }
  }

  void captureColdToFull(const void* cold, void* full) const override
  {
    // Cold path = [node id]; full path = [bit-packed history]. Reconstruct the node's path into the full
    // buffer so the snapshot is self-contained (no later reference into the trie). The runner's step-count
    // prefix that precedes both is copied separately by the StateDb.
    nodeId_t node;
    std::memcpy(&node, cold, sizeof(node));
    reconstructIntoBuffer(node, reinterpret_cast<uint8_t*>(full), /*pin=*/true);
  }

private:
  /// @brief Reconstructs @p node's path into a bit-packed @p buffer. When @p pin, briefly holds a
  /// reference so a worker freeing the source slot cannot free the node mid-walk (best/worst snapshots
  /// run off the search threads).
  void reconstructIntoBuffer(nodeId_t node, uint8_t* buffer, bool pin = false) const
  {
    std::memset(buffer, 0, _bitpackBytes);
    if (node == SequenceInputTrie::NONE) return;
    if (pin) _trie->acquire(node);
    std::vector<InputSet::inputIndex_t> seq; // reconstruct widens the trie's 16-bit elements into these
    _trie->reconstruct(node, seq);
    if (pin) _trie->release(node, _managerShard);
    warnIfTruncated(seq.size());
    for (size_t i = 0; i < seq.size() && i < _maxSize; i++) jaffarCommon::bitwise::bitcopy(buffer, _bitpackBytes, i, &seq[i], sizeof(InputSet::inputIndex_t), 0, 1, _bits);
  }

  /// @brief Warns (once per run) when a path is longer than the snapshot/solution form holds ("Max Size").
  void warnIfTruncated(const size_t length) const
  {
    static std::atomic<bool> warned{false};
    if (length > _maxSize && warned.exchange(true) == false)
      jaffarCommon::logger::log("[J+] Warning: input history exceeded its maximum size (%u). Longer solutions are truncated; raise 'Store Input History / Max Size'.\n", _maxSize);
  }

  /// @brief Rebuilds the cursor's owned trie node by re-extending the @p stepCount path elements stored in a
  /// bit-packed @p buffer (so a restored runner, e.g. the player, can keep extending the path).
  void rebuildNodeFromBuffer(const uint8_t* buffer, const size_t stepCount)
  {
    if (_ownNode) _trie->release(_node, _shardId);
    _node              = SequenceInputTrie::ROOT;
    _ownNode           = false;
    const size_t steps = (stepCount < _maxSize) ? stepCount : _maxSize;
    for (size_t i = 0; i < steps; i++)
    {
      InputSet::inputIndex_t idx = 0;
      jaffarCommon::bitwise::bitcopy(&idx, sizeof(InputSet::inputIndex_t), 0, buffer, _bitpackBytes, i, 1, _bits);
      const nodeId_t next = _trie->extend(_node, (uint16_t)idx, _shardId);
      if (_ownNode) _trie->release(_node, _shardId);
      _node    = next;
      _ownNode = true;
    }
  }

  SequenceInputTrie* const     _trie;                                   ///< The shared trie backing this instance.
  const uint32_t               _shardId;                                ///< This runner's free-list shard.
  const uint32_t               _managerShard;                           ///< Shard for off-thread manager ops.
  const uint32_t               _maxSize;                                ///< Max steps in the snapshot (full) form.
  size_t                       _bits         = 0;                       ///< Bits per input index in the snapshot.
  size_t                       _bitpackBytes = 0;                       ///< Size of the bit-packed snapshot buffer.
  nodeId_t                     _node         = SequenceInputTrie::ROOT; ///< Cursor's current trie node.
  bool                         _ownNode      = false;                   ///< Whether this cursor holds a reference to _node.
  mutable std::vector<uint8_t> _scratch;                                ///< Bit-pack scratch for the full (snapshot) form.
};

} // namespace jaffarPlus
