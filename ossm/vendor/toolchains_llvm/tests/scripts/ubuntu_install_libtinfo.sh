#!/usr/bin/env bash

# Ubuntu 24.04 does not have libtinfo5 in its PPAs:
#
# However, the LLVM binary releases hosted up upstream still target Ubuntu 18.04
# as of this writing and contain binaries linked against `libtinfo5`.
#
# This script installs `libtinfo5` using the `.deb` from Ubuntu 22.04's PPAs:
# https://packages.ubuntu.com/jammy-updates/amd64/libtinfo5/download

set -euo pipefail

pkg="$(mktemp --suffix=.deb)"
trap 'rm -f "${pkg}"' EXIT

curl -L https://mirrors.kernel.org/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb -o "${pkg}"
sudo dpkg -i "${pkg}"
