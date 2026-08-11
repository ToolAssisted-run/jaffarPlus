#pragma once

/**
 * @file game.hpp
 * @brief Abstract base for a JaffarPlus game: wraps an emulator, registers game properties, drives
 *        state advancement/serialization, and parses and evaluates the rule/condition/action set.
 */

#include "emulator.hpp"
#include "rule.hpp"
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief Abstract base class for a JaffarPlus game.
 *
 * @details A game wraps an @ref Emulator and exposes the search-facing interface: it advances and
 * (de)serializes state, computes state hashes, evaluates a configured rule set, and tracks the
 * derived state type, reward, checkpoint level and save-solution information. Concrete games
 * subclass it and implement the pure-virtual hooks (e.g. @ref registerGameProperties,
 * @ref advanceStateImpl, @ref serializeStateImpl, @ref deserializeStateImpl,
 * @ref calculateGameSpecificReward, @ref computeAdditionalHashing, @ref printInfoImpl and
 * @ref parseRuleActionImpl).
 */
class Game
{
public:
  /// @brief Classification of the current game state, derived from the satisfied rules.
  enum stateType_t
  {
    normal = 0, ///< No win or fail rule is currently satisfied.
    win    = 1, ///< A win rule is currently satisfied.
    fail   = 2  ///< A fail rule is currently satisfied.
  };

  /**
   * @brief Constructs a game from an already created emulator and a configuration object.
   * @param emulator The emulator instance this game takes ownership of.
   * @param config   Game configuration JSON, providing "Game Name", "Frame Rate",
   *                 "Bypass Emulator State", "Print Properties", "Hash Properties" and "Rules".
   */
  Game(std::unique_ptr<Emulator> emulator, const nlohmann::json& config) : _emulator(std::move(emulator)), _gameConfigRemaining(config)
  {
    // Getting emulator name (for runtime use)
    _gameName = jaffarCommon::json::popString(_gameConfigRemaining, "Game Name");

    // Parsing frame rate
    _frameRate = jaffarCommon::json::popNumber<float>(_gameConfigRemaining, "Frame Rate");

    // Parsing whether to bypass emulator state load/saving
    _bypassEmulatorState = jaffarCommon::json::popBoolean(_gameConfigRemaining, "Bypass Emulator State");

    // Marking printable properties
    const auto& printProperties = jaffarCommon::json::popArray<std::string>(_gameConfigRemaining, "Print Properties");
    for (const auto& property : printProperties) _printablePropertyNames.push_back(property);

    // Parsing hashable game properties
    const auto& hashProperties = jaffarCommon::json::popArray<std::string>(_gameConfigRemaining, "Hash Properties");
    for (const auto& property : hashProperties) _hashablePropertyNames.push_back(property);

    // Storing rules JSON for later parsing. Consumed as a whole here; the rule-array element keys
    // (Conditions/Actions/Satisfies/...) are still parsed leniently by parseRules() below.
    _rulesJs = jaffarCommon::json::popArray<nlohmann::json>(_gameConfigRemaining, "Rules");
  };

  /**
   * @brief Finds a registered game property by name.
   * @param propertyName The property's registered name.
   * @return Pointer to the property, or nullptr if not registered.
   */
  __INLINE__ Property* findProperty(const std::string& propertyName)
  {
    const auto h = jaffarCommon::hash::hashString(propertyName);
    if (_propertyMap.contains(h) == false) return nullptr;
    return _propertyMap[h].get();
  }

  /**
   * @brief Returns a comma-separated list of the property names registered for this game.
   * @details Intended for use in error messages when a configured property name is not found.
   * @return The registered property names joined by ", ".
   */
  std::string getRegisteredPropertyNames() const
  {
    std::string names;
    for (const auto& entry : _propertyMap) names += (names.empty() ? "" : ", ") + entry.second->getName();
    return names;
  }

  /**
   * @brief Initializes the game: emulator, properties, rules and the first state update.
   *
   * @details Initializes the emulator if needed, calls @ref registerGameProperties, resolves the
   * configured printable and hashable property names to property pointers, parses the rules, runs
   * the pre-/post-update hooks around @ref initializeImpl, and marks the game as initialized.
   * @throws A logic error if the game was already initialized, or if a configured printable or
   *         hashable property name is not registered.
   */
  void initialize()
  {
    if (_isInitialized == true) JAFFAR_THROW_LOGIC("This game instance was already initialized");

    // Initializing emulator, if not already initialized
    if (_emulator->isInitialized() == false) _emulator->initialize();

    // Getting game-specific properties
    registerGameProperties();

    // Registering printable properties
    for (const auto& property : _printablePropertyNames)
    {
      // Getting property name hash
      const auto propertyHash = jaffarCommon::hash::hashString(property);

      // Checking the property is registered
      if (_propertyMap.contains(propertyHash) == false)
        JAFFAR_THROW_LOGIC("Property '%s' is not registered in this game. Registered properties: %s\n", property.c_str(), getRegisteredPropertyNames().c_str());

      // If so, add its pointer to the print property vector
      _propertyPrintVector.push_back(_propertyMap.at(propertyHash).get());
    }

    // Registering hashable properties
    for (const auto& property : _hashablePropertyNames)
    {
      // Getting property name hash
      const auto propertyHash = jaffarCommon::hash::hashString(property);

      // Checking the property is registered
      if (_propertyMap.contains(propertyHash) == false)
        JAFFAR_THROW_LOGIC("Property '%s' is not registered in this game. Registered properties: %s\n", property.c_str(), getRegisteredPropertyNames().c_str());

      // If so, add its pointer to the print property vector
      _propertyHashVector.push_back(_propertyMap.at(propertyHash).get());
    }

    // Now parsing rules
    parseRules(_rulesJs);

    // Update internals pre initialization (first state update)
    stateUpdatePreHook();

    // Calling game-specific initializer
    initializeImpl();

    // Update internals post initialization
    stateUpdatePostHook();

    // Set this as initialized
    _isInitialized = true;
  }

  Game()          = delete; ///< Default construction is disabled; a game requires an emulator and config.
  virtual ~Game() = default;

  /**
   * @brief Advances the game state by applying a single input.
   *
   * @details Runs the pre-update hook, snapshots the previous save-solution rule id, calls
   * @ref advanceStateImpl with the input, then runs the post-update hook.
   * @param input The input index to apply for this step.
   */
  __INLINE__ void advanceState(const InputSet::inputIndex_t input)
  {
    // Calling the pre-update hook
    stateUpdatePreHook();

    // Update save solution last rule id
    _saveSolutionCurrentLastRuleIdx = _saveSolutionCurrentLastRuleId;

    // Performing the requested input
    advanceStateImpl(input);

    // Calling the post-update hook
    stateUpdatePostHook();
  }

  /**
   * @brief Serializes the full game state.
   *
   * @details Serializes the emulator state (unless emulator state is bypassed), then game-specific
   * data via @ref serializeStateImpl, followed by the reward, checkpoint level, save-solution rule
   * ids, state type and rule status vector.
   * @param serializer The serializer to push state into.
   */
  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const
  {
    // Serializing internal emulator state
    if (_bypassEmulatorState == false) _emulator->serializeState(serializer);

    // Storage for game-specific data
    serializeStateImpl(serializer);

    // Serializing reward
    serializer.push(&_reward, sizeof(_reward));

    // Serializing checkpoint level
    serializer.push(&_checkpointLevel, sizeof(_checkpointLevel));

    // Serializing the previous last rule id that activated a save solution
    serializer.push(&_saveSolutionCurrentLastRuleIdx, sizeof(_saveSolutionCurrentLastRuleIdx));

    // Serializing the current last rule id that activated a save solution
    serializer.push(&_saveSolutionCurrentLastRuleId, sizeof(_saveSolutionCurrentLastRuleId));

    // Serializing state type
    serializer.push(&_stateType, sizeof(_stateType));

    // Serializing rule states
    serializer.push(_rulesStatus.data(), _rulesStatus.size());
  }

  /**
   * @brief Restores the full game state previously written by @ref serializeState.
   *
   * @details Runs the pre-update hook, restores the emulator state (unless bypassed) and
   * game-specific data via @ref deserializeStateImpl, runs the post-update hook, then pops the
   * reward, checkpoint level, save-solution rule ids and state type. Finally runs the pre-rule
   * hook, restores the rule status vector, re-runs game-specific rule actions and runs the post-rule
   * hook.
   * @param deserializer The deserializer to pop state from.
   */
  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer)
  {
    // Calling the pre-update hook
    stateUpdatePreHook();

    // Storage for the internal emulator state
    if (_bypassEmulatorState == false) _emulator->deserializeState(deserializer);

    // Storage for game-specific data
    deserializeStateImpl(deserializer);

    // Calling the post-update hook
    stateUpdatePostHook();

    // Deserializing reward
    deserializer.pop(&_reward, sizeof(_reward));

    // Deserializing checkpoint level
    deserializer.pop(&_checkpointLevel, sizeof(_checkpointLevel));

    // Deserializing the previous last rule id that activated a save solution
    deserializer.pop(&_saveSolutionCurrentLastRuleIdx, sizeof(_saveSolutionCurrentLastRuleIdx));

    // Deserializing the last rule id that activated a save solution
    deserializer.pop(&_saveSolutionCurrentLastRuleId, sizeof(_saveSolutionCurrentLastRuleId));

    // Deserializing state type
    deserializer.pop(&_stateType, sizeof(_stateType));

    // Calling the pre-rule update hook
    ruleUpdatePreHook();

    // Deserializing rules status
    deserializer.pop(_rulesStatus.data(), _rulesStatus.size());

    // Running game specific rule actions
    runGameSpecificRuleActions();

    // Calling the post-rule update hook
    ruleUpdatePostHook();
  }

  /**
   * @brief Updates a hash engine with the current state's distinguishing data.
   *
   * @details Feeds each hashable property's bytes into the engine, then calls
   * @ref computeAdditionalHashing for any game-specific contribution.
   * @param hashEngine The hash engine to update.
   */
  __INLINE__ void computeHash(MetroHash128& hashEngine) const
  {
    // Processing hashable game properties
    for (const auto& p : _propertyHashVector) hashEngine.Update(p->getPointer(), p->getSize());

    // Processing any additional game-specific hash
    computeAdditionalHashing(hashEngine);
  }

  /**
   * @brief Prints the current game state to the logger.
   *
   * @details Logs the state type, reward, per-rule satisfaction bits, and each printable property's
   * value (formatted by datatype), then calls @ref printInfoImpl for game-specific output.
   */
  void printInfo() const
  {
    // Getting maximum printable property name, for formatting purposes
    const size_t separatorSize   = 4;
    size_t       maximumNameSize = 0;
    for (const auto& p : _propertyPrintVector) maximumNameSize = std::max(maximumNameSize, p->getName().size());

    // Printing game state
    jaffarCommon::logger::log("[J+]  + Game State Type:                          ");
    if (_stateType == stateType_t::normal) jaffarCommon::logger::log("Normal");
    if (_stateType == stateType_t::win) jaffarCommon::logger::log("Win");
    if (_stateType == stateType_t::fail) jaffarCommon::logger::log("Fail");
    jaffarCommon::logger::log("\n");

    // Printing game state
    jaffarCommon::logger::log("[J+]  + Game State Reward:                        %f\n", _reward);

    // Printing rule status
    jaffarCommon::logger::log("[J+]  + Rule Status:                              ");
    for (size_t i = 0; i < _rules.size(); i++) jaffarCommon::logger::log("%d", jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), i) ? 1 : 0);
    jaffarCommon::logger::log("\n");

    // Printing game properties defined in the script file
    jaffarCommon::logger::log("[J+]  + Game Properties: \n");
    for (const auto& p : _propertyPrintVector)
    {
      // Getting property name
      const auto& name = p->getName();

      // Printing property name first
      jaffarCommon::logger::log("[J+]    + '%s':", name.c_str());

      // Calculating separation spaces for this property
      const auto propertySeparatorSize = separatorSize + maximumNameSize - name.size();

      // Printing separator spaces
      for (size_t i = 0; i < propertySeparatorSize; i++) jaffarCommon::logger::log(" ");

      // Then printing separator spaces
      if (p->getDatatype() == Property::datatype_t::dt_int8) jaffarCommon::logger::log("0x%02X  (%03d)\n", p->getValue<int8_t>(), p->getValue<int8_t>());
      if (p->getDatatype() == Property::datatype_t::dt_int16) jaffarCommon::logger::log("0x%04X  (%05d)\n", p->getValue<int16_t>(), p->getValue<int16_t>());
      if (p->getDatatype() == Property::datatype_t::dt_int32) jaffarCommon::logger::log("0x%08X  (%10d)\n", p->getValue<int32_t>(), p->getValue<int32_t>());
      if (p->getDatatype() == Property::datatype_t::dt_int64) jaffarCommon::logger::log("0x%16lX (%ld)\n", p->getValue<int64_t>(), p->getValue<int64_t>());
      if (p->getDatatype() == Property::datatype_t::dt_uint8) jaffarCommon::logger::log("0x%02X  (%03u)\n", p->getValue<uint8_t>(), p->getValue<uint8_t>());
      if (p->getDatatype() == Property::datatype_t::dt_uint16) jaffarCommon::logger::log("0x%04X  (%05u)\n", p->getValue<uint16_t>(), p->getValue<uint16_t>());
      if (p->getDatatype() == Property::datatype_t::dt_uint32) jaffarCommon::logger::log("0x%08X  (%10u)\n", p->getValue<uint32_t>(), p->getValue<uint32_t>());
      if (p->getDatatype() == Property::datatype_t::dt_uint64) jaffarCommon::logger::log("0x%16lX (%lu)\n", p->getValue<uint64_t>(), p->getValue<uint64_t>());
      if (p->getDatatype() == Property::datatype_t::dt_float32) jaffarCommon::logger::log("%f      (0x%X)\n", p->getValue<float>(), p->getValue<uint32_t>());
      if (p->getDatatype() == Property::datatype_t::dt_float64) jaffarCommon::logger::log("%f      (0x%lX)\n", p->getValue<double>(), p->getValue<uint64_t>());
      if (p->getDatatype() == Property::datatype_t::dt_bool) jaffarCommon::logger::log("%1u\n", p->getValue<bool>());
    }

    // Printing game-specific stuff now
    printInfoImpl();
  }

  /**
   * @brief Evaluates the rule set against the current state.
   *
   * @details Runs the pre-rule hook, then for each not-yet-satisfied rule evaluates its conditions
   * and, if met, marks it satisfied via @ref satisfyRule. Afterwards runs game-specific rule actions
   * and the post-rule hook.
   */
  __INLINE__ void evaluateRules()
  {
    // Calling the pre-update hook
    ruleUpdatePreHook();

    // Second, check which unsatisfied rules have been satisfied now
    for (auto& rule : _rules)
    {
      // Getting rule index
      const auto ruleIdx = rule->getIndex();

      // Evaluate rule only if it's not yet satisfied
      if (jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), ruleIdx) == false)
      {
        // Checking if conditions are met
        bool isSatisfied = rule->evaluate();

        // If it's achieved, update its status and run its actions
        if (isSatisfied) satisfyRule(*rule);
      }
    }

    // Running game-specific rule actions
    runGameSpecificRuleActions();

    // Calling the pre-update hook
    ruleUpdatePostHook();
  }

  /**
   * @brief Runs the registered actions of every currently satisfied rule.
   */
  __INLINE__ void runGameSpecificRuleActions()
  {
    // First, checking if the rules have been satisfied
    for (auto& rule : _rules)
    {
      // Getting rule index
      const auto ruleIdx = rule->getIndex();

      // Run ations only if rule is satisfied
      if (jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), ruleIdx) == true)
        for (const auto& action : rule->getActions()) action();
    }
  }

  /**
   * @brief Recomputes the state type and checkpoint level from the satisfied rules.
   *
   * @details Resets the state type to normal and the checkpoint level to zero, then iterates the
   * satisfied rules: checkpoint rules increment the checkpoint level and record their tolerance,
   * save-solution rules update the current save-solution rule id when their index exceeds the
   * previous one, win rules set the state type to win, and fail rules set it to fail.
   */
  __INLINE__ void updateGameStateType()
  {
    // Clearing game state type before we evaluate satisfied rules
    _stateType = stateType_t::normal;

    // Clearing checkpoint level and tolerance
    _checkpointLevel = 0;

    // Second, we run the specified actions for the satisfied rules in label order
    for (auto& rule : _rules)
    {
      // Getting rule index
      const auto ruleIdx = rule->getIndex();

      // Run actions
      if (jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), ruleIdx) == true)
      {
        // Modify game state, depending on rule type

        // Evaluate checkpoint rule and store tolerance if specified
        if (rule->isCheckpointRule())
        {
          _checkpointLevel++;
          _checkpointTolerance = rule->getCheckpointTolerance();
        }

        // Evaluate save state rule and path if specified -- only if the current rule label is greater than the last rule to activate this
        if (rule->isSaveSolutionRule() && (ssize_t)ruleIdx > _saveSolutionCurrentLastRuleIdx) _saveSolutionCurrentLastRuleId = ruleIdx;

        // Winning in the same rule superseeds checkpoint, and failing superseed everything
        if (rule->isWinRule()) _stateType = stateType_t::win;
        if (rule->isFailRule()) _stateType = stateType_t::fail;
      }
    }
  }

  /**
   * @brief Recomputes the current state's reward from the satisfied rules.
   *
   * @details Resets the reward to zero, sums the reward of every satisfied rule, and adds the
   * game-specific reward from @ref calculateGameSpecificReward.
   */
  __INLINE__ void updateReward()
  {
    // First, we resetting reward to zero
    _reward = 0.0;

    // Second, we get the reward from every satisfied rule
    for (auto& rule : _rules)
    {
      // Getting rule index
      const auto ruleIdx = rule->getIndex();

      // Run actions
      if (jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), ruleIdx) == true)
      {
        // Getting reward from satisfied rule
        const auto ruleReward = rule->getReward();

        // Adding it to the state reward
        _reward += ruleReward;
      }
    }

    // Adding any game-specific rewards
    _reward += calculateGameSpecificReward();
  }

  /**
   * @brief Parses a single rule condition from JSON into a typed @ref Condition.
   *
   * @details Resolves the "Op" operator and the "Property" first operand (which must be registered),
   * then interprets "Value": a number or boolean becomes an immediate second operand, while a string
   * is treated as the name of a second registered property. The condition is instantiated as a
   * @ref _vCondition specialized to the first property's datatype.
   * @param conditionJs The condition JSON object, expected to contain "Op", "Property" and "Value".
   * @return An owning pointer to the constructed condition.
   * @throws A logic error if the operator is unknown, a referenced property is not declared, the
   *         "Value" key is missing, or its format/type is invalid.
   */
  std::unique_ptr<Condition> parseCondition(const nlohmann::json& conditionJs)
  {
    // Parsing operator name
    const auto& opName = jaffarCommon::json::getString(conditionJs, "Op");

    // Getting operator type from its name
    const auto opType = Condition::getOperatorType(opName);

    // Parsing first operand (property name)
    const auto& property1Name = jaffarCommon::json::getString(conditionJs, "Property");

    // Getting property name hash, for indexing
    const auto property1NameHash = jaffarCommon::hash::hashString(property1Name);

    // Making sure the requested property exists in the property map
    if (_propertyMap.contains(property1NameHash) == false) JAFFAR_THROW_LOGIC("[ERROR] Property '%s' has not been declared.\n", property1Name.c_str());

    // Getting property object
    const auto property1 = _propertyMap[property1NameHash].get();

    // Getting property data type
    auto datatype1 = property1->getDatatype();

    // Parsing second operand (number)
    if (conditionJs.contains("Value") == false) JAFFAR_THROW_LOGIC("[ERROR] Rule condition missing 'Value' key.\n");
    if (conditionJs["Value"].is_number() == false && conditionJs["Value"].is_string() == false && conditionJs["Value"].is_boolean() == false)
      JAFFAR_THROW_LOGIC("[ERROR] Wrong format for 'Value' entry in rule condition. It must be a string or number");

    // If value is a number, take it as immediate
    if (conditionJs["Value"].is_number())
    {
      if (datatype1 == Property::datatype_t::dt_uint8) return std::make_unique<_vCondition<uint8_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<uint8_t>());
      if (datatype1 == Property::datatype_t::dt_uint16) return std::make_unique<_vCondition<uint16_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<uint16_t>());
      if (datatype1 == Property::datatype_t::dt_uint32) return std::make_unique<_vCondition<uint32_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<uint32_t>());
      if (datatype1 == Property::datatype_t::dt_uint64) return std::make_unique<_vCondition<uint64_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<uint64_t>());

      if (datatype1 == Property::datatype_t::dt_int8) return std::make_unique<_vCondition<int8_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<int8_t>());
      if (datatype1 == Property::datatype_t::dt_int16) return std::make_unique<_vCondition<int16_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<int16_t>());
      if (datatype1 == Property::datatype_t::dt_int32) return std::make_unique<_vCondition<int32_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<int32_t>());
      if (datatype1 == Property::datatype_t::dt_int64) return std::make_unique<_vCondition<int64_t>>(opType, property1, nullptr, 0, conditionJs["Value"].get<int64_t>());

      if (datatype1 == Property::datatype_t::dt_float32) return std::make_unique<_vCondition<float>>(opType, property1, nullptr, 0, conditionJs["Value"].get<float>());
      if (datatype1 == Property::datatype_t::dt_float64) return std::make_unique<_vCondition<double>>(opType, property1, nullptr, 0, conditionJs["Value"].get<double>());
    }

    // If value is a boolean, take it as immediate
    if (conditionJs["Value"].is_boolean())
    {
      if (datatype1 == Property::datatype_t::dt_bool) return std::make_unique<_vCondition<bool>>(opType, property1, nullptr, 0, conditionJs["Value"].get<bool>());
    }

    // If value is a string, take value as property number 2
    if (conditionJs["Value"].is_string())
    {
      // Parsing second operand (property name)
      const auto& property2Name = jaffarCommon::json::getString(conditionJs, "Value");

      // Getting property name hash, for indexing
      const auto property2NameHash = jaffarCommon::hash::hashString(property2Name);

      // Making sure the requested property exists in the property map
      if (_propertyMap.contains(property2NameHash) == false) JAFFAR_THROW_LOGIC("[ERROR] Property '%s' has not been declared.\n", property2Name.c_str());

      // Getting property object
      const auto property2 = _propertyMap[property2NameHash].get();

      if (datatype1 == Property::datatype_t::dt_uint8) return std::make_unique<_vCondition<uint8_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_uint16) return std::make_unique<_vCondition<uint16_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_uint32) return std::make_unique<_vCondition<uint32_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_uint64) return std::make_unique<_vCondition<uint64_t>>(opType, property1, property2, 0, 0);

      if (datatype1 == Property::datatype_t::dt_int8) return std::make_unique<_vCondition<int8_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_int16) return std::make_unique<_vCondition<int16_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_int32) return std::make_unique<_vCondition<int32_t>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_int64) return std::make_unique<_vCondition<int64_t>>(opType, property1, property2, 0, 0);

      if (datatype1 == Property::datatype_t::dt_float32) return std::make_unique<_vCondition<float>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_float64) return std::make_unique<_vCondition<double>>(opType, property1, property2, 0, 0);
      if (datatype1 == Property::datatype_t::dt_bool) return std::make_unique<_vCondition<bool>>(opType, property1, property2, 0, 0);
    }

    JAFFAR_THROW_LOGIC("[ERROR] Rule contains an invalid 'Value' key.\n", conditionJs["Value"].dump().c_str());
  }

  /** @brief Returns a pointer to the internal emulator. */
  __INLINE__ Emulator* getEmulator() const { return _emulator.get(); }

  /**
   * @brief Factory that constructs the concrete game matching the given configuration.
   * @param emulatorConfig The emulator configuration JSON.
   * @param gameConfig     The game configuration JSON.
   * @return An owning pointer to the constructed game.
   */
  static std::unique_ptr<Game> getGame(const nlohmann::json& emulatorConfig, const nlohmann::json& gameConfig);

  /** @brief Returns the configured frame rate. */
  __INLINE__ float getFrameRate() const { return _frameRate; }

  /** @brief Returns the current state's reward. */
  __INLINE__ float getReward() const { return _reward; }

  /**
   * @brief One line of a per-step trace for `player --dumpTrace` (space-separated coordinates the game wants to
   *        record, e.g. for a trace-magnet reference). Default is empty; games override to emit their coordinates.
   */
  virtual __INLINE__ std::string getTraceLine() const { return ""; }

  /**
   * @brief Reward used for the Reference Reward Floor comparison: the un-biased progress reward, WITHOUT any
   *        eviction-only ranking penalty (e.g. a trace magnet). Defaults to the full reward; a game that
   *        subtracts a ranking-only bias from its reward overrides this to add that bias back, so the floor
   *        measures true progress (position) and never false-cancels a position-correct-but-biased state.
   */
  virtual __INLINE__ float getFloorReward() const { return _reward; }

  /** @brief The input index representing "no buttons pressed" -- used by the engine's Hash
   *         Lookahead (advance-with-null before hashing). Games override with their real null. */
  virtual __INLINE__ InputSet::inputIndex_t getNullInputIndex() const { return 0; }

  /** @brief Returns the current state type (normal, win or fail). */
  __INLINE__ stateType_t getStateType() const { return _stateType; }

  /** @brief Returns the current state's checkpoint level. */
  __INLINE__ size_t getCheckpointLevel() const { return _checkpointLevel; }

  /** @brief Returns the current state's checkpoint tolerance. */
  __INLINE__ size_t getCheckpointTolerance() const { return _checkpointTolerance; }

  /**
   * @brief Indicates whether the current state should trigger a save solution.
   * @return true if the current save-solution rule id is greater than the previous one.
   */
  __INLINE__ bool isSaveSolution() const { return _saveSolutionCurrentLastRuleId > _saveSolutionCurrentLastRuleIdx; }

  /** @brief Returns the previous last rule index that set a save solution. */
  __INLINE__ ssize_t getSaveSolutionPrevLastRuleIdx() const { return _saveSolutionCurrentLastRuleIdx; }

  /** @brief Returns the current last rule index that set a save solution. */
  __INLINE__ ssize_t getSaveSolutionCurrentLastRuleIdx() const { return _saveSolutionCurrentLastRuleId; }

  /**
   * @brief Returns the save path of the rule that activated the current save solution.
   * @return The save-solution path, or an empty string if the state is not a save solution.
   */
  __INLINE__ const std::string getSaveSolutionPath() const { return isSaveSolution() ? _rules[_saveSolutionCurrentLastRuleId]->getSaveSolutionPath() : ""; }

  /** @brief Returns the game name used at runtime. */
  __INLINE__ std::string getName() const { return _gameName; }

  /** @brief Returns whether the game has been initialized. */
  __INLINE__ bool isInitialized() const { return _isInitialized; }

  /**
   * @brief Returns a hash identifying the current state for new-input discovery.
   * @details Base implementation returns a default (empty) hash; games may override it.
   * @return The state input hash.
   */
  virtual jaffarCommon::hash::hash_t getStateInputHash() { return jaffarCommon::hash::hash_t(); };

  /**
   * @brief Lets a game contribute additional allowed inputs based on game-specific decisions.
   * @details Base implementation does nothing; games may override it to append inputs.
   * @param allowedInputSet The set of allowed input indices to extend.
   */
  virtual __INLINE__ void getAdditionalAllowedInputs(std::vector<InputSet::inputIndex_t>& allowedInputSet) {}

  /**
   * @brief Reports all possible inputs the game might require.
   * @details Base implementation returns an empty set; games may override it.
   * @return The set of possible input names.
   */
  virtual __INLINE__ std::set<std::string> getAllPossibleInputs() { return {}; }

  /** @brief Prints the game's player-specific commands, if any. Base implementation does nothing. */
  virtual void playerPrintCommands() const {}
  /**
   * @brief Handles a game-specific player command.
   * @param command The command code to handle.
   * @return true if the command was recognized and handled; base implementation returns false.
   */
  virtual bool playerParseCommand(const int command) { return false; }

  /**
   * @brief Returns the state hash directly, without going through a hashing engine.
   * @details Base implementation returns a default (empty) hash; games may override it.
   * @return The direct state hash.
   */
  virtual jaffarCommon::hash::hash_t getDirectStateHash() const { return jaffarCommon::hash::hash_t(); }

protected:
  /**
   * @brief Asserts that every key in the game configuration has been recognized.
   *
   * @details Call this at the END of a derived core's constructor, after it has consumed all of its
   * own configuration keys (via the jaffarCommon::json::pop* helpers) from @ref _gameConfigRemaining.
   * The base ctor has already popped the common keys (Game Name, Frame Rate, Bypass Emulator State,
   * Print Properties, Hash Properties, Rules), so anything still present is an unrecognized key (a
   * typo or unsupported option) and is reported by name. Cores that do not call this remain lenient
   * (opt-in strict validation).
   */
  void finalizeGameConfig() { jaffarCommon::json::checkEmpty(_gameConfigRemaining, "Game Configuration"); }

  /**
   * @brief Registers a game property so it can be referenced by name in rules and printing/hashing.
   * @param name       The property name (used to compute the indexing hash).
   * @param pointer    Pointer to the underlying memory the property reads/writes.
   * @param dataType   The property's datatype.
   * @param endianness The property's byte endianness.
   * @return The same @p pointer that was passed in (returned as a convenience).
   */
  void* registerGameProperty(const std::string& name, void* const pointer, const Property::datatype_t dataType, const Property::endianness_t endianness)
  {
    // Creating property
    auto property = std::make_unique<Property>(name, pointer, dataType, endianness);

    // Getting property name hash as key
    const auto propertyNameHash = property->getNameHash();

    // Adding property to the map for later reference
    _propertyMap[propertyNameHash] = std::move(property);

    // Return the pointer proper (this is just sugar to make the use of this function more compact)
    return pointer;
  }

  /**
   * @brief Parses the full rule array, builds the rule objects and resolves cross-references.
   *
   * @details Clears any existing rules, then for each JSON entry creates a @ref Rule with its label
   * and parses it via @ref parseRule. Afterwards it resolves each rule's "Satisfies" labels into
   * pointers to the referenced rules, and sizes/clears the rule status bit vector.
   * @param rulesJson The array of rule JSON objects.
   * @throws A logic error if a rule entry is not an object, or if a referenced "Satisfies" label
   *         does not exist.
   */
  void parseRules(const nlohmann::json& rulesJson)
  {
    // Reset the rules container
    _rules.clear();
    _rulesStatus.clear();

    // Evaluate each rule
    for (size_t idx = 0; idx < rulesJson.size(); idx++)
    {
      // Getting specific rule json object
      const auto& ruleJs = rulesJson[idx];

      // Check if rule is a key/value object
      if (ruleJs.is_object() == false) JAFFAR_THROW_LOGIC("Passed rule is not a JSON object. Dump: \n %s", ruleJs.dump(2).c_str());

      // Getting rule label
      auto label = jaffarCommon::json::getNumber<Rule::label_t>(ruleJs, "Label");

      // Creating new rule with the given label
      auto rule = std::make_unique<Rule>(idx, label);

      // Parsing json into a rule class
      parseRule(*rule, ruleJs);

      // Adding new rule to the collection
      _rules.push_back(std::move(rule));
    }

    // Checking all cross references are correct
    for (const auto& rule : _rules)
      for (const auto& label : rule->getSatisfyRuleLabels())
      {
        bool subRuleFound = false;
        for (const auto& subRule : _rules)
          if (subRule->getLabel() == label)
          {
            rule->addSatisfyRule(subRule.get());
            subRuleFound = true;
          }
        if (subRuleFound == false) JAFFAR_THROW_LOGIC("Rule label %u referenced by rule %u in the 'Satisfies' array does not exist.\n", label, rule->getIndex());
      }

    // Create rule status vector
    _rulesStatus.resize(jaffarCommon::bitwise::getByteStorageForBitCount(_rules.size()));

    // Clearing the status vector evaluation
    for (size_t i = 0; i < _rules.size(); i++) jaffarCommon::bitwise::setBitValue(_rulesStatus.data(), i, false);
  }

  /**
   * @brief Parses a single rule's conditions, actions and "Satisfies" labels into a @ref Rule.
   *
   * @details Reads the "Conditions", "Actions" and "Satisfies" arrays: each condition is parsed via
   * @ref parseCondition and added to the rule, each action via @ref parseRuleAction, and each
   * satisfies entry (which must be a number) is added as a satisfy-rule label.
   * @param rule   The rule to populate.
   * @param ruleJs The rule JSON object.
   * @throws A logic error if a "Satisfies" entry is not a number (and from the called parsers).
   */
  void parseRule(Rule& rule, const nlohmann::json& ruleJs)
  {
    // Getting rule condition array
    const auto& conditions = jaffarCommon::json::getArray<nlohmann::json>(ruleJs, "Conditions");

    // Getting rule action array
    const auto& actions = jaffarCommon::json::getArray<nlohmann::json>(ruleJs, "Actions");

    // Parsing satisfies vector
    const auto& satisfiesVectorJs = jaffarCommon::json::getArray<nlohmann::json>(ruleJs, "Satisfies");

    // Parsing rule conditions
    for (const auto& condition : conditions) rule.addCondition(parseCondition(condition));

    // Parsing rule actions
    for (const auto& action : actions) parseRuleAction(rule, action);

    // Parsing satisfies vector
    for (const auto& s : satisfiesVectorJs)
    {
      // Check for correct format
      if (s.is_number() == false) JAFFAR_THROW_LOGIC("Wrong format provided in 'Satisfies' array in rule '%s'\n", ruleJs.dump(2).c_str());

      // Adding the satisfies label
      rule.addSatisfyRuleLabel(s.get<Rule::label_t>());
    }
  }

  /**
   * @brief Parses a single rule action from JSON and applies it to the rule.
   *
   * @details Recognizes the built-in action types "Add Reward", "Trigger Fail", "Trigger Win",
   * "Trigger Checkpoint" (with "Tolerance") and "Trigger Save Solution" (with "Path"). Any other
   * action type is delegated to the game-specific @ref parseRuleActionImpl.
   * @param rule     The rule to apply the action to.
   * @param actionJs The action JSON object, expected to contain "Type".
   * @throws A logic error if the action type is not recognized by either the built-ins or the
   *         game-specific parser.
   */
  void parseRuleAction(Rule& rule, const nlohmann::json& actionJs)
  {
    // Getting action type
    std::string actionType = jaffarCommon::json::getString(actionJs, "Type");

    // Running the action, depending on the type
    bool recognizedActionType = false;

    if (actionType == "Add Reward")
    {
      rule.setReward(jaffarCommon::json::getNumber<float>(actionJs, "Value"));
      recognizedActionType = true;
    }

    // Storing fail state
    if (actionType == "Trigger Fail")
    {
      rule.setFailRule(true);
      recognizedActionType = true;
    }

    // Storing win state
    if (actionType == "Trigger Win")
    {
      rule.setWinRule(true);
      recognizedActionType = true;
    }

    // Storing checkpoint flags
    if (actionType == "Trigger Checkpoint")
    {
      rule.setCheckpointRule(true);
      rule.setCheckpointTolerance(jaffarCommon::json::getNumber<size_t>(actionJs, "Tolerance"));
      recognizedActionType = true;
    }

    // Storing save state flags
    if (actionType == "Trigger Save Solution")
    {
      rule.setSaveSolutionRule(true);
      rule.setSaveSolutionPath(jaffarCommon::json::getString(actionJs, "Path"));
      recognizedActionType = true;
    }

    // If not recognized yet, it must be a game specific action
    if (recognizedActionType == false) recognizedActionType = parseRuleActionImpl(rule, actionType, actionJs);

    // If not recognized at all, then fail
    if (recognizedActionType == false)
      JAFFAR_THROW_LOGIC("[ERROR] Unrecognized action '%s' in rule %lu. Valid actions are: Add Reward, Trigger Fail, Trigger Win, Trigger Checkpoint, Trigger Save Solution (plus "
                         "any game-specific actions)\n",
                         actionType.c_str(), rule.getLabel());
  }

  /**
   * @brief Marks a rule as satisfied, recursively satisfying the rules it satisfies first.
   *
   * @details For each sub-rule in this rule's satisfy list that is not yet satisfied, recurses into
   * @ref satisfyRule, then sets this rule's status bit to satisfied.
   * @param rule The rule to mark as satisfied.
   */
  __INLINE__ void satisfyRule(Rule& rule)
  {
    // Recursively run actions for the yet unsatisfied rules that are satisfied by this one and mark them as satisfied
    for (const auto subRule : rule.getSatisfyRules())
    {
      // Getting index from the subrule
      auto subRuleIdx = subRule->getIndex();

      // Only activate it if it hasn't been activated before
      if (jaffarCommon::bitwise::getBitValue(_rulesStatus.data(), subRuleIdx) == false) satisfyRule(*subRule);
    }

    // Getting rule index
    const auto ruleIdx = rule.getIndex();

    // Setting status to satisfied
    jaffarCommon::bitwise::setBitValue(_rulesStatus.data(), ruleIdx, true);
  }

  /**
   * @brief Game-specific initialization hook, called during @ref initialize.
   * @details Base implementation does nothing; games may override it.
   */
  virtual void initializeImpl() {};

  /**
   * @brief Registers the game's properties (via @ref registerGameProperty). Must be implemented.
   */
  virtual void registerGameProperties() = 0;

  /**
   * @brief Serializes game-specific state. Must be implemented.
   * @param serializer The serializer to push game-specific state into.
   */
  virtual void serializeStateImpl(jaffarCommon::serializer::Base& serializer) const = 0;

  /**
   * @brief Restores game-specific state previously written by @ref serializeStateImpl. Must be implemented.
   * @param deserializer The deserializer to pop game-specific state from.
   */
  virtual void deserializeStateImpl(jaffarCommon::deserializer::Base& deserializer) = 0;

  /**
   * @brief Computes the game-specific contribution to the state reward. Must be implemented.
   * @return The game-specific reward to add to the rule-based reward.
   */
  virtual float calculateGameSpecificReward() const = 0;

  /**
   * @brief Adds game-specific data into the hash engine. Must be implemented.
   * @param hashEngine The hash engine to update with game-specific data.
   */
  virtual void computeAdditionalHashing(MetroHash128& hashEngine) const = 0;

  /**
   * @brief Prints game-specific state information to the logger. Must be implemented.
   */
  virtual void printInfoImpl() const = 0;

  /**
   * @brief Advances the game state by applying the given input. Must be implemented.
   * @param input The input index to apply for this step.
   */
  virtual void advanceStateImpl(const InputSet::inputIndex_t input) = 0;

  /**
   * @brief Parses and applies a game-specific rule action. Must be implemented.
   * @param rule       The rule to apply the action to.
   * @param actionType The action type string that was not matched by the built-in actions.
   * @param actionJs   The action JSON object.
   * @return true if the action type was recognized and applied; false otherwise.
   */
  virtual bool parseRuleActionImpl(Rule& rule, const std::string& actionType, const nlohmann::json& actionJs) = 0;

  /// @brief Optional hook run before a state update (advance/deserialize). Base does nothing.
  virtual __INLINE__ void stateUpdatePreHook() {};
  /// @brief Optional hook run after a state update (advance/deserialize). Base does nothing.
  virtual __INLINE__ void stateUpdatePostHook() {};
  /// @brief Optional hook run before rule evaluation/restoration. Base does nothing.
  virtual __INLINE__ void ruleUpdatePreHook() {};
  /// @brief Optional hook run after rule evaluation/restoration. Base does nothing.
  virtual __INLINE__ void ruleUpdatePostHook() {};

  /// @brief Current game state type. Initialized to normal because it is read (printInfo) and serialized for the initial state before updateGameStateType() first assigns it.
  stateType_t _stateType = stateType_t::normal;

  float _reward = 0.0; ///< Current game state reward.

  size_t _checkpointLevel = 0; ///< Current state's checkpoint level.

  size_t _checkpointTolerance = 0; ///< Tolerance recorded for checkpoint states.

  /// @brief Previous last rule index that activated a save solution (preserved to mark the state where it changes); save state activates only when a rule id exceeds it. -1 when
  /// none.
  ssize_t _saveSolutionCurrentLastRuleIdx = -1;

  /// @brief Current last rule index that activated a save solution; save state activates only when a rule id is bigger than the previous one. -1 when none.
  ssize_t _saveSolutionCurrentLastRuleId = -1;

  const std::unique_ptr<Emulator> _emulator; ///< Underlying emulator instance.

  std::vector<std::unique_ptr<Rule>> _rules; ///< Game script rules, kept in a vector to preserve ordering.

  std::vector<uint8_t> _rulesStatus; ///< Bit vector indicating whether each rule has been satisfied.

  std::vector<std::string> _printablePropertyNames; ///< Parsed property names configured to be printed.

  std::vector<std::string> _hashablePropertyNames; ///< Parsed property names configured to be hashed.

  std::vector<const Property*> _propertyHashVector; ///< Properties used to hash/distinguish states, ordered.

  std::vector<const Property*> _propertyPrintVector; ///< Properties printed for game information, ordered.

  std::map<jaffarCommon::hash::hash_t, std::unique_ptr<Property>> _propertyMap; ///< All registered properties, indexed by name hash.

  float _frameRate; ///< Frame rate to play the game with, required for correct playback.

  bool _bypassEmulatorState; ///< When true, the game handles state save/load entirely, bypassing the emulator.

  std::string _gameName; ///< Game name (for runtime use).

  nlohmann::json _rulesJs; ///< Temporary storage of the rules JSON for delayed parsing.

  /// @brief Mutable working copy of the game config; recognized keys are popped, leftovers are unrecognized. See @ref finalizeGameConfig().
  nlohmann::json _gameConfigRemaining;

  bool _isInitialized = false; ///< Whether the game has been initialized.
};

} // namespace jaffarPlus