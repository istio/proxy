#!/usr/bin/env bash
set -eufo pipefail

root="$BUILD_WORKSPACE_DIRECTORY/$1"
shift

while (( "$#" )); do
  src="$1"
  dest="$2"

  shift 2

  cp -f "$src" "$root/$dest"
done
