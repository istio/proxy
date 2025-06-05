# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""This file contains repository rules and macros to support toolchain registration.
"""

load("//python:versions.bzl", "FREETHREADED", "INSTALL_ONLY", "PLATFORMS")
load(":auth.bzl", "get_auth")
load(":repo_utils.bzl", "REPO_DEBUG_ENV_VAR", "repo_utils")
load(":text_util.bzl", "render")

STANDALONE_INTERPRETER_FILENAME = "STANDALONE_INTERPRETER"

def is_standalone_interpreter(rctx, python_interpreter_path, *, logger = None):
    """Query a python interpreter target for whether or not it's a rules_rust provided toolchain

    Args:
        rctx: {type}`repository_ctx` The repository rule's context object.
        python_interpreter_path: {type}`path` A path representing the interpreter.
        logger: Optional logger to use for operations.

    Returns:
        {type}`bool` Whether or not the target is from a rules_python generated toolchain.
    """

    # Only update the location when using a hermetic toolchain.
    if not python_interpreter_path:
        return False

    # This is a rules_python provided toolchain.
    return repo_utils.execute_unchecked(
        rctx,
        op = "IsStandaloneInterpreter",
        arguments = [
            "ls",
            "{}/{}".format(
                python_interpreter_path.dirname,
                STANDALONE_INTERPRETER_FILENAME,
            ),
        ],
        logger = logger,
    ).return_code == 0

def _python_repository_impl(rctx):
    if rctx.attr.distutils and rctx.attr.distutils_content:
        fail("Only one of (distutils, distutils_content) should be set.")
    if bool(rctx.attr.url) == bool(rctx.attr.urls):
        fail("Exactly one of (url, urls) must be set.")

    logger = repo_utils.logger(rctx)

    platform = rctx.attr.platform
    python_version = rctx.attr.python_version
    python_version_info = python_version.split(".")
    release_filename = rctx.attr.release_filename
    version_suffix = "t" if FREETHREADED in release_filename else ""
    python_short_version = "{0}.{1}{suffix}".format(
        suffix = version_suffix,
        *python_version_info
    )
    urls = rctx.attr.urls or [rctx.attr.url]
    auth = get_auth(rctx, urls)

    if INSTALL_ONLY in release_filename:
        rctx.download_and_extract(
            url = urls,
            sha256 = rctx.attr.sha256,
            stripPrefix = rctx.attr.strip_prefix,
            auth = auth,
        )
    else:
        rctx.download_and_extract(
            url = urls,
            sha256 = rctx.attr.sha256,
            stripPrefix = rctx.attr.strip_prefix,
            auth = auth,
        )

        # Strip the things that are not present in the INSTALL_ONLY builds
        # NOTE: if the dirs are not present, we will not fail here
        rctx.delete("python/build")
        rctx.delete("python/licenses")
        rctx.delete("python/PYTHON.json")

    patches = rctx.attr.patches
    if patches:
        for patch in patches:
            rctx.patch(patch, strip = rctx.attr.patch_strip)

    # Write distutils.cfg to the Python installation.
    if "windows" in platform:
        distutils_path = "Lib/distutils/distutils.cfg"
    else:
        distutils_path = "lib/python{}/distutils/distutils.cfg".format(python_short_version)
    if rctx.attr.distutils:
        rctx.file(distutils_path, rctx.read(rctx.attr.distutils))
    elif rctx.attr.distutils_content:
        rctx.file(distutils_path, rctx.attr.distutils_content)

    if "darwin" in platform and "osx" == repo_utils.get_platforms_os_name(rctx):
        # Fix up the Python distribution's LC_ID_DYLIB field.
        # It points to a build directory local to the GitHub Actions
        # host machine used in the Python standalone build, which causes
        # dyld lookup errors. To fix, set the full path to the dylib as
        # it appears in the Bazel workspace as its LC_ID_DYLIB using
        # the `install_name_tool` bundled with macOS.
        dylib = "libpython{}.dylib".format(python_short_version)
        repo_utils.execute_checked(
            rctx,
            op = "python_repository.FixUpDyldIdPath",
            arguments = [repo_utils.which_checked(rctx, "install_name_tool"), "-id", "@rpath/{}".format(dylib), "lib/{}".format(dylib)],
            logger = logger,
        )

    # Make the Python installation read-only. This is to prevent issues due to
    # pycs being generated at runtime:
    # * The pycs are not deterministic (they contain timestamps)
    # * Multiple processes trying to write the same pycs can result in errors.
    if not rctx.attr.ignore_root_user_error:
        if "windows" not in platform:
            lib_dir = "lib" if "windows" not in platform else "Lib"

            repo_utils.execute_checked(
                rctx,
                op = "python_repository.MakeReadOnly",
                arguments = [repo_utils.which_checked(rctx, "chmod"), "-R", "ugo-w", lib_dir],
                logger = logger,
            )
            exec_result = repo_utils.execute_unchecked(
                rctx,
                op = "python_repository.TestReadOnly",
                arguments = [repo_utils.which_checked(rctx, "touch"), "{}/.test".format(lib_dir)],
                logger = logger,
            )

            # The issue with running as root is the installation is no longer
            # read-only, so the problems due to pyc can resurface.
            if exec_result.return_code == 0:
                stdout = repo_utils.execute_checked_stdout(
                    rctx,
                    op = "python_repository.GetUserId",
                    arguments = [repo_utils.which_checked(rctx, "id"), "-u"],
                    logger = logger,
                )
                uid = int(stdout.strip())
                if uid == 0:
                    fail("The current user is root, please run as non-root when using the hermetic Python interpreter. See https://github.com/bazelbuild/rules_python/pull/713.")
                else:
                    fail("The current user has CAP_DAC_OVERRIDE set, please drop this capability when using the hermetic Python interpreter. See https://github.com/bazelbuild/rules_python/pull/713.")

    python_bin = "python.exe" if ("windows" in platform) else "bin/python3"

    if "linux" in platform:
        # Workaround around https://github.com/indygreg/python-build-standalone/issues/231
        for url in urls:
            head_and_release, _, _ = url.rpartition("/")
            _, _, release = head_and_release.rpartition("/")
            if not release.isdigit():
                # Maybe this is some custom toolchain, so skip this
                break

            if int(release) >= 20240224:
                # Starting with this release the Linux toolchains have infinite symlink loop
                # on host platforms that are not Linux. Delete the files no
                # matter the host platform so that the cross-built artifacts
                # are the same irrespective of the host platform we are
                # building on.
                #
                # Link to the first affected release:
                # https://github.com/indygreg/python-build-standalone/releases/tag/20240224
                rctx.delete("share/terminfo")
                break

    glob_include = []
    glob_exclude = []
    if rctx.attr.ignore_root_user_error or "windows" in platform:
        glob_exclude += [
            # These pycache files are created on first use of the associated python files.
            # Exclude them from the glob because otherwise between the first time and second time a python toolchain is used,"
            # the definition of this filegroup will change, and depending rules will get invalidated."
            # See https://github.com/bazelbuild/rules_python/issues/1008 for unconditionally adding these to toolchains so we can stop ignoring them."
            "**/__pycache__/*.pyc",
            "**/__pycache__/*.pyo",
        ]

    if "windows" in platform:
        glob_include += [
            "*.exe",
            "*.dll",
            "DLLs/**",
            "Lib/**",
            "Scripts/**",
            "tcl/**",
        ]
    else:
        glob_include.append(
            "lib/**",
        )

    if "windows" in platform:
        coverage_tool = None
    else:
        coverage_tool = rctx.attr.coverage_tool

    build_content = """\
# Generated by python/private/python_repositories.bzl

load("@rules_python//python/private:hermetic_runtime_repo_setup.bzl", "define_hermetic_runtime_toolchain_impl")

package(default_visibility = ["//visibility:public"])

define_hermetic_runtime_toolchain_impl(
  name = "define_runtime",
  extra_files_glob_include = {extra_files_glob_include},
  extra_files_glob_exclude = {extra_files_glob_exclude},
  python_version = {python_version},
  python_bin = {python_bin},
  coverage_tool = {coverage_tool},
)
""".format(
        extra_files_glob_exclude = render.list(glob_exclude),
        extra_files_glob_include = render.list(glob_include),
        python_bin = render.str(python_bin),
        python_version = render.str(rctx.attr.python_version),
        coverage_tool = render.str(coverage_tool),
    )
    rctx.delete("python")
    rctx.symlink(python_bin, "python")
    rctx.file(STANDALONE_INTERPRETER_FILENAME, "# File intentionally left blank. Indicates that this is an interpreter repo created by rules_python.")
    rctx.file("BUILD.bazel", build_content)

    attrs = {
        "auth_patterns": rctx.attr.auth_patterns,
        "coverage_tool": rctx.attr.coverage_tool,
        "distutils": rctx.attr.distutils,
        "distutils_content": rctx.attr.distutils_content,
        "ignore_root_user_error": rctx.attr.ignore_root_user_error,
        "name": rctx.attr.name,
        "netrc": rctx.attr.netrc,
        "patch_strip": rctx.attr.patch_strip,
        "patches": rctx.attr.patches,
        "platform": platform,
        "python_version": python_version,
        "release_filename": release_filename,
        "sha256": rctx.attr.sha256,
        "strip_prefix": rctx.attr.strip_prefix,
    }

    if rctx.attr.url:
        attrs["url"] = rctx.attr.url
    else:
        attrs["urls"] = urls

    return attrs

python_repository = repository_rule(
    _python_repository_impl,
    doc = "Fetches the external tools needed for the Python toolchain.",
    attrs = {
        "auth_patterns": attr.string_dict(
            doc = "Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive",
        ),
        "coverage_tool": attr.string(
            doc = """
This is a target to use for collecting code coverage information from {rule}`py_binary`
and {rule}`py_test` targets.

The target is accepted as a string by the python_repository and evaluated within
the context of the toolchain repository.

For more information see {attr}`py_runtime.coverage_tool`.
""",
        ),
        "distutils": attr.label(
            allow_single_file = True,
            doc = "A distutils.cfg file to be included in the Python installation. " +
                  "Either distutils or distutils_content can be specified, but not both.",
            mandatory = False,
        ),
        "distutils_content": attr.string(
            doc = "A distutils.cfg file content to be included in the Python installation. " +
                  "Either distutils or distutils_content can be specified, but not both.",
            mandatory = False,
        ),
        "ignore_root_user_error": attr.bool(
            default = False,
            doc = "Whether the check for root should be ignored or not. This causes cache misses with .pyc files.",
            mandatory = False,
        ),
        "netrc": attr.string(
            doc = ".netrc file to use for authentication; mirrors the eponymous attribute from http_archive",
        ),
        "patch_strip": attr.int(
            doc = """
Same as the --strip argument of Unix patch.

:::{note}
In the future the default value will be set to `0`, to mimic the well known
function defaults (e.g. `single_version_override` for `MODULE.bazel` files.
:::

:::{versionadded} 0.36.0
:::
""",
            default = 1,
            mandatory = False,
        ),
        "patches": attr.label_list(
            doc = "A list of patch files to apply to the unpacked interpreter",
            mandatory = False,
        ),
        "platform": attr.string(
            doc = "The platform name for the Python interpreter tarball.",
            mandatory = True,
            values = PLATFORMS.keys(),
        ),
        "python_version": attr.string(
            doc = "The Python version.",
            mandatory = True,
        ),
        "release_filename": attr.string(
            doc = "The filename of the interpreter to be downloaded",
            mandatory = True,
        ),
        "sha256": attr.string(
            doc = "The SHA256 integrity hash for the Python interpreter tarball.",
            mandatory = True,
        ),
        "strip_prefix": attr.string(
            doc = "A directory prefix to strip from the extracted files.",
        ),
        "url": attr.string(
            doc = "The URL of the interpreter to download. Exactly one of url and urls must be set.",
        ),
        "urls": attr.string_list(
            doc = "The URL of the interpreter to download. Exactly one of url and urls must be set.",
        ),
        "_rule_name": attr.string(default = "python_repository"),
    },
    environ = [REPO_DEBUG_ENV_VAR],
)
