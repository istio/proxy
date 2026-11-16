#!/bin/bash
# Checks a `compressed_tar` tarball: its compression (magic), its layout, modes, owners and mtime
# (as bsdtar lists them), and its contents.
set -euo pipefail

: "${TAR:?TAR must be set}"
: "${TARBALL:?TARBALL must be set}"
: "${MAGIC:?MAGIC must be set}"
: "${HELLO:?HELLO must be set}"
: "${PLAIN:?PLAIN must be set}"
: "${TOOL:?TOOL must be set}"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

magic=$(od -An -t x1 -N 4 "$TARBALL" | tr -d ' \n')
[[ "$magic" == "${MAGIC}"* ]] || fail "unexpected magic ${magic} (expected ${MAGIC})"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$TAR" -tvf "$TARBALL" >"$tmp/list"
cat "$tmp/list"

expect_entry() {
    local mode=$1 path=$2
    # As `pkg_tar` names them: without a `./` prefix (so eg `tar -xf <tarball> plain.txt` works).
    grep -Eq -- "^${mode} +[0-9]+ +root +root .* 2000 ${path}/?\$" "$tmp/list" \
        || fail "expected ${mode} root:root 2000 ${path}"
}

expect_entry 'drwxr-xr-x' 'usr'
expect_entry 'drwxr-xr-x' 'usr/local'
expect_entry 'drwxr-xr-x' 'usr/local/bin'
expect_entry 'drwxr-xr-x' 'usr/share'
expect_entry 'drwxr-xr-x' 'usr/share/demo'
expect_entry '-rw-r--r--' 'plain\.txt'
expect_entry '-rwxr-xr-x' 'usr/local/bin/tool'
expect_entry '-rw-r--r--' 'usr/share/demo/hello\.txt'
# Nothing else, eg nested under a directory listed before it.
[[ $(wc -l <"$tmp/list") -eq 8 ]] || fail "expected 8 entries"

mkdir "$tmp/out"
"$TAR" -xf "$TARBALL" -C "$tmp/out"
cmp "$tmp/out/usr/share/demo/hello.txt" "$HELLO" || fail "hello.txt differs"
cmp "$tmp/out/plain.txt" "$PLAIN" || fail "plain.txt differs"
cmp "$tmp/out/usr/local/bin/tool" "$TOOL" || fail "tool differs"

# A member extracted by its name.
"$TAR" -xOf "$TARBALL" plain.txt | cmp - "$PLAIN" || fail "plain.txt is not a member by that name"

echo "PASS: ${TARBALL}"
