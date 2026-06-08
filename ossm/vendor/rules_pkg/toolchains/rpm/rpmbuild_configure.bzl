# Copyright 2020 The Bazel Authors. All rights reserved.
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
"""Repository rule to autoconfigure a toolchain using the system rpmbuild."""

# NOTE: this must match the name used by register_toolchains in consuming
# MODULE.bazel files.  It seems like we should have a better interface that
# allows for this module name to be specified from a single point.
NAME = "rules_pkg_rpmbuild"
RELEASE_PATH = "/etc/os-release"

def _write_build(rctx, path, version, debuginfo_type):
    if not path:
        path = ""
    rctx.template(
        "BUILD",
        Label("//toolchains/rpm:BUILD.tpl"),
        substitutions = {
            "{GENERATOR}": "@rules_pkg//toolchains/rpm/rpmbuild_configure.bzl%find_system_rpmbuild",
            "{RPMBUILD_PATH}": str(path),
            "{RPMBUILD_VERSION}": version,
            "{RPMBUILD_DEBUGINFO_TYPE}": debuginfo_type,
        },
        executable = False,
    )

def _strip_quote(s):
    if s.startswith("\"") and s.endswith("\"") and len(s) > 1:
        return s[1:-1]

    return s

def _parse_release_info(release_info):
    os_name = "unknown"
    os_version = "unknown"

    for line in release_info.splitlines():
        if "=" not in line:
            continue

        key, value = line.split("=")
        if key == "ID":
            os_name = _strip_quote(value)

        if key == "VERSION_ID":
            os_version = _strip_quote(value)

    return os_name, os_version

DEBUGINFO_TYPE_AUTODETECT = "default"

# The below are also defined in `pkg/make_rpm.py`
DEBUGINFO_TYPE_NONE = "none"
DEBUGINFO_TYPE_CENTOS = "centos"
DEBUGINFO_TYPE_FEDORA = "fedora"

DEBUGINFO_TYPE_BY_OS_RELEASE = {
    "almalinux": DEBUGINFO_TYPE_CENTOS,
    "centos": DEBUGINFO_TYPE_CENTOS,
    "fedora": DEBUGINFO_TYPE_FEDORA,
}

DEBUGINFO_VALID_VALUES = [
    DEBUGINFO_TYPE_FEDORA,
    DEBUGINFO_TYPE_CENTOS,
    DEBUGINFO_TYPE_NONE,
    DEBUGINFO_TYPE_AUTODETECT,
]

def _build_repo_for_rpmbuild_toolchain_impl(rctx):
    if rctx.attr.debuginfo_type not in DEBUGINFO_VALID_VALUES:
        fail("debuginfo_type must be one of", DEBUGINFO_VALID_VALUES)

    debuginfo_type = rctx.attr.debuginfo_type
    if debuginfo_type == DEBUGINFO_TYPE_AUTODETECT:
        if rctx.path(RELEASE_PATH).exists:
            rctx.watch(RELEASE_PATH)
            os_name, _ = _parse_release_info(rctx.read(RELEASE_PATH))
            debuginfo_type = DEBUGINFO_TYPE_BY_OS_RELEASE.get(os_name, debuginfo_type)
        else:
            debuginfo_type = DEBUGINFO_TYPE_NONE

    rpmbuild_path = rctx.which("rpmbuild")
    if rctx.attr.verbose:
        if rpmbuild_path:
            print("Found rpmbuild at '%s'" % rpmbuild_path)  # buildifier: disable=print
        else:
            print("No system rpmbuild found.")  # buildifier: disable=print

    version = "unknown"
    if rpmbuild_path:
        res = rctx.execute([rpmbuild_path, "--version"])
        if res.return_code == 0:
            # expect stdout like: RPM version 4.16.1.2
            parts = res.stdout.strip().split(" ")
            if parts[0] == "RPM" and parts[1] == "version":
                version = parts[2]

    _write_build(
        rctx = rctx,
        path = rpmbuild_path,
        version = version,
        debuginfo_type = debuginfo_type,
    )

build_repo_for_rpmbuild_toolchain = repository_rule(
    implementation = _build_repo_for_rpmbuild_toolchain_impl,
    doc = """Create a repository that defines an rpmbuild toolchain based on the system rpmbuild.""",
    local = True,
    environ = ["PATH"],
    attrs = {
        "verbose": attr.bool(
            doc = "If true, print status messages.",
        ),
        "debuginfo_type": attr.string(
            doc = """
            The underlying debuginfo configuration for the system rpmbuild.

            One of `centos`, `fedora`, `none`, and `default` (which looks up `/etc/os-release`)
            """,
            default = DEBUGINFO_TYPE_AUTODETECT,
        ),
    },
)

# For use from WORKSPACE
def find_system_rpmbuild(name, verbose = False):
    build_repo_for_rpmbuild_toolchain(name = name, verbose = verbose)
    native.register_toolchains("@%s//:all" % name)

# For use from MODULE.bzl
find_system_rpmbuild_bzlmod = module_extension(
    implementation = lambda ctx: build_repo_for_rpmbuild_toolchain(name = NAME),
)
