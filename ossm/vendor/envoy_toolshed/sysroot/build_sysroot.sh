#!/usr/bin/env bash

# Script to build a Debian sysroot for cross-compilation
# This is a non-hermetic build that uses debootstrap to create a minimal Debian system

set -e -o pipefail


retry() {
    # Retry function to handle transient network failures
    # Usage: retry <max_attempts> <delay_seconds> <command> [args...]
    local max_attempts="$1"
    local delay="$2"
    shift 2
    local attempt=1
    local exit_code=0

    while [[ "$attempt" -le "$max_attempts" ]]; do
        if "$@"; then
            return 0
        else
            exit_code=$?
            echo "Command failed with exit code $exit_code on attempt $attempt"

            if [[ "$attempt" -lt "$max_attempts" ]]; then
                echo "Waiting ${delay} seconds before retry..."
                sleep "$delay"
                attempt=$((attempt + 1))
            else
                echo "All $max_attempts attempts failed"
                return $exit_code
            fi
        fi
    done
}

DEFAULT_REMOVE_DIRS=(
    bin
    boot
    dev
    etc/alternatives
    etc/rmt
    etc/systemd
    etc/localtime
    home
    media
    opt
    proc
    root
    run
    sbin
    srv
    sys
    tmp
    usr/bin/awk
    usr/bin/nawk
    usr/bin/pager
    usr/bin/pidof
    usr/sbin
    usr/share/doc
    usr/share/info
    usr/share/lintian
    usr/share/man
    usr/share/zoneinfo
    var
)

ARCH=""
GLIBC_VERSION=""
DEBIAN_VERSION=""
VARIANT="base"
PPA_TOOLCHAIN=""
STDCC_VERSION="13"
KEEP_DIRS=""
REMOVE_DIRS=""
SKIP_CLEANUP="false"

usage () {
    echo "Usage: $0 --arch <amd64|arm64> --glibc <version> --debian <version> [options]"
    echo ""
    echo "Required Options:"
    echo "  --arch           Architecture to build (amd64 or arm64)"
    echo "  --glibc          glibc version (e.g., 2.31 or 2.28)"
    echo "  --debian         Debian version (e.g., bullseye or buster)"
    echo ""
    echo "Variant Options:"
    echo "  --variant        Sysroot variant: base or libstdcxx (default: base)"
    echo "  --ppa-toolchain  Ubuntu PPA toolchain version (required for libstdcxx variant)"
    echo "  --stdcc          libstdc++ version (default: 13)"
    echo ""
    echo "Output Options:"
    echo "  --output         Output directory name"
    echo "  --workdir        Directory to build in"
    echo ""
    echo "Cleanup Options:"
    echo "  --keep-dirs      Comma-separated list of dirs to keep (preserves them from default cleanup)"
    echo "  --remove-dirs    Comma-separated list of additional dirs to remove (added to defaults)"
    echo "  --skip-cleanup   Skip the cleanup phase entirely (keeps everything)"
    echo ""
    echo "Examples:"
    echo "  # Standard build with default cleanup"
    echo "  $0 --arch amd64 --glibc 2.31 --debian bullseye"
    echo ""
    echo "  # Keep the bin directory"
    echo "  $0 --arch amd64 --glibc 2.31 --debian bullseye --keep-dirs bin"
    echo ""
    echo "  # Remove additional directories"
    echo "  $0 --arch amd64 --glibc 2.31 --debian bullseye --remove-dirs 'usr/games,usr/local'"
    echo ""
    echo "  # Skip cleanup entirely"
    echo "  $0 --arch amd64 --glibc 2.31 --debian bullseye --skip-cleanup"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --glibc)
            GLIBC_VERSION="$2"
            shift 2
            ;;
        --debian)
            DEBIAN_VERSION="$2"
            shift 2
            ;;
        --variant)
            VARIANT="$2"
            shift 2
            ;;
        --ppa-toolchain)
            PPA_TOOLCHAIN="$2"
            shift 2
            ;;
        --stdcc)
            STDCC_VERSION="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --keep-dirs)
            KEEP_DIRS="$2"
            shift 2
            ;;
        --remove-dirs)
            REMOVE_DIRS="$2"
            shift 2
            ;;
        --skip-cleanup)
            SKIP_CLEANUP="true"
            shift 1
            ;;
        --workdir)
            WORK_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

if [[ -z "$ARCH" ]] || [[ -z "$GLIBC_VERSION" ]] || [[ -z "$DEBIAN_VERSION" ]]; then
    echo "Error: Missing required arguments"
    usage
fi

if [[ "$VARIANT" == "libstdcxx" ]] && [[ -z "$PPA_TOOLCHAIN" ]]; then
    echo "Error: --ppa-toolchain is required for libstdcxx variant"
    usage
fi

echo "Building sysroot with the following configuration:"
echo "  Architecture: $ARCH"
echo "  glibc version: $GLIBC_VERSION"
echo "  Debian version: $DEBIAN_VERSION"
echo "  Variant: $VARIANT"
echo "  Output directory: $WORK_DIR"

# Determine Debian mirror and libgcc package based on Debian version
if [[ "$DEBIAN_VERSION" == "buster" ]]; then
    DEBIAN_MIRROR="http://archive.debian.org/debian/"
    LIBGCC_PACKAGE="libgcc1"  # buster uses old package name
else
    DEBIAN_MIRROR="http://deb.debian.org/debian/"
    LIBGCC_PACKAGE="libgcc-s1"  # bullseye and newer
fi

echo "  Debian mirror: $DEBIAN_MIRROR"
echo "  libgcc package: $LIBGCC_PACKAGE"

# Detect if we're cross-compiling
HOST_ARCH=$(dpkg --print-architecture)
CROSS_COMPILE="false"
if [[ "$HOST_ARCH" != "$ARCH" ]]; then
    CROSS_COMPILE="true"
    echo "Detected cross-compilation: building $ARCH on $HOST_ARCH"
fi

create_base_sysroot () {
    echo ""
    echo "Step 1: Creating base sysroot with debootstrap..."
    echo ""
    if [[ -d "$WORK_DIR" ]]; then
        echo "Removing existing sysroot directory..."
        sudo rm -rf "$WORK_DIR"
    fi
    DEBOOTSTRAP_ARGS=()
    if [[ "$CROSS_COMPILE" == "true" ]]; then
        DEBOOTSTRAP_ARGS+=(--foreign)
    fi
    DEBOOTSTRAP_ARGS+=(
        --arch="$ARCH"
        --variant=minbase
        --keyring=/usr/share/keyrings/debian-archive-keyring.gpg
        "$DEBIAN_VERSION"
        "$WORK_DIR"
        "$DEBIAN_MIRROR"
    )
    # Retry debootstrap up to 3 times with 30 second delays
    retry 3 30 sudo debootstrap "${DEBOOTSTRAP_ARGS[@]}"
}

configure_package_sources () {
    echo ""
    echo "Step 2: Configuring package sources..."
    echo "deb [check-valid-until=no] http://archive.debian.org/debian $DEBIAN_VERSION-backports main" \
        | sudo tee "$WORK_DIR/etc/apt/sources.list.d/backports.list" > /dev/null
}

install_base_packages () {
    echo ""
    echo "Step 3: Installing base packages..."
    retry 3 10 sudo chroot "$WORK_DIR" apt-get -qq update
    retry 3 10 sudo chroot "$WORK_DIR" apt-get -qq install --no-install-recommends -y \
         libc6 libc6-dev "$LIBGCC_PACKAGE" libxml2-dev
    retry 3 10 sudo chroot "$WORK_DIR" apt-get -qq install --no-install-recommends -y \
         -t "$DEBIAN_VERSION-backports" linux-libc-dev
}

install_libstdcc () {
    if [[ "$VARIANT" != "libstdcxx" ]]; then
        return
    fi
    echo ""
    echo "Step 4: Installing libstdc++..."
    echo "deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu $PPA_TOOLCHAIN main" \
        | sudo tee "$WORK_DIR/etc/apt/sources.list.d/toolchain.list" > /dev/null
    retry 3 10 sudo apt-key --keyring "$WORK_DIR/etc/apt/trusted.gpg" adv \
        --keyserver keyserver.ubuntu.com --recv-keys 1E9377A2BA9EF27F
    retry 3 10 sudo chroot "$WORK_DIR" apt-get -qq update
    retry 3 10 sudo chroot "$WORK_DIR" apt-get -qq install -y "libstdc++-${STDCC_VERSION}-dev"
}

cleanup_sysroot () {
    echo ""
    echo "Step 5: Cleaning up sysroot..."
    if [[ "$CROSS_COMPILE" == "false" ]]; then
        sudo chroot "$WORK_DIR" apt-get clean
    fi

    if [[ "$SKIP_CLEANUP" == "true" ]]; then
        echo "Skipping cleanup (--skip-cleanup specified)"
    else
        # Build the list of directories to remove
        DIRS_TO_REMOVE=("${DEFAULT_REMOVE_DIRS[@]}")
        # Add additional directories if specified
        if [[ -n "$REMOVE_DIRS" ]]; then
            IFS=',' read -ra EXTRA_DIRS <<< "$REMOVE_DIRS"
            DIRS_TO_REMOVE+=("${EXTRA_DIRS[@]}")
            echo "Adding extra directories to remove: ${EXTRA_DIRS[*]}"
        fi
        # Build list of directories to keep
        declare -A KEEP_SET
        if [[ -n "$KEEP_DIRS" ]]; then
            IFS=',' read -ra KEEP_ARRAY <<< "$KEEP_DIRS"
            for dir in "${KEEP_ARRAY[@]}"; do
                KEEP_SET["$dir"]=1
            done
            echo "Preserving directories: ${KEEP_ARRAY[*]}"
        fi
        # Remove directories that are not in the keep list
        for dir in "${DIRS_TO_REMOVE[@]}"; do
            if [[ -n "${KEEP_SET[$dir]}" ]]; then
                echo "Keeping: $dir (in keep list)"
                continue
            fi
            if [[ -e "$WORK_DIR/$dir" ]]; then
                echo "Removing: $dir"
                sudo rm -rf "$WORK_DIR/$dir"
            fi
        done
        # Clean up apt sources (unless etc is in keep list)
        if [[ -z "${KEEP_SET[etc]}" ]]; then
            sudo rm -rf "$WORK_DIR/etc/apt/sources.list.d/"*
        fi
    fi
}

fix_absolute_symlinks () {
    echo ""
    echo "Step 6: Fixing absolute symlinks..."
    find "$WORK_DIR" -type l | while read -r symlink; do
        # Get the current target
        current_target=$(readlink "$symlink")

        # Skip if already relative
        if [[ "$current_target" != /* ]]; then
            continue
        fi

        # If target exists within our sysroot, make it relative
        if [[ -e "$WORK_DIR$current_target" ]]; then
            link_dir=$(dirname "$symlink")
            relative_path=$(realpath --relative-to="$link_dir" "$WORK_DIR$current_target")
            sudo ln -sf "$relative_path" "$symlink"
            echo "Fixed: $symlink -> $relative_path (was: $current_target)"
        else
            echo "Skipping - target outside sysroot: $symlink -> $current_target"
        fi
    done
}

package_sysroot () {
    echo ""
    echo "Step 7: Packaging sysroot..."
    OUTPUT_FILE="$(realpath "${OUTPUT}")"
    sudo tar -cJf "$OUTPUT_FILE" -C "$WORK_DIR" .
    echo ""
    echo "Successfully built sysroot: $OUTPUT_FILE"

    USER_ID="$(id -u)"
    GROUP_ID="$(id -g)"
    OUTPUT_DIR="$(dirname "${OUTPUT_FILE}")"
    sudo chown -R "${USER_ID}:${GROUP_ID}" "$OUTPUT_DIR"
}

build_sysroot () {
    create_base_sysroot
    configure_package_sources
    install_base_packages
    install_libstdcc
    cleanup_sysroot
    fix_absolute_symlinks
    package_sysroot
}

build_sysroot
