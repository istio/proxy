#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail

JQ_FILTER=\
'map(select(.name | contains("-rc") | not)) | map(
  {
    "key": .tag_name,
    "value": .assets
        | map(select((.name | contains("cosign-")) and (.name | contains(".") | not) and (.name | contains("key") | not) ))
        | map({
            "key": .name,
            "value": .browser_download_url
        })
        | from_entries
  }
) | from_entries'

REPOSITORY=${1:-"sigstore/cosign"}


VERSIONS=$(curl --silent  -H "Accept: application/vnd.github.v3+json" "https://api.github.com/repos/$REPOSITORY/releases?per_page=20" | jq "$JQ_FILTER")


# Replace URLs with their hash
for TAG in $(jq -r 'keys | .[]' <<< $VERSIONS); do
  CHECKSUMS="$(curl --silent -L https://github.com/$REPOSITORY/releases/download/$TAG/cosign_checksums.txt)"
  >&2 echo -n "$TAG "
  while read -r SHA256 FILENAME; do
    INTEGRITY="sha256-$(echo $SHA256 | xxd -r -p | base64)"
    VERSIONS=$(jq --arg tag "$TAG" --arg filename "$FILENAME" --arg sha256 "$INTEGRITY"  'if (.[$tag] | has($filename)) then .[$tag][$filename] = $sha256 else . end' <<< $VERSIONS)
    >&2 echo -n "."
  done <<< "$CHECKSUMS"
  >&2 echo ""
done

clear
echo -n "COSIGN_VERSIONS = "
jq 'with_entries(.value |= with_entries(.key |= ltrimstr("cosign-")))' <<< $VERSIONS


echo ""
echo "Copy the version info into cosign/private/versions.bzl"