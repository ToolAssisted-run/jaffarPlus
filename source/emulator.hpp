#pragma once

/**
 * @file emulator.hpp
 * @brief Abstract emulator interface that concrete emulation cores implement, exposing state
 *        load/save, step advancement, named memory properties, and optional rendering/screenshots.
 */

#include <SDL2/SDL.h>
#include <inputParser.hpp>
#include <inputSet.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <string>

namespace jaffarPlus
{

/**
 * @brief A property: a contiguous segment of memory with a size, identifiable by name.
 */
struct property_t
{
  uint8_t* pointer; ///< Pointer to the start of the property's memory segment.
  size_t   size;    ///< Size of the property's memory segment, in bytes.
};

/**
 * @brief Abstract base for an emulation core.
 *
 * @details Defines the interface JaffarPlus uses to drive a guest system: initializing the core,
 * registering and advancing inputs, serializing/deserializing emulator state, exposing guest memory
 * as named @ref property_t segments, and (optionally) rendering frames and saving screenshots.
 * Concrete cores implement the pure-virtual `*Impl` and rendering hooks; @ref getEmulator constructs
 * the appropriate core from configuration.
 */
class Emulator
{
public:
  /**
   * @brief Pairs an input's string form with its decoded, emulator-specific input data.
   */
  struct inputEntry_t
  {
    /// @brief Input string.
    std::string inputString;

    /// @brief Emulator-specific input data.
    jaffar::input_t inputData;
  };

  /**
   * @brief Constructs the emulator, performing only configuration parsing (for dry runs).
   * @param config Emulator configuration object; must contain the "Emulator Name" field.
   *
   * @details Keeps a mutable working copy of @p config in @ref _emulatorConfigRemaining and consumes
   * (pops) the common keys it reads from it, so that a derived core can pop ITS own keys from the
   * same copy and then call @ref finalizeEmulatorConfig() to reject any leftover (unrecognized) key.
   * Strict checking is opt-in: a core that does not call @ref finalizeEmulatorConfig() is never
   * checked, so unconverted cores keep working unchanged.
   */
  Emulator(const nlohmann::json& config) : _emulatorConfigRemaining(config)
  {
    // Getting emulator name (for runtime use)
    _emulatorName = jaffarCommon::json::popString(_emulatorConfigRemaining, "Emulator Name");
  };

  virtual ~Emulator() = default;

  /**
   * @brief Initializes the emulator instance.
   * @details Calls the core-specific @ref initializeImpl and marks the instance as initialized.
   * @throws A logic error if the instance was already initialized.
   */
  __INLINE__ void initialize()
  {
    if (_isInitialized == true) JAFFAR_THROW_LOGIC("This emulator instance was already initialized");

    // Calling emulator-specific initializer
    initializeImpl();

    // Set this as initialized
    _isInitialized = true;
  }

  /**
   * @brief Registers an input string, avoiding repeated decoding of the same input.
   * @details If the input is already registered, returns its existing id; otherwise decodes it via
   * the input parser and stores a new entry. Registration is linear O(n) to keep read access O(1).
   * @param inputString The input string to register.
   * @return The index of the registered input entry.
   */
  __INLINE__ InputSet::inputIndex_t registerInput(const std::string inputString)
  {
    // Registration is done only at the beginning with linear complexity O(n) to optimize for read access O(1)
    for (size_t i = 0; i < _inputMap.size(); i++)
      if (inputString == _inputMap[i].inputString) return i;

    // Otherwise, getting decoded input data from the emulator
    auto inputData = getInputParser()->parseInputString(inputString);

    // Otherwise, register it as a new entry
    _inputMap.push_back({inputString, inputData});

    // Returning current index
    return _inputMap.size() - 1;
  }

  /**
   * @brief Returns the information about a previously registered input.
   * @param inputIdx The index of the registered input.
   * @return A reference to the registered input entry.
   */
  __INLINE__ const inputEntry_t& getRegisteredInput(const InputSet::inputIndex_t inputIdx) const { return _inputMap[(size_t)inputIdx]; }

  /**
   * @brief Advances the emulator state by applying a registered input.
   * @param input The index of the registered input to apply.
   */
  void advanceState(const InputSet::inputIndex_t input) { advanceStateImpl(_inputMap[input].inputData); };

  /**
   * @brief Computes the serialized size of the emulator state.
   * @return The size, in bytes, of the serialized emulator state.
   */
  size_t getStateSize() const
  {
    jaffarCommon::serializer::Contiguous s;
    serializeState(s);
    return s.getOutputSize();
  }

  /** @brief Returns whether the emulator instance has been initialized. */
  __INLINE__ bool isInitialized() const { return _isInitialized; }

  /** @brief Returns the emulator's configured name. */
  __INLINE__ std::string getName() const { return _emulatorName; }

  /** @brief Core-specific initialization, invoked by @ref initialize. */
  virtual void initializeImpl() = 0;

  /**
   * @brief Temporarily re-enables ALL state properties disabled via configuration (e.g. NTAB/CHRR),
   *        so a FULL-fidelity snapshot can be captured for exact restores (player reload). Pair with
   *        @ref reapplyDisabledStateProperties. Default: no-op.
   */
  virtual void enableAllStateProperties() {}

  /** @brief Re-applies the configured disabled state properties after @ref enableAllStateProperties. Default: no-op. */
  virtual void reapplyDisabledStateProperties() {}

  /**
   * @brief Serializes the emulator state into the given serializer.
   * @param serializer The serializer to write the emulator state into.
   */
  virtual void serializeState(jaffarCommon::serializer::Base& serializer) const = 0;
  /**
   * @brief Deserializes the emulator state from the given deserializer.
   * @param deserializer The deserializer to read the emulator state from.
   */
  virtual void deserializeState(jaffarCommon::deserializer::Base& deserializer) = 0;

  /**
   * @brief Returns a reference to the emulator's input parser.
   * @return Pointer to the input parser owned by the core.
   */
  virtual jaffar::InputParser* getInputParser() const = 0;

  /** @brief Prints core-specific debug information. */
  virtual void printInfo() const = 0;

  /**
   * @brief Returns a memory property by name.
   * @param propertyName The name of the property to look up.
   * @return The property's memory pointer and size.
   */
  virtual property_t getProperty(const std::string& propertyName) const = 0;

  /**
   * @brief Constructs the emulator core selected by the configuration.
   * @param emulatorConfig Emulator configuration object.
   * @return An owning pointer to the constructed emulator.
   */
  static std::unique_ptr<Emulator> getEmulator(const nlohmann::json& emulatorConfig);

  /////// Render-related functions

  /** @brief Opens the video output (e.g., window). */
  virtual void initializeVideoOutput() = 0;

  /** @brief Closes the video output (e.g., window). */
  virtual void finalizeVideoOutput() = 0;

  /** @brief Enables rendering within the emulation core (does not output it to screen). */
  virtual void enableRendering() = 0;

  /** @brief Disables rendering within the emulation core (typically enables faster emulation). */
  virtual void disableRendering() = 0;

  /**
   * @brief Updates the internal state of the renderer with the current game state.
   * @param stepIdx The index of the step/frame being rendered.
   * @param input The input string associated with the step/frame.
   */
  virtual void updateRendererState(const size_t stepIdx, const std::string input) = 0;

  /**
   * @brief Gathers the data needed to render a given state/frame into the serializer.
   * @param serializer The serializer to write the renderer state into.
   */
  virtual void serializeRendererState(jaffarCommon::serializer::Base& serializer) const = 0;

  /**
   * @brief Loads the renderer state for a given state/frame from the deserializer.
   * @param deserializer The deserializer to read the renderer state from.
   */
  virtual void deserializeRendererState(jaffarCommon::deserializer::Base& deserializer) = 0;

  /**
   * @brief Returns the size of the renderer state.
   * @return The size, in bytes, of the serialized renderer state.
   */
  virtual size_t getRendererStateSize() const = 0;

  /** @brief Shows the contents of the emulator's renderer in the window. */
  virtual void showRender() = 0;

  /**
   * @brief Saves the currently-rendered frame to an image file at the given destination path.
   * @details Default: no-op (emulators without a screenshot backend simply ignore the request), so
   * the player can request a screenshot emulator-agnostically. Overrides take the destination file
   * path as their sole argument.
   */
  virtual void saveScreenshot(const std::string& /*path*/) {}

  /**
   * @brief Enables offline (no-window) frame rendering so subsequent steps can be screenshotted headlessly.
   * @details Default: no-op. Emulators with a windowless render path (e.g. QuickerGPGX) override this so the
   * player can capture screenshots without initializing SDL video output.
   */
  virtual void enableHeadlessRendering() {}

protected:
  /**
   * @brief Core-specific state advancement, invoked by @ref advanceState.
   * @param input The decoded, emulator-specific input data to apply.
   */
  virtual void advanceStateImpl(const jaffar::input_t& input) = 0;

  /**
   * @brief Asserts that every key in the emulator configuration has been recognized.
   *
   * @details Call this at the END of a derived core's constructor, after it has consumed all of its
   * own configuration keys (via the jaffarCommon::json::pop* helpers) from @ref
   * _emulatorConfigRemaining. The base ctor has already popped the common keys, so anything still
   * present is an unrecognized key (a typo or unsupported option) and is reported by name. Cores
   * that do not call this remain lenient (opt-in strict validation).
   */
  void finalizeEmulatorConfig() { jaffarCommon::json::checkEmpty(_emulatorConfigRemaining, "Emulator Configuration"); }

  /// @brief Emulator name (for runtime use).
  std::string _emulatorName;

  /// @brief Mutable working copy of the emulator config; recognized keys are popped, leftovers are unrecognized. See @ref finalizeEmulatorConfig().
  nlohmann::json _emulatorConfigRemaining;

  /// @brief Whether the emulator has been initialized.
  bool _isInitialized = false;

private:
  /// @brief Maps an input id to its input data.
  std::vector<inputEntry_t> _inputMap;
};

} // namespace jaffarPlus