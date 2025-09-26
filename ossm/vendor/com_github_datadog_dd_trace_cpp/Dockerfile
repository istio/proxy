# This is the image used to build and test the library in CircleCI.
# See .circleci/ for more information.

from ubuntu:22.04

# Expose Docker's predefined platform ARGs
# For more information: <https://docs.docker.com/engine/reference/builder/#automatic-platform-args-in-the-global-scope>
arg TARGETARCH

# Don't issue blocking prompts during installation (sometimes an installer
# prompts for the current time zone).
env DEBIAN_FRONTEND=noninteractive

# - Make available more recent versions of git.
# - Update the package lists and upgrade already-installed software.
# - Install build tooling:
#   - GCC, clang, libc++, make, git, debugger, formatter, and miscellanea
run apt-get update && apt-get install --yes software-properties-common && \
    add-apt-repository ppa:git-core/ppa --yes && \
    apt-get update && apt-get upgrade --yes && \
    apt-get install --yes \
        wget build-essential clang sed gdb clang-format git ssh shellcheck \
        libc++-dev libc++abi-dev python3 pip coreutils curl gnupg

# bazelisk, a launcher for bazel. `bazelisk --help` will cause the latest
# version to be downloaded.
run wget -O/usr/local/bin/bazelisk https://github.com/bazelbuild/bazelisk/releases/download/v1.15.0/bazelisk-linux-$TARGETARCH \
    && chmod +x /usr/local/bin/bazelisk \
    && bazelisk --help

# CMake, by downloading a recent release from their website.
copy bin/install-cmake /tmp/install-cmake
run chmod +x /tmp/install-cmake && /tmp/install-cmake && rm /tmp/install-cmake

# Coverage reporting.
copy bin/install-lcov /tmp/install-lcov
run chmod +x /tmp/install-lcov && /tmp/install-lcov && rm /tmp/install-lcov

