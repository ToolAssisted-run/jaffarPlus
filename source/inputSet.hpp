#pragma once

/**
 * @file inputSet.hpp
 * @brief A set of input indexes gated by conditions: the inputs become available only when all of the
 *        set's conditions are satisfied.
 */

#include "condition.hpp"
#include "property.hpp"
#include <jaffarCommon/json.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief A collection of input indexes whose availability is gated by a set of conditions.
 *
 * An input set holds a group of input indexes together with conditions that must all evaluate to
 * true (logical AND) for the set to be active. It also carries a flag that, when set, stops the
 * evaluation of subsequent input sets.
 */
class InputSet final
{
public:
  /// @brief Type used to index an input.
  typedef size_t inputIndex_t;

  InputSet()  = default;
  ~InputSet() = default;

  /**
   * @brief Evaluates the input set by checking all of its conditions.
   * @return true if every condition evaluates to true, false if any fails.
   */
  // The input set is activated only if all conditions are met
  __INLINE__ bool evaluate() const
  {
    for (const auto& c : _conditions)
      if (c->evaluate() == false) return false;
    return true;
  }

  /** @brief Adds an input index to this set. @param inputIdx The input index to add. */
  void addInput(const inputIndex_t inputIdx) { _inputIndexes.insert(inputIdx); }
  /** @brief Adds a condition that must hold for this set to be active. @param condition The condition to add. */
  void addCondition(std::unique_ptr<Condition> condition) { _conditions.push_back(std::move(condition)); }
  /** @brief Returns the input indexes contained in this set. */
  const std::unordered_set<inputIndex_t>& getInputIndexes() const { return _inputIndexes; }
  /** @brief Returns whether this set stops evaluation of subsequent input sets. */
  bool getStopInputEvaluationFlag() const { return _stopInputEvaluation; }
  /** @brief Sets whether this set stops evaluation of subsequent input sets. @param stopInputEvaluation The flag value. */
  void setStopInputEvaluationFlag(const bool stopInputEvaluation) { _stopInputEvaluation = stopInputEvaluation; }

private:
  /// @brief Conditions that must all hold for this input set to be active.
  // Conditions are evaluated frequently, so this optimized for performance
  // Operands are pre-parsed as pointers/immediates and the evaluation function
  // is a template that is created at compilation time. Evaluated in config order.
  std::vector<std::unique_ptr<Condition>> _conditions;

  /// @brief Input indexes belonging to this set.
  // Storage for game-specific actions
  std::unordered_set<inputIndex_t> _inputIndexes;

  /// @brief If true, input sets after this one are not evaluated.
  // If this flag is true, then the other input sets after this will not be evaluated
  bool _stopInputEvaluation = false;
};

} // namespace jaffarPlus
