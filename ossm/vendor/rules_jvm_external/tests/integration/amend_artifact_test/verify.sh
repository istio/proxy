#!/bin/bash
set -e
GUAVA_OUTPUT="$1"
LANG3_OUTPUT="$2"
SLF4J_OUTPUT="$3"

if [ -s "$GUAVA_OUTPUT" ]; then
  echo "Error: Guava output is NOT empty. 'testonly' was not unset."
  cat "$GUAVA_OUTPUT"
  exit 1
fi

if [ ! -s "$LANG3_OUTPUT" ]; then
  echo "Error: Lang3 output is empty. 'testonly_int=1' failed."
  exit 1
fi

if [ ! -s "$SLF4J_OUTPUT" ]; then
  echo "Error: Slf4j output is empty. 'neverlink_int=1' failed."
  exit 1
fi

echo "All verification checks passed!"

