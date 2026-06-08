#!/bin/bash
set -e

# The file path is passed as the first argument.
QUERY_OUTPUT="$1"

if [ -z "$QUERY_OUTPUT" ]; then
  echo "Could not find query output file"
  exit 1
fi

CONTENT=$(cat "$QUERY_OUTPUT")

# The query should return the target itself
if [ -n "$CONTENT" ] && [[ "$CONTENT" == *"com_squareup_okio_okio"* ]]; then
  echo "SUCCESS: Target exists and has custom visibility set"
else
  echo "FAILURE: Target not found or visibility not properly set. Content: $CONTENT"
  exit 1
fi
