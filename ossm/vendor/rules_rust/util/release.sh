#!/usr/bin/env bash
#
# Usage: util/release.sh [version]
#
# where version (optional) is the new version of rules_rust.
set -xeuo pipefail

# Normalize working directory to root of repository.
cd "$(dirname "${BASH_SOURCE[0]}")"/..

# Read the old version.
readonly OLD="$(cat version.bzl | grep VERSION | awk '{print $3}' | tr -d '"')"

function new_from_old() {
  local major=$(awk -F. '{print $1}' <<<"$OLD")
  local minor=$(awk -F. '{print $2}' <<<"$OLD")
  echo "$major.$((minor + 1)).0"
}

readonly NEW="${1:-$(new_from_old)}"

# Update matching VERSION constants in version.bzl files.
function version_pattern() {
  local version_quoted="\"$1\""
  echo "VERSION = $version_quoted"
}

grep -rl \
  --include='version.bzl' \
  "$(version_pattern $OLD)" \
  | xargs sed -i "s/^$(version_pattern $OLD)$/$(version_pattern $NEW)/g"

# Update matching bazel_dep(name = "rules_rust", version = ...) declarations.
function bazel_dep_pattern() {
  local version_quoted="\"$1\""
  echo "bazel_dep(name = \"rules_rust\", version = $version_quoted)"
}

grep -rl \
  --include='MODULE.bazel' --include='*.bzl' --include='*.md' \
  "$(bazel_dep_pattern $OLD)" \
  | xargs sed -i "s/^$(bazel_dep_pattern $OLD)$/$(bazel_dep_pattern $NEW)/"

# Update module() declarations:
# module(
#     name = "rules_rust",
#     version = "...",
# )
function update_module_version() {
  local file=$1

  local out="$(awk -v old=$OLD -v new=$NEW '
  BEGIN {
    VERSION_PATTERN = "version = \"" old "\"";
    NAME_PATTERN = "name = \"rules_rust.*\"";
    CLOSING_PAREN_PATTERN = "^)$";

    inside_module = 0;
    name_matches = 0;
  }
  /^module\($/ {
    inside_module = 1;
  }
  inside_module {
    if ($0 ~ NAME_PATTERN) {
      name_matches = 1;
    } else if ($0 ~ VERSION_PATTERN && name_matches) {
      gsub(old, new);
    } else if ($0 ~ CLOSING_PAREN_PATTERN) {
      inside_module = 0;
      name_matches = 0;
    }
  }
  { print }
  ' "$file")"
  echo "$out" > "$file"
}

find . -name MODULE.bazel -print0 | while IFS= read -r -d $'\0' file; do
  update_module_version "$file"
done
