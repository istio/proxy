#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail

NAME="$1"
NAME_UPPER="$(echo $NAME | tr '[a-z]' '[A-Z]')"
shift

cat <<EOF
${NAME_UPPER}_INTEGRITY = {
EOF

while (($# > 0)); do

  if [[ "$1" =~ .*.sha256 ]]; then
    base=$(basename $1)
    base="${base/"$NAME-"/}"
    base="${base/".sha256"/}"
    base="${base/".exe"/}"
    cat <<EOF
    "${base}": "sha256-$(cat $1 | awk '{ print $1 }' | xxd -r -p | base64)",
EOF
  fi
  shift
done

echo -n "}"
echo ""
