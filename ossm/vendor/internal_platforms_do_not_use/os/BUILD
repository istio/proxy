# Standard constraint_setting and constraint_values to be used in platforms.
licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

# To add a new constraint_value see https://github.com/bazelbuild/platforms.
constraint_setting(name = "os")

constraint_value(
    name = "freebsd",
    constraint_setting = ":os",
)

constraint_value(
    name = "netbsd",
    constraint_setting = ":os",
)

constraint_value(
    name = "openbsd",
    constraint_setting = ":os",
)

constraint_value(
    name = "haiku",
    constraint_setting = ":os",
)

constraint_value(
    name = "android",
    constraint_setting = ":os",
)

constraint_value(
    name = "linux",
    constraint_setting = ":os",
)

constraint_value(
    name = "windows",
    constraint_setting = ":os",
)

# For the VXworks OS, usefull for embedded systems
constraint_value(
    name = "vxworks",
    constraint_setting = ":os",
)

# For platforms with no OS, like microcontrollers.
constraint_value(
    name = "none",
    constraint_setting = ":os",
)

### Apple OS Values

constraint_value(
    name = "ios",
    constraint_setting = ":os",
)

# TODO(b/138656886): Rename this to macos instead of osx.
constraint_value(
    name = "osx",
    constraint_setting = ":os",
)

alias(
    name = "macos",
    actual = ":osx",
)

constraint_value(
    name = "tvos",
    constraint_setting = ":os",
)

constraint_value(
    name = "watchos",
    constraint_setting = ":os",
)

constraint_value(
    name = "visionos",
    constraint_setting = ":os",
)

constraint_value(
    name = "qnx",
    constraint_setting = ":os",
)

# NixOS uses the Linux kernel, but is not ABI-compatible with any
# other Linux distribution. This is because the dynamic linker is in
# a non-standard (and undefined) location on the filesystem.
constraint_value(
    name = "nixos",
    constraint_setting = ":os",
)

constraint_value(
    name = "emscripten",
    constraint_setting = ":os",
)

# WASI (WebAssembly System Interface)
# https://github.com/bytecodealliance/wasmtime/blob/main/docs/WASI-overview.md
constraint_value(
    name = "wasi",
    constraint_setting = ":os",
)

constraint_value(
    name = "fuchsia",
    constraint_setting = ":os",
)

constraint_value(
    name = "chromiumos",
    constraint_setting = ":os",
)
