#!/usr/bin/env bash

# Fail a test with the specified message.
#
# Args:
#   err_msg: The message to print to stderr.
#
# Outputs:
#   stdout: None.
#   stderr: The error message.
fail() {
  local err_msg="${1:-}"
  if [[ -z "${err_msg:-}" ]]; then
    err_msg="Unspecified error occurred."
  fi
  echo >&2 "${err_msg}"
  exit 1
}

make_err_msg() {
  local err_msg="${1}"
  local prefix="${2:-}"
  if [[ -n "${prefix:-}" ]]; then
    err_msg="${prefix} ${err_msg}"
  fi
  echo "${err_msg}"
}

# Asserts that the actual value equals the expected value.
#
# Args:
#   expected: The expected value.
#   actual: The actual value.
#   err_msg: Optional. The error message to print if the assertion fails.
#
# Outputs:
#   stdout: None.
#   stderr: None.
assert_equal() {
  local expected="${1}"
  local actual="${2}"
  local err_msg
  err_msg="$(
    make_err_msg \
      "Expected to be equal. expected: ${expected}, actual: ${actual}" "${3:-}"
  )"
  if [[ "${expected}" != "${actual}" ]]; then
    fail "${err_msg}"
  fi
}

# Asserts that the actual value contains the specified regex pattern.
#
# Args:
#   pattern: The expected pattern.
#   actual: The actual value.
#   err_msg: Optional. The error message to print if the assertion fails.
#
# Outputs:
#   stdout: None.
#   stderr: None.
assert_match() {
  local pattern=${1}
  local actual="${2}"
  local err_msg
  err_msg="$(
    make_err_msg \
      "Expected to match. pattern: ${pattern}, actual: ${actual}" "${3:-}"
  )"
  if [[ ! "${actual}" =~ ${pattern} ]]; then
    fail "${err_msg}"
  fi
}

# Asserts that the actual value does not contain the specified regex pattern.
#
# Args:
#   pattern: The expected pattern.
#   actual: The actual value.
#   err_msg: Optional. The error message to print if the assertion fails.
#
# Outputs:
#   stdout: None.
#   stderr: None.
assert_no_match() {
  local pattern=${1}
  local actual="${2}"
  local err_msg
  err_msg="$(
    make_err_msg \
      "Expected not to match. pattern: ${pattern}, actual: ${actual}" "${3:-}"
  )"
  if [[ "${actual}" =~ ${pattern} ]]; then
    fail "${err_msg}"
  fi
  # Because this is a negative test, we need to end on a positive note if all is well.
  echo ""
}
