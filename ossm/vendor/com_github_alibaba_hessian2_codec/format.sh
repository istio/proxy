#!/usr/bin/env bash

CMD=clang-format
$CMD -version
$CMD -i  $(git ls-files|grep -E ".*\.(cc|h|hpp)$")
CHANGED="$(git ls-files --modified)"
if [[ ! -z "$CHANGED" ]]; then
  echo "The following files have changes:"
  echo "$CHANGED"
  exit 1
else
  echo "No changes."
fi
