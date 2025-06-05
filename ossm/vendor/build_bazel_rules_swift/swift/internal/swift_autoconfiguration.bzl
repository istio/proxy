# Copyright 2019 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Definitions for autoconfiguring Swift toolchains.

At this time, only the Linux toolchain uses this capability. The Xcode toolchain
determines which
features are supported using Xcode version checks in xcode_toolchain.bzl.

NOTE: This file is loaded from repositories.bzl, before any workspace
dependencies have been downloaded. Therefore, only files within this repository
should be loaded here. Do not load anything else, even common libraries like
Skylib.
"""

load(
    ":feature_names.bzl",
    "SWIFT_FEATURE_CODEVIEW_DEBUG_INFO",
    "SWIFT_FEATURE_DEBUG_PREFIX_MAP",
    "SWIFT_FEATURE_DECLARE_SWIFTSOURCEINFO",
    "SWIFT_FEATURE_EMIT_SWIFTDOC",
    "SWIFT_FEATURE_LLD_GC_WORKAROUND",
    "SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS",
    "SWIFT_FEATURE_NO_EMBED_DEBUG_MODULE",
    "SWIFT_FEATURE_USE_AUTOLINK_EXTRACT",
    "SWIFT_FEATURE_USE_MODULE_WRAP",
    "SWIFT_FEATURE__SUPPORTS_UPCOMING_FEATURES",
    "SWIFT_FEATURE__SUPPORTS_V6",
)
load(":toolchain_utils.bzl", "SWIFT_TOOLCHAIN_TYPE")

def _scratch_file(repository_ctx, temp_dir, name, content = ""):
    """Creates and returns a scratch file with the given name and content.

    Args:
        repository_ctx: The repository context.
        temp_dir: The `path` to the temporary directory where the file should be
            created.
        name: The name of the scratch file.
        content: The text to write into the scratch file.

    Returns:
        The `path` to the file that was created.
    """
    path = temp_dir.get_child(name)
    repository_ctx.file(path, content)
    return path

def _swift_succeeds(repository_ctx, swiftc_path, *args):
    """Returns True if an invocation of the Swift compiler is successful.

    Args:
        repository_ctx: The repository context.
        swiftc_path: The `path` to the `swiftc` executable to spawn.
        *args: Zero or more arguments to pass to `swiftc` on the command line.

    Returns:
        True if the invocation was successful (a zero exit code); otherwise,
        False.
    """
    swift_result = repository_ctx.execute([swiftc_path] + list(args))
    return swift_result.return_code == 0

def _check_supports_language_mode_6(repository_ctx, swiftc_path, _temp_dir):
    """Returns True if the swift compiler supports language mode 6."""
    result = repository_ctx.execute([swiftc_path, "-version"])
    if result.return_code == 0:
        _, _, almost_version = result.stdout.partition("swiftlang-")
        if not almost_version:
            return False

        major_version, _, _ = almost_version.parition(".")
        if not major_version:
            return False

        return int(major_version) >= 6

    return False

def _check_supports_lld_gc_workaround(repository_ctx, swiftc_path, temp_dir):
    """Returns True if lld is being used and it supports nostart-stop-gc."""
    source_file = _scratch_file(
        repository_ctx,
        temp_dir,
        "main.swift",
        """\
print("Hello")
""",
    )
    return _swift_succeeds(
        repository_ctx,
        swiftc_path,
        source_file,
        "-use-ld=lld",
        "-Xlinker",
        "-z",
        "-Xlinker",
        "nostart-stop-gc",
    )

def _check_supports_upcoming_features(repository_ctx, swiftc_path, _temp_dir):
    """Returns True if `swiftc` supports the `-enable-{experimental,upcoming}-feature` flags."""
    return _swift_succeeds(
        repository_ctx,
        swiftc_path,
        "-version",
        "-enable-upcoming-feature",
        "BareRegexLiteralSyntax",
    )

def _write_swift_version(repository_ctx, swiftc_path):
    """Write a file containing the current Swift version info

    This is used to encode the current version of Swift as an input for caching

    Args:
        repository_ctx: The repository context.
        swiftc_path: The `path` to the `swiftc` executable.

    Returns:
        The written file containing the version info
    """
    result = repository_ctx.execute([swiftc_path, "-version"])
    contents = "unknown"
    if result.return_code == 0:
        contents = result.stdout.strip()

    filename = "swift_version"
    repository_ctx.file(filename, contents, executable = False)
    return filename

def _compute_feature_values(repository_ctx, swiftc_path):
    """Computes a list of supported/unsupported features by running checks.

    The result of this function is a list of feature names that can be provided
    as the `features` attribute of a toolchain rule. That is, enabled features
    are represented by the feature name itself, and unsupported features are
    represented as a hyphen ("-") followed by the feature name.

    Args:
        repository_ctx: The repository context.
        swiftc_path: The `path` to the `swiftc` executable.

    Returns:
        A list of feature strings that can be provided as the `features`
        attribute of a toolchain rule.
    """
    feature_values = []
    for feature, checker in _FEATURE_CHECKS.items():
        # Create a scratch directory in which the check function can write any
        # files that it needs to pass to `swiftc`.
        mktemp_result = repository_ctx.execute([
            "mktemp",
            "-d",
            "tmp.autoconfiguration.XXXXXXXXXX",
        ])
        temp_dir = repository_ctx.path(mktemp_result.stdout.strip())

        if checker(repository_ctx, swiftc_path, temp_dir):
            feature_values.append(feature)
        else:
            feature_values.append("-{}".format(feature))

        # Clean up the scratch directory.
        # TODO(allevato): Replace with `repository_ctx.delete` once it's
        # released.
        repository_ctx.execute(["rm", "-r", temp_dir])

    return feature_values

# Features whose support should be checked and the functions used to check them.
# A check function has the following signature:
#
#     def <function_name>(repository_ctx, swiftc_path, temp_dir)
#
# Where `swiftc_path` and `temp_dir` are `path` structures denoting the path to
# the `swiftc` executable and a scratch directory, respectively. The function
# should return True if the feature is supported.
_FEATURE_CHECKS = {
    SWIFT_FEATURE_LLD_GC_WORKAROUND: _check_supports_lld_gc_workaround,
    SWIFT_FEATURE__SUPPORTS_UPCOMING_FEATURES: (
        _check_supports_upcoming_features
    ),
    SWIFT_FEATURE__SUPPORTS_V6: _check_supports_language_mode_6,
}

def _normalized_linux_cpu(cpu):
    if cpu in ("amd64"):
        return "x86_64"
    return cpu

def _create_linux_toolchain(repository_ctx):
    """Creates BUILD targets for the Swift toolchain on Linux.

    Args:
      repository_ctx: The repository rule context.
    """
    path_to_swiftc = repository_ctx.which("swiftc")
    if not path_to_swiftc:
        print("""\
No 'swiftc' executable found in $PATH. Not auto-generating a Linux Swift \
toolchain.
""")  # buildifier: disable=print
        repository_ctx.file(
            "BUILD",
            """\
# No 'swiftc' executable found in $PATH. Not auto-generating a Linux Swift \
toolchain.
""",
        )
        return

    root = path_to_swiftc.dirname.dirname
    feature_values = _compute_feature_values(repository_ctx, path_to_swiftc)
    version_file = _write_swift_version(repository_ctx, path_to_swiftc)

    # TODO: This should be removed so that private headers can be used with
    # explicit modules, but the build targets for CgRPC need to be cleaned up
    # first because they contain C++ code.
    feature_values.append(SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS)
    feature_values.append(SWIFT_FEATURE_USE_AUTOLINK_EXTRACT)
    feature_values.append(SWIFT_FEATURE_USE_MODULE_WRAP)

    repository_ctx.file(
        "BUILD",
        """\
load(
    "@build_bazel_rules_swift//swift/toolchains:swift_toolchain.bzl",
    "swift_toolchain",
)

package(default_visibility = ["//visibility:public"])

swift_toolchain(
    name = "toolchain",
    arch = "{cpu}",
    features = [{feature_list}],
    os = "linux",
    root = "{root}",
    version_file = "{version_file}",
)

toolchain(
    name = "linux-swift-toolchain-{cpu}",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:{cpu}",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:{cpu}",
    ],
    toolchain = ":toolchain",
    toolchain_type = "{toolchain_type}",
    visibility = ["//visibility:public"],
)
""".format(
            cpu = _normalized_linux_cpu(repository_ctx.os.arch),
            feature_list = ", ".join([
                '"{}"'.format(feature)
                for feature in feature_values
            ]),
            root = root,
            toolchain_type = SWIFT_TOOLCHAIN_TYPE,
            version_file = version_file,
        ),
    )

def _create_xcode_toolchain(repository_ctx):
    """Creates BUILD targets for the Swift toolchain on macOS using Xcode.

    Args:
      repository_ctx: The repository rule context.
    """
    feature_values = [
        # TODO: This should be removed so that private headers can be used with
        # explicit modules, but the build targets for CgRPC need to be cleaned
        # up first because they contain C++ code.
        SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS,
    ]

    repository_ctx.file(
        "BUILD",
        """
load(
    "@build_bazel_apple_support//configs:platforms.bzl",
    "APPLE_PLATFORMS_CONSTRAINTS",
)
load(
    "@build_bazel_rules_swift//swift/toolchains:xcode_swift_toolchain.bzl",
    "xcode_swift_toolchain",
)

package(default_visibility = ["//visibility:public"])

_OSX_DEVELOPER_PLATFORM_CPUS = [
    "arm64",
    "x86_64",
]

xcode_swift_toolchain(
    name = "toolchain",
    features = [{feature_list}],
)

[
    toolchain(
        name = "xcode-toolchain-" + arch + "-" + cpu,
        exec_compatible_with = [
            "@platforms//os:macos",
            "@platforms//cpu:" + cpu,
        ],
        target_compatible_with = APPLE_PLATFORMS_CONSTRAINTS[arch],
        toolchain = ":toolchain",
        toolchain_type = "{toolchain_type}",
        visibility = ["//visibility:public"],
    )
    for arch in APPLE_PLATFORMS_CONSTRAINTS.keys()
    for cpu in _OSX_DEVELOPER_PLATFORM_CPUS
]
""".format(
            feature_list = ", ".join([
                '"{}"'.format(feature)
                for feature in feature_values
            ]),
            toolchain_type = SWIFT_TOOLCHAIN_TYPE,
        ),
    )

def _get_python_bin(repository_ctx):
    if "PYTHON_BIN_PATH" in repository_ctx.os.environ:
        return repository_ctx.os.environ.get("PYTHON_BIN_PATH").strip()
    out = repository_ctx.which("python3.exe")
    if out:
        return out
    out = repository_ctx.which("python.exe")
    if out:
        return out
    return None

def _create_windows_toolchain(repository_ctx):
    path_to_swiftc = repository_ctx.which("swiftc.exe")
    if not path_to_swiftc:
        fail("No 'swiftc.exe' executable found in Path")

    root = path_to_swiftc.dirname.dirname
    enabled_features = [
        SWIFT_FEATURE_CODEVIEW_DEBUG_INFO,
        SWIFT_FEATURE_DECLARE_SWIFTSOURCEINFO,
        SWIFT_FEATURE_DEBUG_PREFIX_MAP,
        SWIFT_FEATURE_EMIT_SWIFTDOC,
        SWIFT_FEATURE_NO_EMBED_DEBUG_MODULE,
        SWIFT_FEATURE_MODULE_MAP_NO_PRIVATE_HEADERS,
    ]
    disabled_features = []

    version_file = _write_swift_version(repository_ctx, path_to_swiftc)
    xctest_version = repository_ctx.execute([
        _get_python_bin(repository_ctx),
        "-c",
        "import os, plistlib; " +
        "print(plistlib.loads(open(os.path.join(r'{}', '..', '..', '..', 'Info.plist'), 'rb').read(), fmt=plistlib.FMT_XML)['DefaultProperties']['XCTEST_VERSION'])".format(repository_ctx.os.environ["SDKROOT"]),
    ])

    env = {
        "Path": repository_ctx.os.environ["Path"] if "Path" in repository_ctx.os.environ else repository_ctx.os.environ["PATH"],
        "ProgramData": repository_ctx.os.environ["ProgramData"],
    }

    repository_ctx.file(
        "BUILD",
        """
load(
  "@build_bazel_rules_swift//swift/toolchains:swift_toolchain.bzl",
  "swift_toolchain",
)

package(default_visibility = ["//visibility:public"])

swift_toolchain(
  name = "toolchain",
  arch = "x86_64",
  features = [{features}],
  os = "windows",
  root = "{root}",
  version_file = "{version_file}",
  env = {env},
  sdkroot = "{sdkroot}",
  tool_executable_suffix = ".exe",
  xctest_version = "{xctest_version}",
)

toolchain(
    name = "windows-swift-toolchain-x86_64",
    exec_compatible_with = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = APPLE_PLATFORMS_CONSTRAINTS[arch],
    toolchain = ":toolchain",
    toolchain_type = "{toolchain_type}",
    visibility = ["//visibility:public"],
)
""".format(
            features = ", ".join(['"{}"'.format(feature) for feature in enabled_features] + ['"-{}"'.format(feature) for feature in disabled_features]),
            root = root,
            env = env,
            sdkroot = repository_ctx.os.environ["SDKROOT"].replace("\\", "/"),
            toolchain_type = SWIFT_TOOLCHAIN_TYPE,
            xctest_version = xctest_version.stdout.rstrip(),
            version_file = version_file,
        ),
    )

def _swift_autoconfiguration_impl(repository_ctx):
    # TODO(allevato): This is expedient and fragile. Use the
    # platforms/toolchains APIs instead to define proper toolchains, and make it
    # possible to support non-Xcode toolchains on macOS as well.
    os_name = repository_ctx.os.name.lower()
    if os_name.startswith("mac os"):
        _create_xcode_toolchain(repository_ctx)
    elif os_name.startswith("windows"):
        _create_windows_toolchain(repository_ctx)
    else:
        _create_linux_toolchain(repository_ctx)

swift_autoconfiguration = repository_rule(
    environ = ["CC", "PATH", "ProgramData", "Path"],
    implementation = _swift_autoconfiguration_impl,
    local = True,
)
