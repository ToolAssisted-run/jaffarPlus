#pragma once

/**
 * @file environment.hpp
 * @brief Reads the numeric JAFFAR_* environment overrides, rejecting malformed values with a clear error.
 */

#include <cstdlib>
#include <jaffarCommon/exceptions.hpp>
#include <string>

namespace jaffarPlus
{

/**
 * @brief Reads a non-negative integer from an environment variable, if it is set.
 * @param name  The variable name.
 * @param value Receives the value when the variable is set; left unchanged otherwise.
 * @return true if the variable was set (and @p value updated), false if it is not set.
 * @throws A runtime error if the variable is set but is not a non-negative integer.
 */
inline bool getEnvironmentUnsigned(const char* name, size_t& value)
{
  const char* text = std::getenv(name);
  if (text == nullptr) return false;
  char*               end    = nullptr;
  const unsigned long parsed = std::strtoul(text, &end, 10);
  if (text[0] == '\0' || text[0] == '-' || *end != '\0') JAFFAR_THROW_RUNTIME("[ERROR] Environment variable %s must be a non-negative integer (got '%s')\n", name, text);
  value = parsed;
  return true;
}

} // namespace jaffarPlus
