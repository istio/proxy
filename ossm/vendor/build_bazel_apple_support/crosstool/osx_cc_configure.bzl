# pylint: disable=g-bad-file-header
# Copyright 2016 The Bazel Authors. All rights reserved.
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
"""Configuring the C++ toolchain on macOS."""

load(
    "@bazel_tools//tools/cpp:lib_cc_configure.bzl",
    "escape_string",
)
load("@bazel_tools//tools/osx:xcode_configure.bzl", "run_xcode_locator")

def _get_escaped_xcode_cxx_inc_directories(repository_ctx, xcode_toolchains):
    """Compute the list of default C++ include paths on Xcode-enabled darwin.

    Args:
      repository_ctx: The repository context.
      xcode_toolchains: A list containing the xcode toolchains available
    Returns:
      include_paths: A list of builtin include paths.
    """

    # Assume that everything is managed by Xcode / toolchain installations
    include_dirs = [
        "/Applications/",
        "/Library/",
    ]

    user = repository_ctx.os.environ.get("USER")
    if user:
        include_dirs.extend([
            "/Users/{}/Applications/".format(user),
            "/Users/{}/Library/".format(user),
        ])

    # Include extra Xcode paths in case they're installed on other volumes
    for toolchain in xcode_toolchains:
        include_dirs.append(escape_string(toolchain.developer_dir))

    return include_dirs

def _compile_cc_file(repository_ctx, src_name, out_name):
    env = repository_ctx.os.environ
    xcrun_result = repository_ctx.execute([
        "env",
        "-i",
        "DEVELOPER_DIR={}".format(env.get("DEVELOPER_DIR", default = "")),
        "xcrun",
        "--sdk",
        "macosx",
        "clang",
        "-mmacosx-version-min=10.15",
        "-std=c++17",
        "-lc++",
        "-arch",
        "arm64",
        "-arch",
        "x86_64",
        "-Wl,-no_adhoc_codesign",
        "-Wl,-no_uuid",
        "-O3",
        "-o",
        out_name,
        src_name,
    ])

    if xcrun_result.return_code != 0:
        error_msg = (
            "return code {code}, stderr: {err}, stdout: {out}"
        ).format(
            code = xcrun_result.return_code,
            err = xcrun_result.stderr,
            out = xcrun_result.stdout,
        )
        fail(out_name + " failed to generate. Please file an issue at " +
             "https://github.com/bazelbuild/apple_support/issues with the following:\n" +
             error_msg)

    xcrun_result = repository_ctx.execute([
        "env",
        "-i",
        "codesign",
        "--identifier",  # Required to be reproducible across archs
        out_name,
        "--force",
        "--sign",
        "-",
        out_name,
    ])
    if xcrun_result.return_code != 0:
        error_msg = (
            "codesign return code {code}, stderr: {err}, stdout: {out}"
        ).format(
            code = xcrun_result.return_code,
            err = xcrun_result.stderr,
            out = xcrun_result.stdout,
        )
        fail(out_name + " failed to generate. Please file an issue at " +
             "https://github.com/bazelbuild/apple_support/issues with the following:\n" +
             error_msg)

def configure_osx_toolchain(repository_ctx):
    """Configure C++ toolchain on macOS.

    Args:
      repository_ctx: The repository context.

    Returns:
      Whether or not configuration was successful
    """

    # All Label resolutions done at the top of the function to avoid issues
    # with starlark function restarts, see this:
    # https://github.com/bazelbuild/bazel/blob/ab71a1002c9c53a8061336e40f91204a2a32c38e/tools/cpp/lib_cc_configure.bzl#L17-L38
    # for more info
    xcode_locator = Label("@bazel_tools//tools/osx:xcode_locator.m")
    osx_cc_wrapper = Label("@bazel_tools//tools/cpp:osx_cc_wrapper.sh.tpl")
    xcrunwrapper = Label("@build_bazel_apple_support//crosstool:xcrunwrapper.sh")
    libtool = Label("@build_bazel_apple_support//crosstool:libtool.sh")
    make_hashed_objlist = Label("@build_bazel_apple_support//crosstool:make_hashed_objlist.py")
    cc_toolchain_config = Label("@build_bazel_apple_support//crosstool:cc_toolchain_config.bzl")
    build_template = Label("@build_bazel_apple_support//crosstool:BUILD.tpl")
    libtool_check_unique_src_path = str(repository_ctx.path(
        Label("@build_bazel_apple_support//crosstool:libtool_check_unique.cc"),
    ))
    wrapped_clang_src_path = str(repository_ctx.path(
        Label("@build_bazel_apple_support//crosstool:wrapped_clang.cc"),
    ))

    xcode_toolchains = []
    xcodeloc_err = ""
    allow_non_applications_xcode = "BAZEL_ALLOW_NON_APPLICATIONS_XCODE" in repository_ctx.os.environ and repository_ctx.os.environ["BAZEL_ALLOW_NON_APPLICATIONS_XCODE"] == "1"
    if allow_non_applications_xcode:
        (xcode_toolchains, xcodeloc_err) = run_xcode_locator(repository_ctx, xcode_locator)
        if not xcode_toolchains:
            return False, xcodeloc_err

    # For Xcode toolchains, there's no reason to use anything other than
    # wrapped_clang, so that we still get the Bazel Xcode placeholder
    # substitution and other behavior for actions that invoke this
    # cc_wrapper.sh script. The wrapped_clang binary is already hardcoded
    # into the Objective-C crosstool actions, anyway, so this ensures that
    # the C++ actions behave consistently.
    cc_path = '"$(/usr/bin/dirname "$0")"/wrapped_clang'
    repository_ctx.template(
        "cc_wrapper.sh",
        osx_cc_wrapper,
        {
            "%{cc}": escape_string(cc_path),
            "%{env}": "",
        },
    )
    repository_ctx.symlink(xcrunwrapper, "xcrunwrapper.sh")
    repository_ctx.symlink(libtool, "libtool")
    repository_ctx.symlink(make_hashed_objlist, "make_hashed_objlist.py")
    repository_ctx.symlink(cc_toolchain_config, "cc_toolchain_config.bzl")
    _compile_cc_file(repository_ctx, libtool_check_unique_src_path, "libtool_check_unique")
    _compile_cc_file(repository_ctx, wrapped_clang_src_path, "wrapped_clang")
    repository_ctx.symlink("wrapped_clang", "wrapped_clang_pp")

    tool_paths = {}
    gcov_path = repository_ctx.os.environ.get("GCOV")
    if gcov_path != None:
        if not gcov_path.startswith("/"):
            gcov_path = repository_ctx.which(gcov_path)
        tool_paths["gcov"] = gcov_path

    escaped_include_paths = _get_escaped_xcode_cxx_inc_directories(repository_ctx, xcode_toolchains)
    escaped_cxx_include_directories = []
    for path in escaped_include_paths:
        escaped_cxx_include_directories.append(("            \"%s\"," % path))
    if xcodeloc_err:
        escaped_cxx_include_directories.append("            # Error: " + xcodeloc_err)
    repository_ctx.template(
        "BUILD",
        build_template,
        {
            "%{cxx_builtin_include_directories}": "\n".join(escaped_cxx_include_directories),
            "%{tool_paths_overrides}": ",\n            ".join(
                ['"%s": "%s"' % (k, v) for k, v in tool_paths.items()],
            ),
        },
    )

    return True, ""
