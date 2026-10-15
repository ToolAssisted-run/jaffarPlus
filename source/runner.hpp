#pragma once

/**
 * @file runner.hpp
 * @brief Drives a Game forward one input at a time, managing the allowed/candidate input sets,
 *        frameskip, input-history recording and state hashing/serialization.
 */

#include "game.hpp"
#include "inputHistory/inputHistoryFactory.hpp"
#include "inputSet.hpp"
#include <atomic>
#include <cstdint>
#include <gameList.hpp>
#include <jaffarCommon/bitwise.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief Owns a @ref Game instance and advances it according to configured inputs.
 *
 * @details The runner parses allowed and candidate input sets from the configuration, registers
 * every input string to a numeric index, and on each step advances the game by one input (plus any
 * configured frameskip frames). It can record the sequence of applied inputs as a bit-packed input
 * history, serialize/deserialize its state together with the game's, and compute a hash of the
 * current state.
 */
class Runner final
{
public:
  /**
   * @brief Constructs a runner that takes ownership of @p game and reads its settings from @p config.
   * @param game   The game instance to drive; ownership is moved into the runner.
   * @param config Runner configuration object, supplying the keys "Hash Step Tolerance",
   *               "Store Input History" (with "Enabled" and "Max Size"), "Allowed Input Sets",
   *               "Show Allowed Inputs", "Show Empty Input Slots", "Test Candidate Inputs",
   *               "Candidate Input Sets", "Frameskip" (with "Rate", "Use Custom Input",
   *               "Custom Input") and "Bypass Hash Calculation".
   */
  Runner(std::unique_ptr<Game>& game, const nlohmann::json& config) : _game(std::move(game))
  {
    // Mutable copy so unrecognized Runner keys can be flagged after the known ones are consumed
    auto configRemaining = config;

    _hashStepTolerance = jaffarCommon::json::popNumber<uint32_t>(configRemaining, "Hash Step Tolerance");

    // The input-history strategy (None / Raw / Trie) is selected and validated by the InputHistory
    // factory from this object; stored here and built in initialize() once the input set is known.
    _inputHistoryConfig = jaffarCommon::json::popObject(configRemaining, "Store Input History");

    // Storing game inputs for delayed parsing
    _allowedInputSetsJs = jaffarCommon::json::popArray<nlohmann::json>(configRemaining, "Allowed Input Sets");

    // Print option: do not print allowed inputs
    _showAllowedInputs = jaffarCommon::json::popBoolean(configRemaining, "Show Allowed Inputs");

    // Print option: do not print placeholders for inputs not supported in this frame
    _showEmptyInputSlots = jaffarCommon::json::popBoolean(configRemaining, "Show Empty Input Slots");

    // Stores candidate inputs
    _testCandidateInputs = jaffarCommon::json::popBoolean(configRemaining, "Test Candidate Inputs");

    // Getting candidate input sets
    _candidateInputSetsJs = jaffarCommon::json::popArray<nlohmann::json>(configRemaining, "Candidate Input Sets");

    // Getting frame skip configuration
    auto frameskipJs         = jaffarCommon::json::popObject(configRemaining, "Frameskip");
    _frameskipRate           = jaffarCommon::json::popNumber<size_t>(frameskipJs, "Rate");
    _frameskipUseCustomInput = jaffarCommon::json::popBoolean(frameskipJs, "Use Custom Input");
    _frameskipCustomInput    = jaffarCommon::json::popString(frameskipJs, "Custom Input");
    jaffarCommon::json::checkEmpty(frameskipJs, "Runner Configuration > Frameskip");

    // Option to bypass hash calculation via MetroHash and get it straight from the game
    _bypassHashCalculation = jaffarCommon::json::popBoolean(configRemaining, "Bypass Hash Calculation");

    // Any remaining Runner key is unrecognized
    jaffarCommon::json::checkEmpty(configRemaining, "Runner Configuration");
  }

  /**
   * @brief Initializes the runner: initializes the game, parses input sets, and registers inputs.
   *
   * @details Initializes the game if needed, parses the allowed input sets (and candidate input sets
   * when "Test Candidate Inputs" is enabled), registers the inputs declared by the game's code,
   * resolves the custom frameskip input index, and allocates the bit-packed input-history storage
   * when input history is enabled.
   * @throws A logic error if the runner was already initialized.
   */
  void initialize()
  {
    if (_isInitialized == true) JAFFAR_THROW_LOGIC("This runner instance was already initialized");

    // Initializing game, if not already initialized
    if (_game->isInitialized() == false) _game->initialize();

    // Parsing possible game inputs
    for (const auto& inputSetJs : _allowedInputSetsJs) _allowedInputSets.push_back(parseInputSet(inputSetJs));

    // If testing candidate inputs, parse them now
    if (_testCandidateInputs == true)
      for (const auto& inputSetJs : _candidateInputSetsJs) _candidateInputSets.push_back(parseInputSet(inputSetJs));

    // Parsing inputs declared directly by the game's code
    for (const auto& input : _game->getAllPossibleInputs()) registerInput(input);

    // Resolve the custom frameskip input index once (it is constant), so advanceState() can reuse it
    // instead of re-hashing + re-looking-it-up every skipped frame. Also validates it early -- a bad
    // "Custom Input" now fails here at init rather than crashing deep in the simulation loop.
    if (_frameskipUseCustomInput) _frameskipCustomInputIdx = getInputIndex(_frameskipCustomInput);

    // Build the configured input-history strategy now that the input set (hence _maxInputIndex) is known.
    // Worker runners share a backing injected by the engine (setInputHistoryBacking); a standalone runner
    // (player / driver) makes its own private backing here.
    if (_historyBacking == nullptr)
    {
      _historyBacking = inputHistory::createSharedBacking(_inputHistoryConfig, _historyNumShards);
      _historyShardId = 0;
    }
    _inputHistory = inputHistory::create(_inputHistoryConfig, (uint32_t)_maxInputIndex, _historyNumShards, _historyShardId, _historyBacking);
  }

  /// @brief Injects the shared input-history backing (e.g. the engine's one trie) plus this runner's
  /// free-list shard (its worker thread id) and the shard count, so all workers share one structure while
  /// allocating/freeing contention-free. Must be called before @ref initialize.
  __INLINE__ void setInputHistoryBacking(const std::shared_ptr<void>& backing, const uint32_t shardId, const uint32_t numShards)
  {
    _historyBacking   = backing;
    _historyShardId   = shardId;
    _historyNumShards = numShards;
  }

  /// @brief The configured "Store Input History" object (used by the engine to build the shared backing).
  __INLINE__ const nlohmann::json& getInputHistoryConfig() const { return _inputHistoryConfig; }

  /// @brief The input-history strategy in use (for the StateDb's per-slot manager operations).
  __INLINE__ InputHistory* getInputHistory() const { return _inputHistory.get(); }

  /**
   * @brief Builds an @ref InputSet from its JSON description.
   *
   * @details Parses the set's "Conditions" (via the game's condition parser) and "Inputs" (each
   * registered via @ref registerInput), updates @ref _largestInputSetSize, and applies the
   * "Stop Input Evaluation" flag.
   * @param inputSetJs JSON object describing the input set ("Conditions", "Inputs",
   *                   "Stop Input Evaluation").
   * @return The newly created input set.
   */
  std::unique_ptr<InputSet> parseInputSet(const nlohmann::json& inputSetJs)
  {
    // Creating new input set to add
    auto inputSet = std::make_unique<InputSet>();

    // Getting input set condition array
    const auto& conditions = jaffarCommon::json::getArray<nlohmann::json>(inputSetJs, "Conditions");

    // Getting input string array
    const auto& inputsJs = jaffarCommon::json::getArray<std::string>(inputSetJs, "Inputs");

    // Parsing input set conditions
    for (const auto& condition : conditions) inputSet->addCondition(_game->parseCondition(condition));

    // Parsing input set inputs
    for (const auto& input : inputsJs) inputSet->addInput(registerInput(input));

    // Keep track of maximum size (for better output)
    _largestInputSetSize = std::max(_largestInputSetSize, inputSet->getInputIndexes().size());

    // Getting stop evaluating flag
    inputSet->setStopInputEvaluationFlag(jaffarCommon::json::getBoolean(inputSetJs, "Stop Input Evaluation"));

    // Returning new input set
    return inputSet;
  }

  /**
   * @brief Registers an input string and returns its numeric index.
   *
   * @details Obtains the index from the emulator, records the hash->index and index->string mappings,
   * and updates @ref _maxInputIndex. Registering an already-registered input returns its prior index.
   * @param input The input string to register.
   * @return The index assigned to the input.
   */
  InputSet::inputIndex_t registerInput(const std::string& input)
  {
    // Getting input hash
    const auto inputHash = jaffarCommon::hash::hashString(input);

    // Getting index for the new input
    InputSet::inputIndex_t inputIdx = _game->getEmulator()->registerInput(input);

    // Adding new input hash->index to the map
    _inputHashMap[inputHash] = inputIdx;

    // Adding new input index->string to the map
    _inputStringMap[inputIdx] = input;

    // Register maximum input index to determine how many bytes to use for input history storage
    _maxInputIndex = std::max(_maxInputIndex, inputIdx + 1);

    // Returning this input's index (either new or the one registered before)
    return inputIdx;
  }

  /**
   * @brief Collects the input indexes of every input set whose conditions currently evaluate to true.
   *
   * @details Iterates the given input sets in order; for each set that evaluates to true, its input
   * indexes are appended to the result. If a satisfied set has its stop-evaluation flag set,
   * iteration stops after that set.
   * @param inputSets The input sets to evaluate.
   * @return The concatenated input indexes of the satisfied input sets.
   */
  std::vector<InputSet::inputIndex_t> getInputsFromInputSets(const std::vector<std::unique_ptr<InputSet>>& inputSets) const
  {
    // Storage for the possible input set
    std::vector<InputSet::inputIndex_t> possibleInputs;

    // For all registered input sets, see which ones satisfy their conditions and add them
    for (const auto& inputSet : inputSets)
      if (inputSet->evaluate() == true)
      {
        possibleInputs.insert(possibleInputs.end(), inputSet->getInputIndexes().begin(), inputSet->getInputIndexes().end());

        // Getting stop evaluating flag
        bool stopEvaluating = inputSet->getStopInputEvaluationFlag();

        // If stop evaluation is set, then return now
        if (stopEvaluating) break;
      }

    // Return possible inputs
    return possibleInputs;
  }

  /**
   * @brief Returns the inputs currently allowed for the game's state.
   *
   * @details Collects the inputs from the allowed input sets and appends any additional inputs
   * supplied by the game via @ref Game::getAdditionalAllowedInputs.
   * @return The allowed input indexes.
   */
  __INLINE__ const auto getAllowedInputs() const
  {
    // Getting possible inputs based on the configuration file
    auto allowedInputs = getInputsFromInputSets(_allowedInputSets);

    // Getting additional inputs based on the custom game function
    _game->getAdditionalAllowedInputs(allowedInputs);

    return allowedInputs;
  }

  /**
   * @brief Returns the inputs currently allowed by the candidate input sets.
   * @return The candidate input indexes whose input sets evaluate to true.
   */
  __INLINE__ const auto getCandidateInputs() const { return getInputsFromInputSets(_candidateInputSets); }

  /**
   * @brief Looks up the index registered for an input string.
   * @param input The input string to look up.
   * @return The index registered for the input.
   * @throws A logic error if the input was not previously registered.
   */
  __INLINE__ jaffarPlus::InputSet::inputIndex_t getInputIndex(const std::string& input) const
  {
    // Getting input hash
    const auto inputHash = jaffarCommon::hash::hashString(input);

    // Getting input index from the hash map
    auto it = _inputHashMap.find(inputHash);
    if (it == _inputHashMap.end()) JAFFAR_THROW_LOGIC("[ERROR] Input '%s' provided but has not been registered as allowed input first.\n", input.c_str());
    const auto inputIndex = it->second;

    return inputIndex;
  }

  /**
   * @brief Resolves the input that the engine's "Hash Lookahead" advances with before hashing.
   * @param inputString The configured "Hash Lookahead Input", or empty to use the game's null input.
   * @return The input index.
   * @throws A logic error if the input is not registered, or if none is configured and the game defines
   *         no null input (Game::getNullInputIndex returns Game::noNullInput): pressing an arbitrary
   *         input instead would merge states that differ (input 0 is usually a real move).
   */
  __INLINE__ InputSet::inputIndex_t getHashLookaheadInputIndex(const std::string& inputString) const
  {
    if (inputString.empty() == false) return getInputIndex(inputString);
    const auto nullIdx = _game->getNullInputIndex();
    if (nullIdx == Game::noNullInput)
      JAFFAR_THROW_LOGIC("[ERROR] 'Hash Lookahead' needs the input to advance with: set 'Hash Lookahead Input' (Engine Configuration) to the game's no-op input\n");
    return nullIdx;
  }

  /**
   * @brief Reports whether an input string has been registered.
   * @param inputString The input string to test.
   * @return true if the input is registered, false otherwise.
   */
  bool isInputRegistered(const std::string& inputString)
  {
    // Getting input hash
    const auto inputHash = jaffarCommon::hash::hashString(inputString);

    // Returning true if found; false, otherwise
    return _inputHashMap.contains(inputHash);
  }

  /**
   * @brief Advances the game by one input, then by the configured number of frameskip frames.
   *
   * @details Applies @p inputIdx to the game and records it in the input history. Then, for each of
   * the @ref _frameskipRate skipped frames, advances the game with either the custom frameskip input
   * (when "Use Custom Input" is set) or the same @p inputIdx, recording each in the input history.
   * @param inputIdx The index of the input to apply for this step.
   */
  __INLINE__ void advanceState(const InputSet::inputIndex_t inputIdx)
  {
    // Safety check -- Disabled, for performance
    // if (_inputStringMap.contains(inputIdx) == false) JAFFAR_THROW_RUNTIME("Move Index %u not found in runner\n", inputIdx);

    // Performing the requested input
    _game->advanceState(inputIdx);

    // Pushing normal input into the history
    pushInput(inputIdx);

    // If frameskip was set, execute it now
    for (size_t i = 0; i < _frameskipRate; i++)
    {
      InputSet::inputIndex_t frameskipInputIdx = inputIdx;
      if (_frameskipUseCustomInput) frameskipInputIdx = _frameskipCustomInputIdx;
      _game->advanceState(frameskipInputIdx);
      pushInput(frameskipInputIdx);
    }
  }

  /**
   * @brief Records an applied input into the input history at the current step, and advances the step counter.
   *
   * @details The runner owns the step counter (it is what feeds Hash Step Tolerance and is NOT serialized into
   * every state -- see @ref setSearchStep). The configured strategy stores the input at position @ref _stepCount
   * (subject to its own capacity rules); the runner then advances the counter.
   * @param inputIdx The index of the input that was applied.
   */
  __INLINE__ void pushInput(const InputSet::inputIndex_t inputIdx)
  {
    _inputHistory->pushInput(_stepCount, inputIdx);
    _stepCount++;
  }

  /// @brief Sets the step counter directly (its input-count value). Used when restoring a snapshot whose depth
  /// was recorded at capture (best/worst/win/manual), and by the player as it replays.
  __INLINE__ void setStepCount(const size_t stepCount) { _stepCount = stepCount; }

  /// @brief Sets the step counter from a search step. Since every state at search step N shares the same depth,
  /// the engine/driver supply N here instead of serializing the counter per state. Accounts for frameskip
  /// (each search step records 1 + @ref _frameskipRate inputs).
  __INLINE__ void setSearchStep(const size_t searchStep) { _stepCount = searchStep * (_frameskipRate + 1); }

  /// @brief Returns the current step counter (number of inputs applied / the state's depth).
  __INLINE__ size_t getStepCount() const { return _stepCount; }

  /**
   * @brief Serializes the runner state: the game state, the input history, and the input counter.
   * @param serializer The serializer to write the state into.
   */
  __INLINE__ void serializeState(jaffarCommon::serializer::Base& serializer) const
  {
    // Game state, then the self-contained ("full") input-history form (used by in-memory, run-scoped
    // snapshots: the player's initial state and the best/worst/win buffers).
    _game->serializeState(serializer);
    _inputHistory->serializeFull(serializer);
  }

  /**
   * @brief Restores the runner state: the game state, the input history, and the input counter.
   * @param deserializer The deserializer to read the state from.
   */
  __INLINE__ void deserializeState(jaffarCommon::deserializer::Base& deserializer)
  {
    _game->deserializeState(deserializer);
    // The step counter is not part of the stream; the caller sets it (via setStepCount/setSearchStep) before
    // deserializing so strategies that rebuild from the path length (the trie) have it available.
    _inputHistory->deserializeFull(deserializer, _stepCount);
  }

  /**
   * @brief Computes the size in bytes of the serialized runner state.
   * @return The contiguous serialized state size in bytes.
   */
  __INLINE__ size_t getStateSize() const
  {
    jaffarCommon::serializer::Contiguous s;
    this->serializeState(s);
    return s.getOutputSize();
  }

  // ---- Hot/cold split ------------------------------------------------------------------------------
  // The State DB stores the per-state "path" (the bit-packed input history + step counter) in a
  // parallel "cold" slab instead of the hot state slot it hashes/advances every step. These helpers let
  // it write/measure the two parts separately. serializeState()/getStateSize() above are unchanged and
  // still produce the FULL state for every other caller (player, playback, on-disk checkpoints).

  /// @brief Serializes only the hot game+emulator state (what the search reads every step) into @p serializer.
  __INLINE__ void serializeHotState(jaffarCommon::serializer::Base& serializer) const { _game->serializeState(serializer); }
  /// @brief Restores only the hot game+emulator state from @p deserializer.
  __INLINE__ void deserializeHotState(jaffarCommon::deserializer::Base& deserializer) { _game->deserializeState(deserializer); }

  /// @brief Serializes only the cold input-history "path" (written once at state creation, read at
  /// solution time) into @p serializer.
  __INLINE__ void serializeHistory(jaffarCommon::serializer::Base& serializer) const { _inputHistory->serializeCold(serializer); }
  /// @brief Restores only the cold input-history "path" from @p deserializer.
  __INLINE__ void deserializeHistory(jaffarCommon::deserializer::Base& deserializer) { _inputHistory->deserializeCold(deserializer); }

  /// @brief Returns the serialized size of the hot game+emulator state, in bytes.
  __INLINE__ size_t getHotStateSize() const
  {
    jaffarCommon::serializer::Contiguous s;
    this->serializeHotState(s);
    return s.getOutputSize();
  }
  /// @brief Returns the serialized size of the cold input-history "path", in bytes.
  __INLINE__ size_t getHistorySize() const { return _inputHistory->getColdSize(); }

  /**
   * @brief Computes a hash of the current runner state.
   *
   * @details When "Bypass Hash Calculation" is enabled, returns the game's direct state hash.
   * Otherwise, hashes the current hash-step-tolerance stage together with the game's hash
   * contribution using MetroHash128.
   * @return The computed state hash.
   */
  __INLINE__ jaffarCommon::hash::hash_t computeHash() const
  {
    // If normal hash calculation is to be bypassed, get it straight from the game
    if (_bypassHashCalculation == true) return _game->getDirectStateHash();

    // Storage for hash calculation
    MetroHash128 hashEngine;

    // Calculating hash tolerance stage
    auto hashStepToleranceStage = getHashStepToleranceStage();

    // Hashing step tolerance stage
    hashEngine.Update(hashStepToleranceStage);

    // Processing hashing from the game proper
    _game->computeHash(hashEngine);

    jaffarCommon::hash::hash_t result;
    hashEngine.Finalize(reinterpret_cast<uint8_t*>(&result));
    return result;
  }

  /**
   * @brief Builds a newline-separated string of the recorded input history.
   *
   * @details Returns an empty string when input history is disabled. Otherwise concatenates the
   * input string for each recorded step up to the current input count or the history capacity.
   * @return The recorded inputs, one per line.
   * @throws A runtime error if a recorded input index has no registered string.
   */
  std::string getInputHistoryString() const { return _inputHistory->toString(_inputStringMap, _stepCount); }

  /**
   * @brief Returns the input string registered for an input index.
   * @param input The input index to resolve.
   * @return The registered input string for @p input.
   */
  std::string getInputStringFromIndex(const InputSet::inputIndex_t input) { return _inputStringMap[input]; }

  /**
   * @brief Logs runner state information.
   *
   * @details Logs input-history configuration, the current input count, the state hash, the
   * hash-step-tolerance stage and the frameskip rate. When "Show Allowed Inputs" is set, also logs
   * the currently allowed inputs (with empty-slot placeholders when "Show Empty Input Slots" is set).
   */
  void printInfo() const
  {
    // Getting state hash
    const auto hash = jaffarCommon::hash::hashToString(computeHash());

    // Calculating hash tolerance stage
    auto hashStepToleranceStage = getHashStepToleranceStage();

    // Memory usage
    jaffarCommon::logger::log("[J+]  + Input History Type:                       %s (cold %lu B, full %lu B)\n", inputHistory::getType(_inputHistoryConfig).c_str(),
                              _inputHistory->getColdSize(), _inputHistory->getFullSize());

    // Printing runner state
    jaffarCommon::logger::log("[J+]  + Current Input Count:                      %lu\n", _stepCount);
    jaffarCommon::logger::log("[J+]  + Hash:                                     %s\n", hash.c_str());
    jaffarCommon::logger::log("[J+]  + Hash Step Tolerance Stage:                %u / %u\n", hashStepToleranceStage, _hashStepTolerance);

    // Printing frameskip information
    if (_frameskipRate > 0) jaffarCommon::logger::log("[J+]  + Frameskip Rate:                           %lu\n", _frameskipRate);

    // Check whether we want to print inputs
    if (_showAllowedInputs == true)
    {
      // Getting allowed inputs
      const auto& possibleInputs = getAllowedInputs();

      // Printing them
      jaffarCommon::logger::log("[J+]  + Allowed Inputs:\n");

      size_t currentInputIdx = 0;
      for (const auto inputIdx : possibleInputs)
      {
        jaffarCommon::logger::log("[J+]    + '%s'\n", _inputStringMap.at(inputIdx).c_str());
        currentInputIdx++;
      }

      if (_showEmptyInputSlots)
        for (; currentInputIdx < _largestInputSetSize; currentInputIdx++) jaffarCommon::logger::log("[J+]    + ----- \n");
    }
  }

  /** @brief Returns the current hash-step-tolerance stage (current input count modulo tolerance + 1, in 64 bits so the
   *         largest tolerance does not wrap to a division by zero). */
  __INLINE__ uint32_t getHashStepToleranceStage() const { return (uint32_t)((uint64_t)_stepCount % ((uint64_t)_hashStepTolerance + 1)); }
  /** @brief Returns a pointer to the owned game instance. */
  __INLINE__ Game* getGame() const { return _game.get(); }

  /**
   * @brief Creates a runner from the emulator, game and runner configurations.
   *
   * @details Builds the game via @ref Game::getGame and wraps it in a new runner.
   * @param emulatorConfig Configuration for the emulator.
   * @param gameConfig     Configuration for the game.
   * @param runnerConfig   Configuration for the runner.
   * @return The newly created runner.
   */
  static std::unique_ptr<Runner> getRunner(const nlohmann::json& emulatorConfig, const nlohmann::json& gameConfig, const nlohmann::json& runnerConfig)
  {
    // Getting Game
    auto game = Game::getGame(emulatorConfig, gameConfig);

    // Creating new runner
    auto r = std::make_unique<Runner>(game, runnerConfig);

    // Returning runner
    return r;
  }

  /** @brief Returns whether the runner has been initialized. */
  __INLINE__ bool isInitialized() const { return _isInitialized; }

private:
  bool _isInitialized = false; ///< Whether the runner has been initialized.

  std::unique_ptr<Game> _game; ///< The owned game instance driven by the runner.

  uint32_t _hashStepTolerance; ///< Hash step tolerance, used to derive the hash-step-tolerance stage.

  // --- Input history (strategy-agnostic) -----------------------------------------------------------
  nlohmann::json                _inputHistoryConfig; ///< The "Store Input History" config object (selects None/Raw/Trie).
  std::unique_ptr<InputHistory> _inputHistory;       ///< The configured strategy instance (built in initialize()).
  size_t _stepCount = 0; ///< Inputs applied so far (the state's depth). Runner-owned, NOT serialized; set per search step by the engine/driver (@ref setSearchStep) or the player,
                         ///< and advanced by @ref pushInput. Feeds Hash Step Tolerance and bounds solution reconstruction.
  std::shared_ptr<void> _historyBacking;       ///< Shared backing (e.g. the one trie), injected by the engine or owned privately.
  uint32_t              _historyShardId   = 0; ///< This runner's contention-free free-list shard (its worker thread id).
  uint32_t              _historyNumShards = 2; ///< Shard count of the backing (engine: threadCount+1; standalone: 2).

  bool _bypassHashCalculation; ///< Whether @ref computeHash returns the game's direct hash instead of hashing via MetroHash128.

  ///////////////////////////////
  // Input processing variables
  //////////////////////////////

  InputSet::inputIndex_t _maxInputIndex = 0; ///< One past the highest registered input index; determines the input-history encoding width.

  std::map<jaffarCommon::hash::hash_t, InputSet::inputIndex_t> _inputHashMap; ///< Maps an input string's hash to its index.

  std::map<InputSet::inputIndex_t, std::string> _inputStringMap; ///< Maps an input index back to its input string.

  ///////////////////////////////
  //  Frameskip. These are frames that are simply skipped over, but they inputs registered nevertheless
  //////////////////////////////

  size_t _frameskipRate; ///< Number of frames to skip after each applied input.

  bool _frameskipUseCustomInput; ///< Whether skipped frames use the custom input; false repeats the applied input.

  std::string _frameskipCustomInput; ///< The custom input string to apply on skipped frames.

  /// @brief Resolved index of the custom frameskip input, computed once at initialize() so advanceState()
  /// does not re-hash + re-look-up the input string on every skipped frame.
  InputSet::inputIndex_t _frameskipCustomInputIdx{};

  ///////////////////////////////////////////
  // Allowed and candidate input sets
  //////////////////////////////////////////

  std::vector<nlohmann::json> _allowedInputSetsJs; ///< JSON descriptions of the allowed input sets (parsed at initialization).

  std::vector<nlohmann::json> _candidateInputSetsJs; ///< JSON descriptions of the candidate input sets (parsed at initialization).

  std::vector<std::unique_ptr<InputSet>> _candidateInputSets; ///< Parsed candidate input sets.

  std::vector<std::unique_ptr<InputSet>> _allowedInputSets; ///< Parsed allowed input sets.

  size_t _largestInputSetSize = 0; ///< Largest input set size detected (used for output formatting).

  bool _testCandidateInputs; ///< Whether candidate input sets are parsed and tested.

  bool _showEmptyInputSlots; ///< Whether placeholders are printed for unused input slots.

  bool _showAllowedInputs; ///< Whether allowed inputs are printed in @ref printInfo.
};

} // namespace jaffarPlus