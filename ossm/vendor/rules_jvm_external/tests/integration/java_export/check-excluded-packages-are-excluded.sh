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

set -uox pipefail

jar_file=$(rlocation rules_jvm_external/tests/integration/java_export/with-proto-dep-project.jar)
# Calling rlocation will also call `set -e` (see line 9)
set +e

found=$(jar tvf "${jar_file}" | grep "com/google/protobuf")

if [ "$found" ]; then
  echo "Unexpectedly found compiled com/google/protobuf files in jar"
  exit 1
fi

pom_file=$(rlocation rules_jvm_external/tests/integration/java_export/with-proto-dep-pom.xml)
set +e

found=$(cat "${pom_file}" | grep "protobuf-java")

if [ "$found" ]; then
  echo "Unexpectedly found dependency on protobuf when none listed"
  exit 1
fi

pom_file_with_deps=$(rlocation rules_jvm_external/tests/integration/java_export/with-added-dependency-pom.xml)
set +e

# Test will fail if we don't find the dep
found=$(cat "${pom_file_with_deps}" | grep "protobuf-java")
if [ ! "$found" ]; then
  echo "Did not find expected dependency on protobuf when none listed"
  exit 1
fi
