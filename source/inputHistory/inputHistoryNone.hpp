#pragma once

/**
 * @file inputHistoryNone.hpp
 * @brief Input-history strategy that records nothing but the step counter. Smallest possible per-state
 *        footprint; solutions cannot be reconstructed. Selected with `{"Type": "None"}`.
 */

#include "inputHistory.hpp"

namespace jaffarPlus
{

/// @brief Records nothing; smallest per-state footprint (the runner's count alone), no solution reconstruction.
class InputHistoryNone final : public InputHistory
{
public:
  void reset() override {}
  void pushInput(const size_t /*stepCount*/, const InputSet::inputIndex_t /*input*/) override {}

  void serializeCold(jaffarCommon::serializer::Base& /*s*/, const size_t /*stepCount*/ = SIZE_MAX) const override {}
  void deserializeCold(jaffarCommon::deserializer::Base& /*d*/, const size_t /*stepCount*/ = SIZE_MAX) override {}
  void serializeFull(jaffarCommon::serializer::Base& /*s*/) const override {}
  void deserializeFull(jaffarCommon::deserializer::Base& /*d*/, const size_t /*stepCount*/) override {}

  std::string toString(const std::map<InputSet::inputIndex_t, std::string>& /*m*/, const size_t /*stepCount*/) const override { return ""; }

  size_t getColdSize() const override { return 0; }
  size_t getFullSize() const override { return 0; }

  void captureColdToFull(const void* /*cold*/, void* /*full*/, const size_t /*stepCount*/ = SIZE_MAX) const override {}
};

} // namespace jaffarPlus
