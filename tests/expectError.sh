#!/usr/bin/env bash
# Runs a command that must fail with a reported error: it passes only if the command exits with the
# error code the executables use for a caught exception (8) and its output contains the expected
# message. A crash (signal), a sanitizer abort or any other exit code fails the test.
#
# usage: expectError.sh <expected message substring> <command> [args...]
set -uo pipefail

EXPECTED="${1:?usage: expectError.sh <expected message> <command> [args...]}"
shift

out="$("$@" 2>&1)"
rc=$?

if [[ $rc -ne 8 ]]; then
  printf '%s\n' "$out" | tail -n 20
  echo "FAIL: expected exit code 8 (reported error), got $rc"
  exit 1
fi
if [[ "$out" != *"$EXPECTED"* ]]; then
  printf '%s\n' "$out" | tail -n 20
  echo "FAIL: the error message does not contain '$EXPECTED'"
  exit 1
fi
echo "PASS: failed cleanly with '$EXPECTED'"
exit 0
