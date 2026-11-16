load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//:versions.bzl", "VERSIONS")
load(":pigz_test.bzl", "host_constraints_test", "render_hub_build_test", "unavailable_test")

exports_files([
    "assert_pigz_package.sh",
    "assert_pigz_roundtrip.sh",
])

# `//pigz`, resolved through `//pigz:toolchain_type` (the prebuilt on Linux, the source fallback
# eg on macOS), as consumers use it.
sh_test(
    name = "pigz_toolchain_test",
    size = "small",
    srcs = [":assert_pigz_roundtrip.sh"],
    args = ["$(rootpath //pigz)"],
    data = ["//pigz"],
    target_compatible_with = select({
        "@platforms//os:linux": [],
        "@platforms//os:macos": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
)

# The published tarballs, before publication: layout, modes, stripping and ELF checks for both
# arches, and the roundtrip check against the extracted package on a matching host.
[
    sh_test(
        name = "pigz_linux_%s_package_test" % arch,
        size = "medium",
        srcs = [":assert_pigz_package.sh"],
        data = [
            ":assert_pigz_roundtrip.sh",
            "//compile:llvm_minimal_host_llvm_readelf",
            "//pigz/source:pigz_linux_%s" % arch,
            "@bsd_tar_toolchains//:resolved_toolchain",
        ],
        env = {
            "ARCH": arch,
            "GLIBC_FLOOR": "2.31",
            "PIGZ_VERSION": VERSIONS["pigz"],
            "READELF": "$(rootpath //compile:llvm_minimal_host_llvm_readelf)",
            "ROUNDTRIP_CHECK": "$(rootpath :assert_pigz_roundtrip.sh)",
            "TAR": "$(rootpath @bsd_tar_toolchains//:resolved_toolchain)",
            "TARBALL": "$(rootpath //pigz/source:pigz_linux_%s)" % arch,
        },
        target_compatible_with = ["@platforms//os:linux"],
    )
    for arch in [
        "aarch64",
        "x86_64",
    ]
]

render_hub_build_test(
    name = "render_hub_build_test",
)

host_constraints_test(
    name = "host_constraints_test",
)

# A platform that neither the prebuilt nor the source toolchains match.
platform(
    name = "no_pigz_platform",
    constraint_values = [
        "@platforms//cpu:x86_64",
        "@platforms//os:windows",
    ],
)

# Without a pigz for the platform, `//pigz` fails with a message, rather than building pigz.
unavailable_test(
    name = "unavailable_test",
    target_under_test = "//pigz",
)

# The source build that the fallback toolchain uses, run directly: toolchain resolution prefers
# the prebuilt where there is one. Unconstrained, so CI also builds it for macOS.
sh_test(
    name = "pigz_source_test",
    size = "small",
    srcs = [":assert_pigz_roundtrip.sh"],
    args = ["$(rootpath @pigz)"],
    data = ["@pigz"],
)
