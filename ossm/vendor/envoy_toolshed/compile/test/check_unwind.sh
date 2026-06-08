#!/bin/bash
# Checks that a cross-compiled binary has (or does not have) libunwind statically linked.
#
# Required environment variables:
#   BINARY          - path to the compiled binary
#   EXPECTED_ARCH   - expected ELF architecture (e.g. aarch64, x86-64)
#   EXPECT_LIBUNWIND - "true" if libunwind.a should be statically linked, "false" otherwise
#   LLVM_NM         - path to llvm-nm binary
#   LLVM_READELF    - path to llvm-readelf binary
set -euo pipefail

: "${BINARY:?BINARY must be set to the path of the compiled binary}"
: "${EXPECTED_ARCH:?EXPECTED_ARCH must be set to the expected architecture (e.g. aarch64, x86-64)}"
: "${EXPECT_LIBUNWIND:?EXPECT_LIBUNWIND must be set to 'true' or 'false'}"
: "${LLVM_NM:?LLVM_NM must be set to the path of llvm-nm}"
: "${LLVM_READELF:?LLVM_READELF must be set to the path of llvm-readelf}"

# Check architecture using llvm-readelf -h
READELF_OUTPUT="$("${LLVM_READELF}" -h "${BINARY}")"
echo "llvm-readelf -h output:"
echo "${READELF_OUTPUT}"

if echo "${READELF_OUTPUT}" | grep -q "${EXPECTED_ARCH}"; then
    echo "PASS: binary is ${EXPECTED_ARCH}"
else
    echo "FAIL: expected ${EXPECTED_ARCH} in readelf output"
    exit 1
fi

# Check for _Unwind_RaiseException defined (T/t) symbol via llvm-nm
NM_OUTPUT="$("${LLVM_NM}" "${BINARY}")"
echo "llvm-nm output (filtered):"
echo "${NM_OUTPUT}" | grep -i "_Unwind_RaiseException" || echo "(no _Unwind_RaiseException symbols found)"

UNWIND_DEFINED="false"
if grep -qE '[Tt] _Unwind_RaiseException' <<< "${NM_OUTPUT}"; then
    UNWIND_DEFINED="true"
fi

# Check for libgcc_s in dynamic NEEDED entries via llvm-readelf -d
DYNAMIC_OUTPUT="$("${LLVM_READELF}" -d "${BINARY}")"
echo "llvm-readelf -d output:"
echo "${DYNAMIC_OUTPUT}"

HAS_LIBGCC_S="false"
if echo "${DYNAMIC_OUTPUT}" | grep -q "libgcc_s"; then
    HAS_LIBGCC_S="true"
fi

if [[ "${EXPECT_LIBUNWIND}" = "true" ]]; then
    if [ "${UNWIND_DEFINED}" = "true" ]; then
        echo "PASS: _Unwind_RaiseException is defined (statically linked from libunwind.a)"
    else
        echo "FAIL: expected _Unwind_RaiseException to be a defined (T/t) symbol, but it was not found"
        exit 1
    fi
    if [[ "${HAS_LIBGCC_S}" = "false" ]]; then
        echo "PASS: no libgcc_s in dynamic NEEDED entries"
    else
        echo "FAIL: found libgcc_s in dynamic NEEDED entries (binary is using libgcc fallback unwinder)"
        exit 1
    fi
else
    if [[ "${UNWIND_DEFINED}" = "false" ]]; then
        echo "PASS: _Unwind_RaiseException is not a defined (T/t) symbol (libunwind.a not statically linked)"
    else
        echo "FAIL: expected _Unwind_RaiseException to NOT be a defined (T/t) symbol, but it was found"
        exit 1
    fi
fi
