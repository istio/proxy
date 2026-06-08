"""Function for preserving `select` entries for Cargo cfg expressions which did
not match any enabled target triple / Bazel platform.

For example we might generate:

    rust_library(
        ...
        deps = [
            "//common:unconditional_dep",
        ] + selects.with_unmapped({
            "@rules_rust//rust/platform:x86_64-pc-windows-msvc": [
                "//third-party/rust:windows-sys",  # cfg(windows)
            ],
            "@rules_rust//rust/platform:x86_64-unknown-linux-gnu": [
                "//third-party/rust:libc",  # cfg(any(unix, target_os = "wasi"))
            ],
            "//conditions:default": [],
            selects.NO_MATCHING_PLATFORM_TRIPLES: [
                "//third-party/rust:hermit-abi",  # cfg(target_os = "hermit")
            ],
        })
    )
"""

_SENTINEL = struct()

def _with_unmapped(configurations):
    configurations.pop(_SENTINEL)
    return select(configurations)

selects = struct(
    with_unmapped = _with_unmapped,
    NO_MATCHING_PLATFORM_TRIPLES = _SENTINEL,
)
