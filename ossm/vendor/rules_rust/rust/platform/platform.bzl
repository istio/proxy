"""Definitions for support config settings and platform definitions"""

load("@bazel_skylib//lib:selects.bzl", "selects")
load(
    ":triple_mappings.bzl",
    "ALL_PLATFORM_TRIPLES",
    "cpu_arch_to_constraints",
    "system_to_constraints",
    "triple_to_constraint_set",
)

_SUPPORTED_CPU_ARCH = [
    "aarch64",
    "arm",
    "armv7",
    "i686",
    "powerpc",
    "s390x",
    "x86_64",
    "riscv32",
    "riscv64",
]

_SUPPORTED_SYSTEMS = [
    "android",
    "darwin",
    "freebsd",
    "ios",
    "linux",
    "windows",
    "nto",
]

# buildifier: disable=unnamed-macro
def declare_config_settings():
    """Helper function for declaring `config_setting`s"""
    for cpu_arch in _SUPPORTED_CPU_ARCH:
        native.config_setting(
            name = cpu_arch,
            constraint_values = cpu_arch_to_constraints(cpu_arch),
        )

    for system in _SUPPORTED_SYSTEMS:
        native.config_setting(
            name = system,
            constraint_values = system_to_constraints(system),
        )

    # Add alias for OSX to "darwin" to match what users will be expecting.
    native.alias(
        name = "osx",
        actual = ":darwin",
    )

    # Add alias for OSX to "macos" to be consistent with the long-term
    # direction of `@platforms` in using the OS's modern name.
    native.alias(
        name = "macos",
        actual = ":darwin",
    )

    all_supported_triples = ALL_PLATFORM_TRIPLES
    for triple in all_supported_triples:
        native.config_setting(
            name = triple,
            constraint_values = triple_to_constraint_set(triple),
        )

    # Add alias for wasm to maintain backwards compatibility.
    native.alias(
        name = "wasm32-wasi",
        actual = ":wasm32-wasip1",
        deprecation = "Use `@rules_rust//rust/platform:wasm-wasip1` instead.",
    )

    native.platform(
        name = "wasm32",
        constraint_values = [
            "@platforms//cpu:wasm32",
            str(Label("//rust/platform/os:unknown")),
        ],
    )

    # Add alias for wasm to maintain backwards compatibility.
    native.alias(
        name = "wasm",
        actual = ":wasm32",
        deprecation = "Use `@rules_rust//rust/platform:wasm32` instead",
    )

    native.platform(
        name = "wasm64",
        constraint_values = [
            "@platforms//cpu:wasm64",
            str(Label("//rust/platform/os:unknown")),
        ],
    )

    native.platform(
        name = "wasip1",
        constraint_values = [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
        ],
    )

    # Add alias for wasi to maintain backwards compatibility.
    native.alias(
        name = "wasi",
        actual = ":wasip1",
        deprecation = "Use `@rules_rust//rust/platform:wasip1` instead",
    )

    selects.config_setting_group(
        name = "unix",
        match_any = [
            ":android",
            ":darwin",
            ":freebsd",
            ":linux",
            ":nto",
        ],
    )

    native.alias(
        name = "aarch64-fuchsia",
        actual = "aarch64-unknown-fuchsia",
        deprecation = "Use `@rules_rust//rust/platform:aarch64-unknown-fuchsia` instead.",
    )

    native.alias(
        name = "x86_64-fuchsia",
        actual = "x86_64-unknown-fuchsia",
        deprecation = "Use `@rules_rust//rust/platform:x86_64-unknown-fuchsia` instead.",
    )
