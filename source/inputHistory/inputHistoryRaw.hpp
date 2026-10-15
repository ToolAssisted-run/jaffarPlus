#pragma once

/**
 * @file inputHistoryRaw.hpp
 * @brief Input-history strategy that stores the full bit-packed input sequence in every state. This is
 *        JaffarPlus's classic behavior. Self-contained per state (no shared structure), but the largest
 *        per-state footprint. Selected with `{"Type": "Raw", "Max Size": N}`.
 */

#include "inputHistory.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/logger.hpp>
#include <vector>

namespace jaffarPlus
{

/// @brief Stores the full bit-packed input sequence in every state (JaffarPlus's classic behavior):
/// self-contained per state, but the largest per-state footprint. Selected with `{"Type": "Raw", ...}`.
class InputHistoryRaw final : public InputHistory
{
public:
  /// @brief Constructs the strategy and sizes the per-state bit-packed buffer.
  /// @param maxInputIndex One past the highest input index (sets the per-step bit width).
  /// @param maxSize       Maximum number of steps recorded; longer solutions are truncated.
  InputHistoryRaw(const uint32_t maxInputIndex, const uint32_t maxSize) : _maxSize(maxSize)
  {
    _bits              = jaffarCommon::bitwise::getEncodingBitsForElementCount(maxInputIndex);
    const size_t bytes = (_maxSize * _bits + 7) / 8;
    _buffer.resize(bytes, 0);
  }

  void reset() override
  {
    std::memset(_buffer.data(), 0, _buffer.size());
    _usedBytes = 0;
  }

  void pushInput(const size_t stepCount, const InputSet::inputIndex_t input) override
  {
    if (stepCount < _maxSize)
    {
      setInput(stepCount, input);
      _usedBytes = std::max(_usedBytes, getPrefixBytes(stepCount + 1));
    }
    else if (_truncationWarned.exchange(true) == false)
      jaffarCommon::logger::log("[J+] Warning: input history exceeded its maximum size (%u). Longer solutions are truncated; raise 'Store Input History / Max Size'.\n", _maxSize);
  }

  // A state of depth n uses only the first getPrefixBytes(n) bytes of the buffer (the rest is zero), so
  // the cold form moves just that prefix: the bytes beyond it in a slot are left over from earlier states
  // and never read. The cursor's own buffer is kept zero beyond its used bytes, so the full form (and with
  // it every serialized state) stays exactly the zero-padded buffer.
  void serializeCold(jaffarCommon::serializer::Base& s, const size_t stepCount = SIZE_MAX) const override { s.pushContiguous(_buffer.data(), getPrefixBytes(stepCount)); }
  void deserializeCold(jaffarCommon::deserializer::Base& d, const size_t stepCount = SIZE_MAX) override
  {
    const size_t prefix = getPrefixBytes(stepCount);
    d.popContiguous(_buffer.data(), prefix);
    if (_usedBytes > prefix) std::memset(_buffer.data() + prefix, 0, _usedBytes - prefix);
    _usedBytes = prefix;
  }
  void serializeFull(jaffarCommon::serializer::Base& s) const override { s.pushContiguous(_buffer.data(), _buffer.size()); }
  void deserializeFull(jaffarCommon::deserializer::Base& d, const size_t /*stepCount*/) override
  {
    d.popContiguous(_buffer.data(), _buffer.size());
    _usedBytes = _buffer.size();
  }

  std::string toString(const std::map<InputSet::inputIndex_t, std::string>& inputStringMap, const size_t stepCount) const override
  {
    std::string out;
    for (size_t i = 0; i < stepCount && i < _maxSize; i++)
    {
      const auto idx = getInput(i);
      if (inputStringMap.contains(idx) == false) JAFFAR_THROW_RUNTIME("Move Index %lu not found in runner\n", idx);
      out += inputStringMap.at(idx) + std::string("\n");
    }
    return out;
  }

  size_t getColdSize() const override { return _buffer.size(); }
  size_t getFullSize() const override { return _buffer.size(); }

  void captureColdToFull(const void* cold, void* full, const size_t stepCount = SIZE_MAX) const override
  {
    const size_t prefix = getPrefixBytes(stepCount);
    memcpy(full, cold, prefix);
    std::memset((uint8_t*)full + prefix, 0, _buffer.size() - prefix);
  }

private:
  /// @brief Bytes of the buffer that a path of @p stepCount inputs uses (the whole buffer from Max Size on).
  __INLINE__ size_t getPrefixBytes(const size_t stepCount) const { return stepCount >= _maxSize ? _buffer.size() : std::min(_buffer.size(), (stepCount * _bits + 7) / 8); }

  /// @brief Writes the input index for @p step into the bit-packed buffer.
  __INLINE__ void setInput(const size_t step, const InputSet::inputIndex_t input)
  {
    jaffarCommon::bitwise::bitcopy(_buffer.data(), _buffer.size(), step, &input, sizeof(InputSet::inputIndex_t), 0, 1, _bits);
  }
  /// @brief Reads the input index recorded for @p step from the bit-packed buffer.
  __INLINE__ InputSet::inputIndex_t getInput(const size_t step) const
  {
    InputSet::inputIndex_t idx = 0;
    jaffarCommon::bitwise::bitcopy(&idx, sizeof(InputSet::inputIndex_t), 0, _buffer.data(), _buffer.size(), step, 1, _bits);
    return idx;
  }

  const uint32_t                  _maxSize;                  ///< Maximum number of steps recorded.
  size_t                          _bits = 0;                 ///< Bits used to encode one input index.
  std::vector<uint8_t>            _buffer;                   ///< Bit-packed input sequence.
  size_t                          _usedBytes        = 0;     ///< Leading bytes of _buffer that may be nonzero (the rest is zero).
  static inline std::atomic<bool> _truncationWarned = false; ///< Set once after the first over-Max-Size push.
};

} // namespace jaffarPlus
