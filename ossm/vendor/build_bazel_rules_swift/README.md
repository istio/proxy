# Swift Rules for [Bazel](https://bazel.build)

[![Build status](https://badge.buildkite.com/d562b11425e192a8f6ba9c43715bc8364985bccf54e4b9194a.svg?branch=master)](https://buildkite.com/bazel/rules-swift-swift)

This repository contains rules for [Bazel](https://bazel.build) that can be
used to build Swift libraries, tests, and executables for macOS and Linux.

To build applications for all of Apple's platforms (macOS, iOS, tvOS,
visionOS, and watchOS), they can be combined with the
[Apple Rules](https://github.com/bazelbuild/rules_apple).

If you run into any problems with these rules, please
[file an issue!](https://github.com/bazelbuild/rules_swift/issues/new)

## Basic Examples

Create a simple CLI that can run on macOS, Linux, or Windows:

```bzl
load("@build_bazel_rules_swift//swift:swift_binary.bzl", "swift_binary")

swift_binary(
    name = "cli",
    srcs = ["CLI.swift"],
)
```

Create a single library target that can be used by other targets in your
build:

```bzl
load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")

swift_library(
    name = "MyLibrary",
    srcs = ["MyLibrary.swift"],
    tags = ["manual"],
)
```

## Reference Documentation

[Click here](https://github.com/bazelbuild/rules_swift/tree/master/doc)
for the reference documentation for the rules and other definitions in this
repository.

## Quick Setup

### 1. Install Swift

Before getting started, make sure that you have a Swift toolchain installed.

**Apple users:** Install [Xcode](https://developer.apple.com/xcode/downloads/).
If this is your first time installing it, make sure to open it once after
installing so that the command line tools are correctly configured.

**Linux users:** Follow the instructions on the
[Swift download page](https://swift.org/download/) to download and install the
appropriate Swift toolchain for your platform. Take care to ensure that you have
all of Swift's dependencies installed (such as ICU, Clang, and so forth), and
also ensure that the Swift compiler is available on your system path.

### 2. Configure your workspace

Copy the `WORKSPACE` snippet from [the releases
page](https://github.com/bazelbuild/rules_swift/releases).

### 3. Additional configuration (Linux only)

The `swift_binary` and `swift_test` rules expect to use `clang` as the driver
for linking, and they query the Bazel C++ API and CROSSTOOL to determine which
arguments should be passed to the linker. By default, the C++ toolchain used by
Bazel is `gcc`, so Swift users on Linux need to override this by setting the
environment variable `CC=clang` when invoking Bazel.

This step is not necessary for macOS users because the Xcode toolchain always
uses `clang`.

## Building with Custom Toolchains

**macOS hosts:** You can build with a custom Swift toolchain (downloaded
from https://swift.org/download) instead of Xcode's default. To do so,
pass the following flag to Bazel:

```lang-none
--action_env=TOOLCHAINS=toolchain.id
```

Where `toolchain.id` is the value of the `CFBundleIdentifier` key in the
toolchain's Info.plist file.

To list the available toolchains and their bundle identifiers, you can run:

```command
bazel run @build_bazel_rules_swift//tools/dump_toolchains
```

**Linux hosts:** At this time, Bazel uses whichever `swift` executable is
encountered first on your `PATH`.

## Supporting debugging

To make cacheable builds work correctly with debugging see
[this doc](doc/debuggable_remote_swift.md).

## Swift Package Manager Support

To download, build, and reference external Swift packages as Bazel
targets, check out
[rules_swift_package_manager](https://github.com/cgrindel/rules_swift_package_manager).

## Supported bazel versions

rules_apple and rules_swift are often affected by changes in bazel
itself. This means you generally need to update these rules as you
update bazel.

You can also see the supported bazel versions in the notes for each
release on the [releases
page](https://github.com/bazelbuild/rules_swift/releases).

Besides these constraint this repo follows [semver](https://semver.org/)
as best as we can since the 1.0.0 release.

| Bazel release | Minimum supported rules version | Final supported rules version|
|:-------------------:|:-------------------:|:-------------------------:|
| 8.x (most recent rolling) | 0.27.0 | current |
| 7.x | 0.27.0 | current |
| 6.x | 0.27.0 | current |
| 5.x | 0.25.0 | 1.14.0 |
| 4.x | 0.19.0 | 0.24.0 |
| 3.x | 0.14.0 | 0.18.0 |
