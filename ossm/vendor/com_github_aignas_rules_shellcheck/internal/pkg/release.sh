#!/usr/bin/env bash
set -euo pipefail

DST="${BUILD_WORKSPACE_DIRECTORY}/$1"
GITHUB_REF_NAME="${2:-{BUILD_EMBED_LABEL}}"

# GH PRs will have the `BUILD_EMBED_LABEL` as `<number>/merge` and
# in order to keep the logic of manipulating the GITHUB_REF_NAME
# we have it here. This makes it easier to test.
#
# https://docs.github.com/en/actions/learn-github-actions/contexts#github-context
if [[ "$GITHUB_REF_NAME" == *"/merge" ]]; then
  GITHUB_REF_NAME="PR${GITHUB_REF_NAME%%\/*}"
fi

mkdir -p "$DST"

RELEASE_ARCHIVE="$DST/rules_shellcheck-$GITHUB_REF_NAME.tar.gz"
RELEASE_NOTES="$DST/release_notes.md"

cp -v "$ARCHIVE" "$RELEASE_ARCHIVE" >&2
chmod 644 "$RELEASE_ARCHIVE"
SHA=$(sha256sum "$RELEASE_ARCHIVE" | awk '{print $1}')

sed \
  -e "s/%%TAG%%/$GITHUB_REF_NAME/g" \
  -e "s/%%SHA256%%/$SHA/g" \
  "${RELEASE_NOTES_TEMPLATE}" \
  > "$RELEASE_NOTES"

# Output the release artifact path
echo "$RELEASE_ARCHIVE"
