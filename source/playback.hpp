#pragma once

/**
 * @file playback.hpp
 * @brief Solution playback used by the player tool: replays an input sequence through a Runner,
 *        caching per-step game/renderer state, hashes, and win/fail markers for navigation.
 */

#include "jaffarCommon/deserializers/contiguous.hpp"
#include "jaffarCommon/hash.hpp"
#include "jaffarCommon/serializers/contiguous.hpp"
#include "runner.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace jaffarPlus
{

/**
 * @brief Replays a solution's input sequence and caches per-step state for navigation.
 *
 * @details Built from a @ref Runner, @ref initialize applies each input in order, recording for every
 * step its input, allowance, serialized game and renderer state, state hash, and any earlier steps
 * sharing that hash, as well as the first win/fail step. Cached steps can then be queried and
 * re-loaded/rendered out of order by the player tool.
 */
class Playback final
{
public:
  /**
   * @brief A single recorded playback step.
   */
  struct step_t
  {
    /// @brief The step's input string.
    std::string inputString;

    /// @brief The step's input index.
    jaffarPlus::InputSet::inputIndex_t inputIndex;

    /// @brief Whether the move is allowed by the current move set.
    bool isInputAllowed;

    /// @brief The step's serialized game state data.
    void* gameStateData;

    /// @brief The step's serialized renderer state data.
    void* rendererStateData;

    /// @brief The step's state hash.
    jaffarCommon::hash::hash_t stateHash;

    /// @brief Earlier steps (ascending) that shared this step's hash.
    std::vector<size_t> _repeatedHashSteps;
  };

  /**
   * @brief Constructs the playback over a runner and caches state sizes.
   * @param runner The runner used to advance and serialize the solution.
   */
  Playback(Runner& runner) : _runner(&runner)
  {
    // Getting game state size
    _gameStateSize = _runner->getStateSize();

    // Getting renderer state size
    _rendererStateSize = _runner->getGame()->getEmulator()->getRendererStateSize();
  };

  /**
   * @brief Replays the input sequence, recording one cached step per input (plus a trailing
   *        end-of-sequence step).
   * @details For each step, registers/looks up the input, computes its allowance and state hash,
   * records earlier steps sharing that hash, serializes the game and renderer state, advances the
   * runner, evaluates rules, updates the game state type and reward, and records the first win/fail
   * step.
   * @param inputSequence The ordered list of input strings to replay.
   * @param storeRendererState When false, the per-step renderer framebuffer (256KB/step) is not
   *        cached. Safe in headless mode (--disableRender), where renderFrame is never called; this
   *        cuts memory from ~256KB/step to just the game state (~11KB/step) for long movies.
   */
  void initialize(const std::vector<std::string>& inputSequence, bool storeRendererState = true)
  {
    _storeRendererState = storeRendererState;
    // For each input in the sequence, store the game's state
    for (size_t i = 0; i <= inputSequence.size(); i++)
    {
      // Creating new step
      step_t step;

      // Checking if this is the end of the sequence
      bool isEndOfSequence = i == inputSequence.size();

      // Setting step input string
      step.inputString = isEndOfSequence == false ? inputSequence[i] : "<End Of Sequence>";

      // Checking if the input is allowed
      bool isRegisteredInput = _runner->isInputRegistered(step.inputString);

      // Getting input index
      if (isEndOfSequence == true) step.inputIndex = 0;
      if (isEndOfSequence == false && isRegisteredInput == true) step.inputIndex = _runner->getInputIndex(step.inputString);
      if (isEndOfSequence == false && isRegisteredInput == false) step.inputIndex = _runner->registerInput(step.inputString);

      // Checking if the input is allowed
      step.isInputAllowed = false;
      if (isRegisteredInput == true)
      {
        auto allowedInputs  = _runner->getAllowedInputs();
        step.isInputAllowed = std::find(allowedInputs.begin(), allowedInputs.end(), step.inputIndex) != allowedInputs.end();
      }

      // Getting state hash
      step.stateHash = _runner->computeHash();

      // Recording duplicate states: any earlier steps already filed under this step's hash are
      // exactly the repeated states the engine would have pruned on encountering them. Looking them
      // up in a hash map is O(1) amortized, versus the previous O(n^2) scan over every prior step,
      // so this scales to long movies. The earlier steps are stored in ascending order, matching the
      // previous behaviour.
      auto& sameHashSteps     = _hashOccurrences[step.stateHash];
      step._repeatedHashSteps = sameHashSteps;
      sameHashSteps.push_back(i);

      // Allocating space for the game state data
      step.gameStateData = malloc(_gameStateSize);

      // Serializing game state
      jaffarCommon::serializer::Contiguous sg(step.gameStateData, _gameStateSize);
      _runner->serializeState(sg);

      // Allocating and serializing renderer state (skipped in headless mode to save ~256KB/step)
      if (_storeRendererState)
      {
        step.rendererStateData = malloc(_rendererStateSize);

        // Updating renderer state
        _runner->getGame()->getEmulator()->updateRendererState(i, step.inputString);

        // Serializing renderer state
        jaffarCommon::serializer::Contiguous sr(step.rendererStateData, _rendererStateSize);
        _runner->getGame()->getEmulator()->serializeRendererState(sr);
      }
      else
        step.rendererStateData = nullptr;

      // Advancing state
      if (i < inputSequence.size()) _runner->advanceState(step.inputIndex);
      if (i == inputSequence.size()) _runner->advanceState(_sequence.rbegin()->inputIndex);

      // Diagnostic (JAFFAR_DUMP_FULLSTATE_DIR=<dir>): write each step's full serialized state to
      // <dir>/state<step>.bin. Diffing two replays' dumps at equal depth pinpoints which emulator
      // fields (beyond RAM) distinguish converging-looking states.
      if (const char* dumpDir = std::getenv("JAFFAR_DUMP_FULLSTATE_DIR"); dumpDir != nullptr)
      {
        std::vector<uint8_t>                 buf(_gameStateSize);
        jaffarCommon::serializer::Contiguous s(buf.data(), buf.size());
        _runner->serializeState(s);
        char path[512];
        snprintf(path, sizeof(path), "%s/state%04lu.bin", dumpDir, i + 1);
        jaffarCommon::file::saveStringToFile(std::string((char*)buf.data(), buf.size()), path);
      }

      // Diagnostic (JAFFAR_ROUNDTRIP_PER_STEP=1): serialize+deserialize the runner after every
      // advance, mimicking the engine's store/reload dynamics. Diffing a --dumpHashes file produced
      // with this flag against a normal one pinpoints where round-trip advancing forks from live.
      if (std::getenv("JAFFAR_ROUNDTRIP_PER_STEP") != nullptr)
      {
        std::vector<uint8_t> rt(_gameStateSize);
        {
          jaffarCommon::serializer::Contiguous s(rt.data(), rt.size());
          _runner->serializeState(s);
        }
        {
          jaffarCommon::deserializer::Contiguous d(rt.data(), rt.size());
          _runner->deserializeState(d);
        }
      }

      // Evaluate game rules
      _runner->getGame()->evaluateRules();

      // Determining new game state type
      _runner->getGame()->updateGameStateType();

      // Recording the first step at which the solution reaches a win/fail state. Only real applied
      // inputs are considered (i < size); the i == size iteration re-applies the last input as a
      // sentinel. The count is "inputs applied" (i + 1), matching the player's step convention.
      if (i < inputSequence.size())
      {
        const auto stateType = _runner->getGame()->getStateType();
        if (stateType == Game::stateType_t::win && _firstWinStep < 0) _firstWinStep = (ssize_t)i + 1;
        if (stateType == Game::stateType_t::fail && _firstFailStep < 0) _firstFailStep = (ssize_t)i + 1;
      }

      // Updating game reward
      _runner->getGame()->updateReward();

      // Adding step to the internal storage
      _sequence.push_back(step);
    }
  }

  /**
   * @brief Frees the game and renderer state memory allocated during initialization.
   */
  ~Playback()
  {
    // Freeing up memory reserved during initialization
    for (const auto& step : _sequence)
    {
      free(step.gameStateData);
      if (step.rendererStateData != nullptr) free(step.rendererStateData);
    }
  }

  /** @brief Returns the input string of the given step. */
  __INLINE__ std::string getStateInputString(const size_t currentStep) const { return getStep(currentStep).inputString; }
  /** @brief Returns the input index of the given step. */
  __INLINE__ jaffarPlus::InputSet::inputIndex_t getStateInputIndex(const size_t currentStep) const { return getStep(currentStep).inputIndex; }
  /** @brief Returns the serialized game state data of the given step. */
  __INLINE__ void* getStateData(const size_t currentStep) const { return getStep(currentStep).gameStateData; }
  /** @brief Returns the earlier steps (ascending) sharing the given step's hash. */
  __INLINE__ const std::vector<size_t> getStateRepeatedHashSteps(const size_t currentStep) const { return getStep(currentStep)._repeatedHashSteps; }
  /** @brief Returns the state hash of the given step. */
  __INLINE__ jaffarCommon::hash::hash_t getStateHash(const size_t currentStep) const { return getStep(currentStep).stateHash; }
  /** @brief Returns whether the input of the given step is allowed by the current move set. */
  __INLINE__ bool isInputAllowed(const size_t currentStep) const { return getStep(currentStep).isInputAllowed; }

  /**
   * @brief Returns the first step (number of inputs applied) at which the solution reaches a win
   *        state, or -1 if it never does.
   * @details Computed once during @ref initialize.
   */
  __INLINE__ ssize_t getFirstWinStep() const { return _firstWinStep; }
  /**
   * @brief Returns the first step (number of inputs applied) at which the solution reaches a fail
   *        state, or -1 if it never does.
   * @details Computed once during @ref initialize.
   */
  __INLINE__ ssize_t getFirstFailStep() const { return _firstFailStep; }

  /**
   * @brief Renders the cached frame for the given step into the emulator window.
   * @param currentStep The step whose renderer state to display.
   */
  __INLINE__ void renderFrame(const size_t currentStep)
  {
    const auto&                            step = getStep(currentStep);
    jaffarCommon::deserializer::Contiguous d(step.rendererStateData, _rendererStateSize);
    _runner->getGame()->getEmulator()->deserializeRendererState(d);
    _runner->getGame()->getEmulator()->showRender();
  }

  /**
   * @brief Loads the cached game state of the given step back into the runner.
   * @param stepId The step whose game state to deserialize.
   */
  void loadStepData(const size_t stepId)
  {
    // Deserializing appropriate state
    jaffarCommon::deserializer::Contiguous d(getStateData(stepId), _gameStateSize);
    _runner->deserializeState(d);
  }

  /**
   * @brief Prints runner, game, and emulator information.
   */
  void printInfo() const
  {
    // Now printing information
    jaffarCommon::logger::log("[J+] Runner Information: \n");
    _runner->printInfo();
    jaffarCommon::logger::log("[J+] Game Information: \n");
    _runner->getGame()->printInfo();
    jaffarCommon::logger::log("[J+] Emulator Information: \n");
    _runner->getGame()->getEmulator()->printInfo();
  }

private:
  /**
   * @brief Returns the cached step with the given id.
   * @param stepId The index of the step to retrieve.
   * @return A copy of the cached step.
   * @throws A runtime error if @p stepId exceeds the recorded sequence size.
   */
  step_t getStep(const size_t stepId) const
  {
    if (stepId >= _sequence.size()) JAFFAR_THROW_RUNTIME("Requested step %lu which exceeds sequence size %lu", stepId, _sequence.size());
    return _sequence.at(stepId);
  }

  /**
   * @brief Hash functor for 128-bit state hashes (std::pair<uint64_t, uint64_t>).
   * @details Lets a state hash key an unordered_map for O(1) duplicate-state lookup.
   */
  struct hashHasher_t
  {
    /**
     * @brief Combines the two 64-bit halves of a state hash into a size_t.
     * @param h The 128-bit state hash to reduce.
     * @return The combined hash value.
     */
    size_t operator()(const jaffarCommon::hash::hash_t& h) const noexcept { return h.first ^ (h.second + 0x9E3779B97F4A7C15ULL + (h.first << 6) + (h.first >> 2)); }
  };

  /// @brief Pointer to the runner used for playback.
  Runner* _runner;

  /// @brief Size, in bytes, of a serialized game state.
  size_t _gameStateSize;

  /// @brief Size, in bytes, of a serialized renderer state.
  size_t _rendererStateSize;

  /// @brief Whether to cache the per-step renderer framebuffer (disabled in headless mode to save memory).
  bool _storeRendererState = true;

  /// @brief The recorded sequence of playback steps.
  std::vector<step_t> _sequence;

  /// @brief Maps each state hash to the steps (ascending) at which it occurred, used to detect the repeated states the engine would have deduplicated.
  std::unordered_map<jaffarCommon::hash::hash_t, std::vector<size_t>, hashHasher_t> _hashOccurrences;

  ssize_t _firstWinStep  = -1; ///< First step (inputs applied) reaching a win state; -1 until/unless one is seen.
  ssize_t _firstFailStep = -1; ///< First step (inputs applied) reaching a fail state; -1 until/unless one is seen.
};

} // namespace jaffarPlus
