# Copyright 2018 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Runfiles lookup library for Bazel-built Bash binaries and tests, version 2.
#
# VERSION HISTORY:
# - version 2: Shorter init code.
#   Features:
#     - "set -euo pipefail" only at end of init code.
#       "set -e" breaks the source <path1> || source <path2> || ... scheme on
#       macOS, because it terminates if path1 does not exist.
#     - Not exporting any environment variables in init code.
#       This is now done in runfiles.bash itself.
#   Compatibility:
#     - The v1 init code can load the v2 library, i.e. if you have older source
#       code (still using v1 init) then you can build it with newer Bazel (which
#       contains the v2 library).
#     - The reverse is not true: the v2 init code CANNOT load the v1 library,
#       i.e. if your project (or any of its external dependencies) use v2 init
#       code, then you need a newer Bazel version (which contains the v2
#       library).
# - version 1: Original Bash runfiles library.
#
# ENVIRONMENT:
# - If RUNFILES_LIB_DEBUG=1 is set, the script will print diagnostic messages to
#   stderr.
#
# USAGE:
# 1.  Depend on this runfiles library from your build rule:
#
#       sh_binary(
#           name = "my_binary",
#           ...
#           deps = ["@bazel_tools//tools/bash/runfiles"],
#       )
#
# 2.  Source the runfiles library.
#
#     The runfiles library itself defines rlocation which you would need to look
#     up the library's runtime location, thus we have a chicken-and-egg problem.
#     Insert the following code snippet to the top of your main script:
#
#       # --- begin runfiles.bash initialization v2 ---
#       # Copy-pasted from the Bazel Bash runfiles library v2.
#       set -uo pipefail; f=bazel_tools/tools/bash/runfiles/runfiles.bash
#       source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
#         source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
#         source "$0.runfiles/$f" 2>/dev/null || \
#         source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
#         source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
#         { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
#       # --- end runfiles.bash initialization v2 ---
#
#
# 3.  Use rlocation to look up runfile paths.
#
#       cat "$(rlocation my_workspace/path/to/my/data.txt)"
#

case "$(uname -s | tr [:upper:] [:lower:])" in
msys*|mingw*|cygwin*)
  # matches an absolute Windows path
  export _RLOCATION_ISABS_PATTERN="^[a-zA-Z]:[/\\]"
  ;;
*)
  # matches an absolute Unix path
  # rules_nodejs modification
  # In the upstream this pattern requires a second character which is not a slash
  # https://github.com/bazelbuild/bazel/blob/22d376cf41d50bfee129a0a7fa656d66af2dbf14/tools/bash/runfiles/runfiles.bash#L88
  # However in integration testing with rules_docker we observe runfiles path starting with two slashes
  # This fails on Linux CI (not Mac or Windows) in our //e2e:e2e_nodejs_image:
  # ERROR[runfiles.bash]: cannot look up runfile "nodejs_linux_amd64/bin/nodejs/bin/node"
  # (RUNFILES_DIR="/app/main.runfiles/e2e_nodejs_image//app//main.runfiles", RUNFILES_MANIFEST_FILE="")
  export _RLOCATION_ISABS_PATTERN="^/.*"
  ;;
esac

if [[ ! -d "${RUNFILES_DIR:-/dev/null}" && ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  if [[ -f "$0.runfiles_manifest" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles_manifest"
  elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
  elif [[ -f "$0.runfiles/build_bazel_rules_nodejs/third_party/github.com/bazelbuild/bazel/tools/bash/runfiles/runfiles.bash" ]]; then
    if [[ "${0}" =~ $_RLOCATION_ISABS_PATTERN ]]; then
      export RUNFILES_DIR="${0}.runfiles"
    else
      export RUNFILES_DIR="${PWD}/${0}.runfiles"
    fi
  fi
fi

# --- begin rules_nodejs custom code ---
# normpath() function removes all `/./` and `dir/..` sequences from a path
function normpath() {
  # Remove all /./ sequences.
  local path=${1//\/.\//\/}
  # Remove dir/.. sequences.
  while [[ $path =~ ([^/][^/]*/\.\./) ]]; do
    path=${path/${BASH_REMATCH[0]}/}
  done
  echo $path
}
# --- end rules_nodejs custom code ---

# Prints to stdout the runtime location of a data-dependency.
function rlocation() {
  if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
    echo >&2 "INFO[runfiles.bash]: rlocation($1): start"
  fi
  if [[ "$1" =~ $_RLOCATION_ISABS_PATTERN ]]; then
    if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
      echo >&2 "INFO[runfiles.bash]: rlocation($1): absolute path, return"
    fi
    # If the path is absolute, print it as-is.
    echo "$1"
  # --- begin rules_nodejs custom code ---
  # We allow "./*" and "../*" as these are the formats returned from $(rootpath)
  # "./*" for a root "//:file"
  # "../*" for an external repo "@other_repo//path/to:file"
  elif [[ "$1" == */.. || "$1" == */./* || "$1" == "*/." || "$1" == *//* ]]; then
  # --- end rules_nodejs custom code ---
    # --- begin rules_nodejs custom code ---
    # We print error messages regardless of RUNFILES_LIB_DEBUG value
    # TODO: upstream this change to Bazel as it seems correct
    echo >&2 "ERROR[runfiles.bash]: rlocation($1): path is not normalized"
    # --- end rules_nodejs custom code ---
    return 1
  elif [[ "$1" == \\* ]]; then
    # --- begin rules_nodejs custom code ---
    # We print error messages regardless of RUNFILES_LIB_DEBUG value
    # TODO: upstream this change to Bazel as it seems correct
    echo >&2 "ERROR[runfiles.bash]: rlocation($1): absolute path without" \
              "drive name"
    # --- end rules_nodejs custom code ---
    return 1
  else
    # --- begin rules_nodejs custom code ---
    # Normalize ${BAZEL_WORKSPACE}/$1.
    # If $1 is a $(rootpath) this will convert it to the runfiles manifest path
    readonly from_rootpath=$(normpath ${BAZEL_WORKSPACE:-/dev/null}/$1)
    # --- end rules_nodejs custom code ---
    if [[ -e "${RUNFILES_DIR:-/dev/null}/$1" ]]; then
      if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
        echo >&2 "INFO[runfiles.bash]: rlocation($1): found under RUNFILES_DIR ($RUNFILES_DIR), return"
      fi
      echo "${RUNFILES_DIR}/$1"
    # --- begin rules_nodejs custom code ---
    # If $1 is a rootpath then check if the converted rootpath to runfiles manifest path file is found
    elif [[ -e "${RUNFILES_DIR:-/dev/null}/${from_rootpath}" ]]; then
      if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
        echo >&2 "INFO[runfiles.bash]: rlocation($1): found under RUNFILES_DIR/BAZEL_WORKSPACE ($RUNFILES_DIR/$BAZEL_WORKSPACE), return"
      fi
      echo "${RUNFILES_DIR}/${from_rootpath}"
    # --- end rules_nodejs custom code ---
    elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
      if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
        echo >&2 "INFO[runfiles.bash]: rlocation($1): looking in RUNFILES_MANIFEST_FILE ($RUNFILES_MANIFEST_FILE)"
      fi
      local result=$(grep -m1 "^$1 " "${RUNFILES_MANIFEST_FILE}" | cut -d ' ' -f 2-)
      # --- begin rules_nodejs custom code ---
      # If $1 is not found in the MANIFEST then check if the converted rootpath
      # to runfiles manifest path file is found in the MANIFEST in the case the
      # $1 is a rootpath.
      if [[ -z "${result}" ]]; then
        result=$(grep -m1 "^${from_rootpath} " "${RUNFILES_MANIFEST_FILE}" | cut -d ' ' -f 2-)
      fi
      # --- end rules_nodejs custom code ---
      if [[ -e "${result:-/dev/null}" ]]; then
        if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
          echo >&2 "INFO[runfiles.bash]: rlocation($1): found in manifest as ($result)"
        fi
        echo "$result"
      else
        if [[ "${RUNFILES_LIB_DEBUG:-}" == 1 ]]; then
          echo >&2 "INFO[runfiles.bash]: rlocation($1): not found in manifest"
        fi
        echo ""
      fi
    else
      # --- begin rules_nodejs custom code ---
      # We print error messages regardless of RUNFILES_LIB_DEBUG value
      # TODO: upstream this change to Bazel as it seems correct
      echo >&2 "ERROR[runfiles.bash]: cannot look up runfile \"$1\" " \
               "(RUNFILES_DIR=\"${RUNFILES_DIR:-}\"," \
               "RUNFILES_MANIFEST_FILE=\"${RUNFILES_MANIFEST_FILE:-}\")"
      # --- end rules_nodejs custom code ---
      return 1
    fi
  fi
}
export -f rlocation

# Exports the environment variables that subprocesses need in order to use
# runfiles.
# If a subprocess is a Bazel-built binary rule that also uses the runfiles
# libraries under @bazel_tools//tools/<lang>/runfiles, then that binary needs
# these envvars in order to initialize its own runfiles library.
function runfiles_export_envvars() {
  if [[ ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" \
        && ! -d "${RUNFILES_DIR:-/dev/null}" ]]; then
    return 1
  fi

  if [[ ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
    if [[ -f "$RUNFILES_DIR/MANIFEST" ]]; then
      export RUNFILES_MANIFEST_FILE="$RUNFILES_DIR/MANIFEST"
    elif [[ -f "${RUNFILES_DIR}_manifest" ]]; then
      export RUNFILES_MANIFEST_FILE="${RUNFILES_DIR}_manifest"
    else
      export RUNFILES_MANIFEST_FILE=
    fi
  elif [[ ! -d "${RUNFILES_DIR:-/dev/null}" ]]; then
    if [[ "$RUNFILES_MANIFEST_FILE" == */MANIFEST \
          && -d "${RUNFILES_MANIFEST_FILE%/MANIFEST}" ]]; then
      export RUNFILES_DIR="${RUNFILES_MANIFEST_FILE%/MANIFEST}"
      export JAVA_RUNFILES="$RUNFILES_DIR"
    elif [[ "$RUNFILES_MANIFEST_FILE" == *_manifest \
          && -d "${RUNFILES_MANIFEST_FILE%_manifest}" ]]; then
      export RUNFILES_DIR="${RUNFILES_MANIFEST_FILE%_manifest}"
      export JAVA_RUNFILES="$RUNFILES_DIR"
    else
      export RUNFILES_DIR=
    fi
  fi
}
export -f runfiles_export_envvars
