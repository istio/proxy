# --- begin runfiles.bash initialization v2 ---
# Copy-pasted from the Bazel Bash runfiles library v2.
set -uo pipefail; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v2 ---

set -euox pipefail

deps_file=$(rlocation rules_jvm_external/tests/integration/override_targets/trace_otel_deps)

function clean_up_workspace_names() {
  local file_name="$1"
  local target="$2"
  # The first `sed` command replaces `@@` with `@`. The second extracts the visible name
  # from the bzlmod mangled workspace name
  cat "$file_name" | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | grep "$target"
  cat "$file_name" | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | grep -q "$target"
}

# we should contain the original target
if ! clean_up_workspace_names "$deps_file" "@override_target_in_deps//:io_opentelemetry_opentelemetry_sdk"; then
  echo "Unable to find SDK target"
  exit 1
fi

# should contain the "raw" dep
if ! clean_up_workspace_names "$deps_file" "@override_target_in_deps//:original_io_opentelemetry_opentelemetry_api"; then
  echo "Unable to find raw API target"
  exit 1
fi

# the "context" dependency is depended upon by `io.opentelemetry:opentelemetry-api` and
# nothing else in the SDK. If we have built the raw dependency properly, this should
# also be present in the dependencies
if ! clean_up_workspace_names "$deps_file" "@override_target_in_deps//:io_opentelemetry_opentelemetry_context"; then
  echo "Unable to find transitive dep of raw target"
  exit 1
fi

# Finally, we expect jedis (which is not an OTel dep) to have also been added
if ! clean_up_workspace_names "$deps_file" "@override_target_in_deps//:redis_clients_jedis"; then
  echo "Unable to find additional target added to a transitive dep of the SDK"
  exit 1
fi
