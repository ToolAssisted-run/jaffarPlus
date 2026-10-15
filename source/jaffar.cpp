/**
 * @file jaffar.cpp
 * @brief The jaffar executable: parses a configuration script and either validates it (@c --dryRun)
 *        or builds and runs the search engine via a @ref jaffarPlus::Driver.
 *
 * @details Reads the JSON config file named on the command line. In dry-run mode it only constructs
 * the driver to validate the configuration (signalling the construction path via the
 * @c JAFFAR_IS_DRY_RUN environment variable so data-file loading is skipped) and exits.
 * Otherwise it initializes and runs the driver, then reports the exit reason and final step. Any
 * error (bad arguments or configuration, missing files, failed allocations) is reported with its
 * message and ends the process with exit code @ref errorExitCode instead of an abort.
 */

#include "driver.hpp"
#include <argparse/argparse.hpp>
#include <cstdlib>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/string.hpp>

/// @brief Process exit code for an error (an exception reaching main), distinct from every exit reason.
constexpr int errorExitCode = 8;

/// @brief The program proper (see @ref main, which reports its errors).
int run(int argc, char* argv[]);

/**
 * @brief Entry point for the jaffar engine executable.
 *
 * @details Parses the command line (required @c configFile and the @c --dryRun flag), loads and
 * parses the JSON config, and constructs a @ref jaffarPlus::Driver from it. In dry-run mode it sets
 * the @c JAFFAR_IS_DRY_RUN environment variable before constructing the driver (so data-file loading
 * is skipped) and returns after
 * successful validation. Otherwise it initializes and runs the driver, maps the resulting
 * @ref jaffarPlus::Driver::exitReason_t to a message, and prints the final step and exit reason.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return The driver's exit reason value (in non-dry-run mode), 0 after a successful dry run, or
 *         @ref errorExitCode if an error was raised.
 */
int main(int argc, char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (const std::exception& e)
  {
    fflush(stdout);
    fprintf(stderr, "[J+] Error: %s\n", e.what());
    return errorExitCode;
  }
}

int run(int argc, char* argv[])
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

  // In dry-run mode we only validate the configuration. Signal this to the construction path (via an
  // environment variable, which game modules read as well) so data-file loading (traces, reference
  // solutions and hashes) is skipped: config validation must not depend on data files. A real run
  // clears the variable, since a value left in the environment would silently disable those files.
  if (isDryRun) setenv("JAFFAR_IS_DRY_RUN", "1", 1);
  if (isDryRun == false && std::getenv("JAFFAR_IS_DRY_RUN") != nullptr)
  {
    jaffarCommon::logger::log("[J+] Warning: ignoring JAFFAR_IS_DRY_RUN from the environment (use --dryRun for a dry run)\n");
    unsetenv("JAFFAR_IS_DRY_RUN");
  }

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
