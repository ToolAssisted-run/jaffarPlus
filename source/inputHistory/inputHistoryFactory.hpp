#pragma once

/**
 * @file inputHistoryFactory.hpp
 * @brief Builds the configured InputHistory strategy from the "Store Input History" config object.
 *        The shared backing (the trie, for the "Trie" strategy) is created once per search via
 *        createSharedBacking() and then bound into each runner's instance via create(). None/Raw
 *        have no shared backing. Default type when unspecified: "Trie".
 */

#include "inputHistory.hpp"
#include "inputHistoryNone.hpp"
#include "inputHistoryRaw.hpp"
#include "inputHistoryTrie.hpp"
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/json.hpp>

namespace jaffarPlus
{

namespace inputHistory
{

/// @brief The configured strategy name ("None" | "Raw" | "Trie"). Required -- JaffarPlus configs must set
/// every option explicitly, so an absent "Type" is an error (thrown by the strict accessor).
__INLINE__ std::string getType(const nlohmann::json& config) { return jaffarCommon::json::getString(config, "Type"); }

/**
 * @brief Creates the shared backing for the configured strategy (the trie for "Trie"; nullptr otherwise).
 * @param config    The "Store Input History" configuration object (its "Type" selects the strategy).
 * @param numShards Number of contention-free free-list shards (one per worker thread + one for the
 *                  StateDb/driver manager operations).
 * @return An opaque shared handle held by the caller for the lifetime of the search; pass it to create().
 */
__INLINE__ std::shared_ptr<void> createSharedBacking(const nlohmann::json& config, const uint32_t numShards)
{
  if (getType(config) == "Trie") return std::make_shared<SequenceInputTrie>(numShards);
  return nullptr;
}

/// @brief The configured cap on the trie's node storage ("Max Memory (Mb)", Trie only), in bytes; 0 if not set.
__INLINE__ size_t getConfiguredMaxMemoryBytes(const nlohmann::json& config)
{
  if (getType(config) != "Trie" || config.contains("Max Memory (Mb)") == false) return 0;
  return jaffarCommon::json::getNumber<size_t>(config, "Max Memory (Mb)") * 1024ul * 1024ul;
}

/// @brief Sets the cap on the shared backing's memory (the trie's node storage); no-op for None/Raw.
/// Call before the search starts.
__INLINE__ void setSharedBackingMaxMemoryBytes(const std::shared_ptr<void>& backing, const size_t bytes)
{
  if (backing != nullptr) static_cast<SequenceInputTrie*>(backing.get())->setMaxMemoryBytes(bytes);
}

/// @brief Hard upper bound (bytes) on the shared backing's memory: the trie's node-storage ceiling for the
/// "Trie" strategy, 0 for None/Raw (their per-state history lives in the StateDb slot, already budgeted).
/// Used by the engine's RAM guard to account for this otherwise-uncounted, unbounded-growth structure.
__INLINE__ size_t getSharedBackingMaxMemoryBytes(const std::shared_ptr<void>& backing)
{
  if (backing == nullptr) return 0;
  return static_cast<SequenceInputTrie*>(backing.get())->getMaxMemoryBytes();
}

/// @brief Current (approximate) live memory of the shared backing: the trie's node-storage footprint for
/// the "Trie" strategy, 0 for None/Raw. The driver polls this against the ceiling to stop gracefully before
/// the trie's hard cap is hit.
__INLINE__ size_t getSharedBackingApproxMemoryBytes(const std::shared_ptr<void>& backing)
{
  if (backing == nullptr) return 0;
  return static_cast<SequenceInputTrie*>(backing.get())->getApproxMemoryBytes();
}

/// @brief True if the shared backing (the trie) has hit its hard node-storage ceiling. False for None/Raw
/// (which have no such ceiling -- their history lives in the StateDb slot, already budgeted).
__INLINE__ bool isSharedBackingExhausted(const std::shared_ptr<void>& backing)
{
  if (backing == nullptr) return false;
  return static_cast<SequenceInputTrie*>(backing.get())->isExhausted();
}

/**
 * @brief Creates one runner's input-history instance bound to @p backing.
 * @param config        The "Store Input History" configuration object ("Type" plus any per-strategy keys).
 * @param maxInputIndex One past the highest input index (per-step bit width for raw/snapshot encoding).
 * @param numShards     Shard count the backing was created with (the last shard is the manager shard).
 * @param shardId       This runner's free-list shard (its worker thread id; 0 for standalone runners).
 * @param backing       The shared handle from createSharedBacking() (ignored by None/Raw).
 */
__INLINE__ std::unique_ptr<InputHistory> create(const nlohmann::json& config, const uint32_t maxInputIndex, const uint32_t numShards, const uint32_t shardId,
                                                const std::shared_ptr<void>& backing)
{
  // Strictly consume the config: pop the recognized keys, then reject any leftover (unknown) key.
  auto                          cfg  = config;
  const auto                    type = jaffarCommon::json::popString(cfg, "Type");
  std::unique_ptr<InputHistory> result;
  if (type == "None")
    result = std::make_unique<InputHistoryNone>();
  else if (type == "Raw")
    result = std::make_unique<InputHistoryRaw>(maxInputIndex, jaffarCommon::json::popNumber<uint32_t>(cfg, "Max Size"));
  else if (type == "Trie")
  {
    result = std::make_unique<InputHistoryTrie>(static_cast<SequenceInputTrie*>(backing.get()), shardId, numShards - 1, maxInputIndex,
                                                jaffarCommon::json::popNumber<uint32_t>(cfg, "Max Size"));
    if (cfg.contains("Max Memory (Mb)")) jaffarCommon::json::popNumber<size_t>(cfg, "Max Memory (Mb)"); // read by the engine (getConfiguredMaxMemoryBytes)
  }
  else
    JAFFAR_THROW_LOGIC("Unrecognized 'Store Input History' Type: '%s' (expected None, Raw or Trie)", type.c_str());
  jaffarCommon::json::checkEmpty(cfg, "Runner Configuration > Store Input History");
  return result;
}

} // namespace inputHistory

} // namespace jaffarPlus
