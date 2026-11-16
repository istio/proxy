#!/bin/bash
# Checks a published pigz tarball: layout, modes, stripping, ELF arch, runtime deps and the glibc
# floor. Where the tarball matches the host arch, also runs the roundtrip check against the
# extracted binary.
set -euo pipefail

: "${TAR:?TAR must be set}"
: "${TARBALL:?TARBALL must be set}"
: "${READELF:?READELF must be set}"
: "${ROUNDTRIP_CHECK:?ROUNDTRIP_CHECK must be set}"
: "${GLIBC_FLOOR:?GLIBC_FLOOR must be set}"
: "${PIGZ_VERSION:?PIGZ_VERSION must be set}"
: "${ARCH:?ARCH must be set}"

TAR="$(realpath "$TAR")"
READELF="$(realpath "$READELF")"
ROUNDTRIP_CHECK="$(realpath "$ROUNDTRIP_CHECK")"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

list_file="$tmp/tar.list"
"$TAR" -tvf "$TARBALL" >"$list_file"
"$TAR" -xf "$TARBALL" -C "$tmp"

prefix="pigz-${PIGZ_VERSION}-linux-${ARCH}"
pkg_dir="$tmp/$prefix"
test -d "$pkg_dir" || fail "missing top-level $prefix/"

expect_entry() {
    local mode=$1 path=$2
    grep -Eq -- "^${mode} .* (\./)?${prefix//./\\.}/${path}\$" "$list_file" \
        || fail "expected ${mode} ${prefix}/${path}"
}

expect_entry '-rw-r--r--' 'BUILD\.bazel'
expect_entry '-rwxr-xr-x' 'bin/pigz'
for license in \
    pigz/LICENSE \
    zlib-ng/LICENSE.md \
    zopfli/COPYING
do
    expect_entry '-rw-r--r--' "share/doc/${license}"
done

pigz="$pkg_dir/bin/pigz"

case "$ARCH" in
    x86_64) machine="X86-64" ;;
    aarch64) machine="AArch64" ;;
    *) fail "unknown ARCH: $ARCH" ;;
esac
# Capture readelf output before matching: `grep -q` exits on the first match, and under
# `pipefail` the resulting SIGPIPE to readelf would fail the pipeline.
header=$("$READELF" -h "$pigz")
[[ "$header" =~ Machine:[^$'\n']*"${machine}" ]] || fail "bin/pigz is not ${machine}"

sections=$("$READELF" -S "$pigz")
[[ "$sections" != *".symtab"* ]] || fail "bin/pigz is not stripped"

# Only glibc at runtime: zlib-ng and zopfli are linked statically.
unexpected=$("$READELF" -d "$pigz" \
    | awk '/NEEDED/ { gsub(/\[|\]/, "", $NF); print $NF }' \
    | grep -Ev '^(libc\.so\.6|libm\.so\.6|libpthread\.so\.0|libdl\.so\.2|librt\.so\.1|ld-linux[^[:space:]]*\.so(\.[0-9]+)*|libgcc_s\.so\.1)$' || true)
[[ -z "$unexpected" ]] || fail "unexpected shared libraries: $unexpected"

max_glibc=$("$READELF" -V "$pigz" | grep -Eo 'GLIBC_[0-9]+(\.[0-9]+)+' | cut -d_ -f2 | sort -uV | tail -n 1)
if [[ -n "$max_glibc" ]]; then
    highest=$(printf '%s\n%s\n' "$max_glibc" "$GLIBC_FLOOR" | sort -V | tail -n 1)
    [[ "$highest" == "$GLIBC_FLOOR" ]] || fail "requires GLIBC_${max_glibc}, above the ${GLIBC_FLOOR} floor"
fi

if [[ "$(uname -m)" == "$ARCH" ]]; then
    "$ROUNDTRIP_CHECK" "$pigz"
else
    echo "Skipping the roundtrip check: ${ARCH} tarball on a $(uname -m) host"
fi

echo "PASS: ${prefix} package checks passed"
