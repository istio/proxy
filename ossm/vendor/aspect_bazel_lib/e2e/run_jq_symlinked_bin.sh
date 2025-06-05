#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail

BZLMOD_FLAG="${BZLMOD_FLAG:-}"

case "$(uname -s)" in
CYGWIN* | MINGW32* | MSYS* | MINGW*)
  bazel run $BZLMOD_FLAG @jq//:jq.exe -- --null-input .a=5
  ;;

*)
  bazel run $BZLMOD_FLAG @jq//:jq -- --null-input .a=5
  ;;
esac
