#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail

JQ_FILTER=\
'map({
    "key": .tag_name, 
    "value": .assets 
        | map( select( .name | startswith("regctl-") ) )
        | map({ key: .name | ltrimstr("regctl-") | rtrimstr(".exe"), value: .name })
        | from_entries 
}) | from_entries
'


INFO="$(curl --silent -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/regclient/regclient/releases?per_page=1 | jq "$JQ_FILTER")"

for VERSION in $(jq -r 'keys | join("\n")' <<< $INFO); do 
    ALL_INFO=$(jq -r ".[\"$VERSION\"] | to_entries[] | \"\(.key) \(.value)\"" <<< $INFO)
    while read -r PLATFORM FILENAME; do 
        SHA256=$(curl -fLs "https://github.com/regclient/regclient/releases/download/$VERSION/$FILENAME" | shasum -a 256 | xxd -r -p | base64)
        INFO=$(jq ".[\"$VERSION\"][\"$PLATFORM\"] = \"sha256-$SHA256\"" <<< $INFO)
    done <<< "$ALL_INFO"
done

echo -n "REGCTL_VERSIONS = "
echo $INFO | jq -M

echo ""
echo "Copy the version info into oci/private/versions.bzl"
