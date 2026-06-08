"""Tests to verify toolchain resolution of different config variants.

NOTE: This test relies on the project toolchain configuration. This is
intentional because it wants to verify that, using the toolchains as
rules_python configures them, the different implementations can be used
by setting the appropriate flags.
"""

load("@bazel_skylib//lib:structs.bzl", "structs")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python:versions.bzl", "TOOL_VERSIONS")
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private:toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility
load("//python/private:version.bzl", "version")  # buildifier: disable=bzl-visibility
load("//tests/support/platforms:platforms.bzl", "platform_targets")

_PLATFORM_TARGET_MAP = {
    "linux": {
        "aarch64": platform_targets.LINUX_AARCH64,
        "x86_64": platform_targets.LINUX_X86_64,
    },
    "osx": {
        "aarch64": platform_targets.MAC_AARCH64,
        "x86_64": platform_targets.MAC_X86_64,
    },
    "windows": {
        "aarch64": platform_targets.WINDOWS_AARCH64,
        "x86_64": platform_targets.WINDOWS_X86_64,
    },
}
_PLATFORM_TRIPLES = {
    ("linux", "glibc"): "unknown_linux_gnu",
    ("linux", "musl"): "unknown_linux_musl",
    "osx": "apple_darwin",
    "windows": "pc_windows_msvc",
}

_ResolvedToolchainsInfo = provider(
    doc = "Tell what toolchain was found",
    fields = {
        "target": "ToolchainInfo for //python:toolchain_type",
    },
)

def _current_toolchain_impl(ctx):
    # todo: also return current settings for various config flags
    # to help identify state
    return [_ResolvedToolchainsInfo(
        target = ctx.toolchains[TARGET_TOOLCHAIN_TYPE],
    )]

_current_toolchain = rule(
    implementation = _current_toolchain_impl,
    toolchains = [
        TARGET_TOOLCHAIN_TYPE,
    ],
)

def _platform(os, arch, libc, *, ft):
    if os == "linux":
        platform_triple = _PLATFORM_TRIPLES[os, libc]
    else:
        platform_triple = _PLATFORM_TRIPLES[os]
    return struct(
        arch = arch,
        freethreaded = ft,
        libc = libc,
        os = os,
        platform_triple = platform_triple,
        platform_target = _PLATFORM_TARGET_MAP[os][arch],
    )

# There's many exceptions to the full `os x arch x libc x threading` matrix,
# so just list the specific combinations that are supported.
# We also omit some more esoteric archs to reduce the matrix size (and
# thus how many runtimes get downloaded).
# The important quality is to have at least 2 for every dimension
_PLATFORMS = [
    _platform("linux", "aarch64", "glibc", ft = "no"),
    _platform("linux", "aarch64", "glibc", ft = "yes"),
    _platform("linux", "x86_64", "glibc", ft = "no"),
    _platform("linux", "x86_64", "glibc", ft = "yes"),
    _platform("linux", "x86_64", "musl", ft = "no"),
    _platform("osx", "aarch64", None, ft = "no"),
    _platform("osx", "aarch64", None, ft = "yes"),
    _platform("osx", "x86_64", None, ft = "no"),
    _platform("osx", "x86_64", None, ft = "yes"),
    _platform("windows", "aarch64", None, ft = "no"),
    _platform("windows", "aarch64", None, ft = "yes"),
    _platform("windows", "x86_64", None, ft = "no"),
    _platform("windows", "x86_64", None, ft = "yes"),
]

def _compute_runtimes():
    runtimes = []

    # Limit to the two most recent versions. This helps ensure that multiple
    # versions are matching correctly. Limit to two because the disk/download
    # isn't worth the marginal coverage improvment.
    selected_versions = sorted(
        TOOL_VERSIONS.keys(),
        key = lambda v: version.parse(v).key(),
    )[-2:]

    for python_version in selected_versions:
        for platform in _PLATFORMS:
            runtimes.append(struct(
                name = "{python_version}_{arch}_{triple}{threading}".format(
                    python_version = python_version.replace(".", "_"),
                    arch = platform.arch,
                    triple = platform.platform_triple,
                    threading = "_freethreaded" if platform.freethreaded == "yes" else "",
                ),
                python_version = python_version,
                **structs.to_dict(platform)
            ))

    return sorted(runtimes, key = lambda v: v.name)

def _test_toolchains_impl(env, target):
    target_tc = target[_ResolvedToolchainsInfo].target
    toolchain_str = str(target_tc.toolchain_label).replace("-", "_")
    env.expect.that_str(toolchain_str).contains(env.ctx.attr.expected_toolchain_name)

def _test_toolchains(name):
    _current_toolchain(
        name = name + "_current_toolchain",
    )
    test_names = []
    for runtime in _compute_runtimes():
        test_name = "test_{}".format(runtime.name)
        test_names.append(test_name)
        config_settings = {
            "//command_line_option:platforms": [runtime.platform_target],
            labels.VISIBLE_FOR_TESTING: True,
            labels.PY_FREETHREADED: runtime.freethreaded,
            labels.PYTHON_VERSION: runtime.python_version,
        }
        if runtime.libc:
            config_settings[labels.PY_LINUX_LIBC] = runtime.libc

        analysis_test(
            name = test_name,
            target = name + "_current_toolchain",
            impl = _test_toolchains_impl,
            config_settings = config_settings,
            attrs = {"expected_toolchain_name": attr.string()},
            attr_values = {
                "expected_toolchain_name": runtime.name,
                # A lot of tests are generated, so set tags to make selecting
                # subsets easier
                "tags": [
                    "python-version={}".format(runtime.python_version),
                    "libc={}".format(runtime.libc),
                    "freethreaded={}".format(runtime.freethreaded),
                    "os={}".format(runtime.os),
                    "arch={}".format(runtime.arch),
                ],
            },
        )

    # We have to return a target for `name`.
    native.test_suite(
        name = name,
        tests = test_names,
    )

_tests = [
    _test_toolchains,
]

def resolution_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
