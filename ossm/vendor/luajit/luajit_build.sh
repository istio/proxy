#!/bin/bash

set -e

PREFIX=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix=*)
            PREFIX="${1#*=}"
            shift
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

# Copy source tree to a build directory
SRC_DIR="$(dirname "$(realpath "$0")")"
BUILD_DIR="$(basename "$SRC_DIR")_build"
cp -r "$SRC_DIR" "$BUILD_DIR"
cd "$BUILD_DIR"

# Set environment variables
export MACOSX_DEPLOYMENT_TARGET="10.8"
export DEFAULT_CC="${CC:-}"
export TARGET_CFLAGS="${CFLAGS:-} -fno-function-sections -fno-data-sections"
EXTRA_MAKE_ARGS=()
FUSE_LD_FLAG=$(echo "$LDFLAGS" | grep -o -- '-fuse-ld=[^ ]*' | tail -1 || :)
SYSROOT_FLAG=$(echo "$LDFLAGS" | grep -o -- '--sysroot=[^ ]*' || :)
TARGET_TRIPLE=$(echo "$LDFLAGS" | grep -o -- '--target=[^ ]*' | sed 's/--target=//' | tail -1 || :)
if [[ "$(uname -s)" == "Linux" ]]; then
    SAN_LDFLAGS="${FUSE_LD_FLAG} ${SYSROOT_FLAG}"
    EXTRA_MAKE_ARGS+=("TARGET_AR=${AR} ${ARFLAGS:-rcus}")
    if [[ -n "${TARGET_TRIPLE}" && "${TARGET_TRIPLE}" != *"$(uname -m)"* ]]; then
        # Cross-compiling: use native gcc for host tools so they run on the build machine.
        EXTRA_MAKE_ARGS+=("HOSTCC=gcc")
    elif [[ -n "${FUSE_LD_FLAG}" ]]; then
        # Native hermetic build: system linker may not exist; pass linker flags for host tools.
        EXTRA_MAKE_ARGS+=("HOST_LDFLAGS=${FUSE_LD_FLAG} ${SYSROOT_FLAG}")
    fi
    export TARGET_LDFLAGS="${LDFLAGS:-} -fno-function-sections -fno-data-sections"
    export CFLAGS=""
    # Clear LDFLAGS so cross-compilation flags (--target, --sysroot) are not applied
    # when linking host build tools (e.g. minilua) via make's implicit rules.
    export LDFLAGS=""
else
    # macOS: extract sysroot from CFLAGS and pass to HOST_CFLAGS/HOST_LDFLAGS for building host tools
    SYSROOT_FLAG=$(echo "$CFLAGS" | grep -o -- '--sysroot=[^ ]*' || :)
    if [[ -n "${SYSROOT_FLAG}" ]]; then
        export HOST_CFLAGS="${SYSROOT_FLAG}"
        export HOST_LDFLAGS="${SYSROOT_FLAG}"
        EXTRA_MAKE_ARGS+=("HOST_CFLAGS=${HOST_CFLAGS}" "HOST_LDFLAGS=${HOST_LDFLAGS}")
    fi
    export TARGET_LDFLAGS="${CFLAGS:-} -fno-function-sections -fno-data-sections"
    export CFLAGS=""
    export LDFLAGS=""
fi

# Don't strip the binary - it doesn't work when cross-compiling
export TARGET_STRIP="@echo"

# Remove LuaJIT from ASAN/MSAN for now
# TODO(htuch): Remove this when https://github.com/envoyproxy/envoy/issues/6084 is resolved
if [[ -n "${ENVOY_CONFIG_ASAN}" ]] || [[ -n "${ENVOY_CONFIG_MSAN}" ]]; then
    export LDFLAGS="$SAN_LDFLAGS"
    BLOCK_PATH=$(realpath clang-asan-blocklist.txt)
    export TARGET_CFLAGS="${TARGET_CFLAGS} -fsanitize-blacklist=${BLOCK_PATH}"
    echo "fun:*" > clang-asan-blocklist.txt
fi

# Run make with all available cores
"${MAKE:-make}" -j$(nproc) V=1 PREFIX="$PREFIX" \
  "${EXTRA_MAKE_ARGS[@]}" \
  install
