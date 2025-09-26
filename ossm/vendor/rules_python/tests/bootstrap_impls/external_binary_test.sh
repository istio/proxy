#!/usr/bin/env bash
set -euxo pipefail

tmpdir="${TEST_TMPDIR}/external_binary"
mkdir -p "${tmpdir}"
tar xf "tests/bootstrap_impls/external_binary.tar" -C "${tmpdir}"
test -x "${tmpdir}/external_main"
output="$("${tmpdir}/external_main")"
test "$output" = "token"
