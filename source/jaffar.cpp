/**
 * @file jaffar.cpp
 * @brief The jaffar executable: parses a configuration script and either validates it (@c --dryRun)
 *        or builds and runs the search engine via a @ref jaffarPlus::Driver.
 *
 * @details Reads the JSON config file named on the command line. In dry-run mode it only constructs
 * the driver to validate the configuration (signalling the construction path via the
 * @c JAFFAR_IS_DRY_RUN environment variable so host-specific side effects are skipped) and exits.
 * Otherwise it initializes and runs the driver, then reports the exit reason and final step.
 */

#include "driver.hpp"
#include <argparse/argparse.hpp>
#include <cstdlib>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/string.hpp>

/**
 * @brief Entry point for the jaffar engine executable.
 *
 * @details Parses the command line (required @c configFile and the @c --dryRun flag), loads and
 * parses the JSON config, and constructs a @ref jaffarPlus::Driver from it. In dry-run mode it sets
 * the @c JAFFAR_IS_DRY_RUN environment variable before constructing the driver (so host-specific side
 * effects -- the NUMA topology check and trace-file loading -- are skipped) and returns after
 * successful validation. Otherwise it initializes and runs the driver, maps the resulting
 * @ref jaffarPlus::Driver::exitReason_t to a message, and prints the final step and exit reason.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return The driver's exit reason value (in non-dry-run mode), or 0 after a successful dry run.
 */
int main(int argc, char* argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("jaffar", "2.0.0");

  program.add_argument("configFile").help("path to the Jaffar configuration script (.jaffar) file to run.").required();
  program.add_argument("--dryRun").help("Only check for configuration without initializing or running the engine").default_value(false).implicit_value(true);

  // Try to parse arguments
  try
  {
    program.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err)
  {
    JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str());
  }

  // Getting config file name
  const std::string configFile = program.get<std::string>("configFile");

  // Getting dry run option
  const bool isDryRun = program.get<bool>("--dryRun");

  // In dry-run mode we only validate the configuration. Signal this to the construction path (via the
  // same env var the test harness uses) so host-specific side effects -- the NUMA topology check and
  // trace-file loading -- are skipped: config validation must not depend on the host or on data files.
  if (isDryRun) setenv("JAFFAR_IS_DRY_RUN", "1", 1);

  // Reporting script file
  jaffarCommon::logger::log("[J+] Loading script file: '%s'\n", configFile.c_str());

  // Loading script file contents
  std::string configFileString;
  if (jaffarCommon::file::loadStringFromFile(configFileString, configFile) == false)
    JAFFAR_THROW_RUNTIME("[ERROR] Could not find or read from Jaffar config file: %s\n%s \n", configFile.c_str(), program.help().str().c_str());

  // Parsing JSON from script file
  nlohmann::json config;
  try
  {
    config = nlohmann::json::parse(configFileString);
  }
  catch (const std::exception& err)
  {
    JAFFAR_THROW_LOGIC("[ERROR] Parsing configuration file %s. Details:\n%s\n", configFile.c_str(), err.what());
  }

  // Creating driver to run the Jaffar engine
  auto d = jaffarPlus::Driver::getDriver(configFile, config);

  // Returning now if dry running
  if (isDryRun)
  {
    jaffarCommon::logger::log("[J+] Finished dry run successfully.\n");
    return 0;
  }

  // Initializing driver
  d->initialize();

  // Running driver
  auto exitReason = d->run();

  // Printing exit reason
  std::string exitReasonString;
  if (exitReason == jaffarPlus::Driver::exitReason_t::winStateFound) exitReasonString = "Solution found.";
  if (exitReason == jaffarPlus::Driver::exitReason_t::winCollectionFull) exitReasonString = "Win collection complete (Max Files reached).";
  if (exitReason == jaffarPlus::Driver::exitReason_t::outOfStates) exitReasonString = "Engine ran out of states.";
  if (exitReason == jaffarPlus::Driver::exitReason_t::maximumStepReached) exitReasonString = "Maximum step count reached.";
  if (exitReason == jaffarPlus::Driver::exitReason_t::bestBelowReference) exitReasonString = "Best fell below the reference reward floor.";
  if (exitReason == jaffarPlus::Driver::exitReason_t::referenceBelowWorst) exitReasonString = "Reference fell below the worst kept state (reference evicted from the frontier).";
  if (exitReason == jaffarPlus::Driver::exitReason_t::exceededReferenceFrames) exitReasonString = "Reached the reference's frame count without winning (can no longer beat it).";
  if (exitReason == jaffarPlus::Driver::exitReason_t::inputHistoryNearCapacity)
    exitReasonString = "Input-history trie neared its hard memory ceiling (use Type \"Raw\" or a smaller State DB).";

  // Getting current step
  auto finalStep = d->getCurrentStep();

  // Printing exit message
  jaffarCommon::logger::log("[J+] Step %lu - Exit Reason: %s\n", finalStep, exitReasonString.c_str());

  // Return exit reason
  return exitReason;
}
