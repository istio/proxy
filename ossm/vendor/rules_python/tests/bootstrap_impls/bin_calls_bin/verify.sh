#!/bin/bash
set -euo pipefail

verify_output() {
  local OUTPUT_FILE=$1

  # Extract the RULES_PYTHON_TESTING_MODULE_SPACE values
  local OUTER_MODULE_SPACE=$(grep "outer: RULES_PYTHON_TESTING_MODULE_SPACE" "$OUTPUT_FILE" | sed "s/outer: RULES_PYTHON_TESTING_MODULE_SPACE='\(.*\)'/\1/")
  local INNER_MODULE_SPACE=$(grep "inner: RULES_PYTHON_TESTING_MODULE_SPACE" "$OUTPUT_FILE" | sed "s/inner: RULES_PYTHON_TESTING_MODULE_SPACE='\(.*\)'/\1/")

  echo "Outer module space: $OUTER_MODULE_SPACE"
  echo "Inner module space: $INNER_MODULE_SPACE"

  # Check 1: The two values are different
  if [ "$OUTER_MODULE_SPACE" == "$INNER_MODULE_SPACE" ]; then
    echo "Error: Outer and Inner module spaces are the same."
    exit 1
  fi

  # Check 2: Inner is not a subdirectory of Outer
  case "$INNER_MODULE_SPACE" in
    "$OUTER_MODULE_SPACE"/*)
      echo "Error: Inner module space is a subdirectory of Outer's."
      exit 1
      ;;
    *)
      # This is the success case
      ;;
  esac

  echo "Verification successful."
}
