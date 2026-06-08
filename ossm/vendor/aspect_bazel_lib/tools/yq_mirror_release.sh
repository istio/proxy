#!/usr/bin/env bash
# Produce a dictionary for the current yq release,
# suitable for adding to lib/private/yq_toolchain.bzl

set -o errexit -o nounset -o pipefail

# Find the latest version
if [ "${1:-}" ]; then
  version=$1
else
  version=$(curl --silent "https://api.github.com/repos/mikefarah/yq/releases/latest" | grep '"tag_name":' | sed -E 's/.*"v([^"]+)".*/\1/')
fi

# yq publishes its checksums and a script to extract them
curl --silent --location "https://github.com/mikefarah/yq/releases/download/v$version/extract-checksum.sh" -o /tmp/extract-checksum.sh
curl --silent --location "https://github.com/mikefarah/yq/releases/download/v$version/checksums_hashes_order" -o /tmp/checksums_hashes_order
curl --silent --location "https://github.com/mikefarah/yq/releases/download/v$version/checksums" -o /tmp/checksums

cd /tmp
chmod u+x extract-checksum.sh

# Extract the checksums and output a starlark map entry
echo "\"$version\": {"
platforms=(darwin_{amd64,arm64} linux_{amd64,arm64,s390x,riscv64,ppc64le} windows_{amd64,arm64})
for release in ${platforms[@]}; do
  artifact=$release
  if [[ $release == windows* ]]; then
    artifact="$release.exe"
  fi
  echo "    \"$release\": \"$(./extract-checksum.sh SHA-384 $artifact | awk '{ print $2 }' | xxd -r -p | base64 | awk '{ print "sha384-" $1 }')\","
done
echo "},"

printf "\n"
echo "Paste the above into VERSIONS in yq_toolchain.bzl."
