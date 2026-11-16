#!/bin/bash
# Checks a pigz binary: compresses data with all threads and without the name and time in the
# header (`-n`, for reproducible output), that the output is gzip and the same for the same
# input, and decompresses it back.
set -euo pipefail

pigz="${1:?usage: assert_pigz_roundtrip.sh <pigz>}"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Enough data for several 128K blocks, so the compression is split across threads.
for i in $(seq 1 20000); do
    echo "line ${i} of the pigz roundtrip check"
done >"$tmp/input"

"$pigz" -n -p 4 <"$tmp/input" >"$tmp/first.gz" || fail "compression failed"
"$pigz" -n -p 4 <"$tmp/input" >"$tmp/second.gz" || fail "second compression failed"

magic=$(od -An -t x1 -N 2 "$tmp/first.gz" | tr -d ' \n')
[[ "$magic" == "1f8b" ]] || fail "output is not gzip (magic: ${magic})"
cmp -s "$tmp/first.gz" "$tmp/second.gz" || fail "output differs for the same input"

"$pigz" -d <"$tmp/first.gz" >"$tmp/output" || fail "decompression failed"
cmp -s "$tmp/input" "$tmp/output" || fail "roundtrip output differs from the input"

echo "PASS: pigz roundtrip"
