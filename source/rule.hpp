#pragma once

/**
 * @file rule.hpp
 * @brief A rule evaluated by the engine: a labelled set of conditions that, when all satisfied, grants
 *        a reward, runs actions, may satisfy other rules, and may flag win/fail/checkpoint/save-solution.
 */

#include "condition.hpp"
#include "property.hpp"
#include <functional>
#include <jaffarCommon/json.hpp>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief A labelled collection of conditions with associated reward, actions and outcome flags.
 *
 * A rule is satisfied when all of its conditions evaluate to true (logical AND). On top of that it
 * carries an identifying label and index, a numeric reward, a list of game-specific actions, and a
 * set of other rules (by label or pointer) that are considered satisfied alongside it. Boolean flags
 * mark the rule as a win, fail, checkpoint or save-solution rule.
 */
class Rule final
{
public:
  /// @brief Type used to identify a rule by label.
  typedef size_t label_t;

  /**
   * @brief Constructs a rule with its sequential index and identifying label.
   * @param index Internal index used for sequential access.
   * @param label Identifying label for the rule.
   */
  Rule(const size_t index, const label_t label) : _index(index), _label(label) {};
  ~Rule() = default;

  /**
   * @brief Evaluates the rule by checking all of its conditions.
   * @return true if every condition evaluates to true, false if any fails.
   */
  // The rule is achieved only if all conditions are met
  __INLINE__ bool evaluate() const
  {
    for (const auto& c : _conditions)
      if (c->evaluate() == false) return false;
    return true;
  }

  /** @brief Sets the reward granted when this rule is satisfied. @param reward The reward value. */
  void setReward(const float reward) { _reward = reward; }
  /** @brief Sets whether this rule is a win rule. @param isWinRule The win-rule flag. */
  void setWinRule(const bool isWinRule) { _isWinRule = isWinRule; }
  /** @brief Sets whether this rule is a fail rule. @param isFailRule The fail-rule flag. */
  void setFailRule(const bool isFailRule) { _isFailRule = isFailRule; }
  /** @brief Sets whether this rule is a checkpoint rule. @param isCheckpointRule The checkpoint-rule flag. */
  void setCheckpointRule(const bool isCheckpointRule) { _isCheckpointRule = isCheckpointRule; }
  /** @brief Sets the checkpoint tolerance for this rule. @param checkPointTolerance The tolerance value. */
  void setCheckpointTolerance(const size_t checkPointTolerance) { _checkPointTolerance = checkPointTolerance; }
  /** @brief Sets whether this rule triggers saving the solution. @param isSaveSolutionRule The save-solution flag. */
  void setSaveSolutionRule(const bool isSaveSolutionRule) { _isSaveSolutionRule = isSaveSolutionRule; }
  /** @brief Sets the path where the solution is saved. @param saveSolutionPath The file path. */
  void setSaveSolutionPath(const std::string& saveSolutionPath) { _saveSolutionPath = saveSolutionPath; }
  /** @brief Appends a game-specific action to run for this rule. @param function The action to add. */
  void addAction(const std::function<void()>& function) { _actions.push_back(function); }
  /** @brief Adds a condition that must hold for this rule to be satisfied. @param condition The condition to add. */
  void addCondition(std::unique_ptr<Condition> condition) { _conditions.push_back(std::move(condition)); }
  /** @brief Adds the label of another rule considered satisfied alongside this one. @param satisfyRuleLabel The rule label. */
  void addSatisfyRuleLabel(const label_t satisfyRuleLabel) { _satisfyRuleLabels.insert(satisfyRuleLabel); }
  /** @brief Adds a pointer to another rule considered satisfied alongside this one. @param subRule The rule to add. */
  void addSatisfyRule(Rule* const subRule) { _satisfyRules.push_back(subRule); }

  /** @brief Returns the rule's identifying label. */
  label_t getLabel() const { return _label; }
  /** @brief Returns the reward granted when this rule is satisfied. */
  float getReward() const { return _reward; }
  /** @brief Returns whether this rule is a win rule. */
  bool isWinRule() const { return _isWinRule; }
  /** @brief Returns whether this rule is a fail rule. */
  bool isFailRule() const { return _isFailRule; }
  /** @brief Returns whether this rule is a checkpoint rule. */
  bool isCheckpointRule() const { return _isCheckpointRule; }
  /** @brief Returns the rule's checkpoint tolerance. */
  size_t getCheckpointTolerance() const { return _checkPointTolerance; }
  /** @brief Returns whether this rule triggers saving the solution. */
  bool isSaveSolutionRule() const { return _isSaveSolutionRule; }
  /** @brief Returns the path where the solution is saved. */
  const std::string& getSaveSolutionPath() const { return _saveSolutionPath; }
  /** @brief Returns the labels of rules satisfied alongside this one. */
  const std::unordered_set<label_t>& getSatisfyRuleLabels() const { return _satisfyRuleLabels; }
  /** @brief Returns pointers to the rules satisfied alongside this one. */
  const std::vector<Rule*>& getSatisfyRules() const { return _satisfyRules; }
  /** @brief Returns the rule's internal sequential index. */
  size_t getIndex() const { return _index; }
  /** @brief Returns the rule's game-specific actions. */
  const std::vector<std::function<void()>>& getActions() const { return _actions; }

private:
  /// @brief Conditions that must all hold for the rule to be satisfied.
  // Conditions are evaluated frequently, so this optimized for performance
  // Operands are pre-parsed as pointers/immediates and the evaluation function
  // is a template that is created at compilation time. Evaluated in config order.
  std::vector<std::unique_ptr<Condition>> _conditions;

  /// @brief Internal index for sequential access.
  const size_t _index;

  /// @brief Identifying label for the rule.
  const label_t _label;

  /// @brief Reward associated with meeting this rule.
  float _reward = 0.0;

  // Special condition flags
  bool        _isWinRule           = false; ///< Whether this rule marks a win.
  bool        _isFailRule          = false; ///< Whether this rule marks a fail.
  bool        _isCheckpointRule    = false; ///< Whether this rule marks a checkpoint.
  size_t      _checkPointTolerance = 0;     ///< Checkpoint tolerance value.
  bool        _isSaveSolutionRule  = false; ///< Whether this rule triggers saving the solution.
  std::string _saveSolutionPath;            ///< Path where the solution is saved.

  /// @brief Labels of rules also satisfied when this one is.
  std::unordered_set<label_t> _satisfyRuleLabels;

  /// @brief Pointers to rules also satisfied when this one is.
  std::vector<Rule*> _satisfyRules;

  /// @brief Storage for game-specific actions run for this rule.
  std::vector<std::function<void()>> _actions;
};

} // namespace jaffarPlus
