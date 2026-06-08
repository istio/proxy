#!/usr/bin/env bash

FAIL_ERR_MSGS=()

fail() {
  local err_msg="${1:-"Unspecified error occurred."}"
  FAIL_ERR_MSGS+=("${err_msg}")
}

reset_fail_err_msgs() {
  FAIL_ERR_MSGS=()
}

new_fail() {
  local err_msg="${1:-}"
  [[ -n "${err_msg}" ]] || err_msg="Unspecified error occurred."
  echo >&2 "${err_msg}"
  exit 1
}

assert_fail() {
  local pattern=${1}
  [[ ${#FAIL_ERR_MSGS[@]} == 0 ]] &&
    new_fail "Expected a failure. None found. pattern: ${pattern}"
  [[ ${#FAIL_ERR_MSGS[@]} -gt 1 ]] &&
    new_fail "Expected a single failure. Found ${#FAIL_ERR_MSGS[@]}. pattern: ${pattern}"
  [[ "${FAIL_ERR_MSGS[0]}" =~ ${pattern} ]] ||
    new_fail "Unexpected failure. Found '${FAIL_ERR_MSGS[0]}'. pattern: ${pattern}"
}

assert_no_fail() {
  [[ ${#FAIL_ERR_MSGS[@]} == 0 ]] || (
    err_msg=$("${FAIL_ERR_MSGS[@]}") &&
      new_fail "Expected no failures. Found ${#FAIL_ERR_MSGS[@]}. '${err_msg}'"
  )
}
