# Copyright 2021-2025 Buf Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Buf toolchains macros to declare and register toolchains"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "update_attrs")

_TOOLCHAINS_REPO = "rules_buf_toolchains"

_BUILD_FILE = """
load(":toolchain.bzl", "declare_buf_toolchains")

package(default_visibility = ["//visibility:public"])

declare_buf_toolchains(
    os = "{os}",
    cpu = "{cpu}",
    rules_buf_repo_name = "{rules_buf_repo_name}",
 )
"""

_TOOLCHAIN_FILE = """
load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

def _buf_toolchain_impl(ctx):
    toolchain_info = platform_common.ToolchainInfo(
        cli = ctx.executable.cli,
    )
    return [toolchain_info]

_buf_toolchain = rule(
    implementation = _buf_toolchain_impl,
    attrs = {
        "cli": attr.label(
            doc = "The buf cli",
            executable = True,
            allow_single_file = True,
            mandatory = True,
            cfg = "exec",
        ),
    },
)

def declare_buf_toolchains(os, cpu, rules_buf_repo_name):
    for cmd in ["buf", "protoc-gen-buf-lint", "protoc-gen-buf-breaking"]:
        if os == "windows":
            native_binary(
                name = cmd,
                src = ":" + cmd + ".exe",
            )
        toolchain_impl = cmd + "_toolchain_impl"
        _buf_toolchain(
            name = toolchain_impl,
            cli = str(Label("//:"+ cmd)),
        )
        native.toolchain(
            name = cmd + "_toolchain",
            toolchain = ":" + toolchain_impl,
            toolchain_type = "@@{}//tools/{}:toolchain_type".format(rules_buf_repo_name, cmd),
            exec_compatible_with = [
                "@platforms//os:" + os,
                "@platforms//cpu:" + cpu,
            ],
        )

"""

def _register_toolchains(repo, cmd):
    native.register_toolchains(
        "@{repo}//:{cmd}_toolchain".format(
            repo = repo,
            cmd = cmd,
        ),
    )

# Copied from rules_go: https://github.com/bazelbuild/rules_go/blob/19ad920c6869a179d186a365d117ab82f38d0f3a/go/private/sdk.bzl#L517
def _detect_host_platform(ctx):
    goos = ctx.os.name
    if goos == "mac os x":
        goos = "darwin"
    elif goos.startswith("windows"):
        goos = "windows"

    goarch = ctx.os.arch
    if goarch == "aarch64":
        goarch = "arm64"
    elif goarch == "x86_64":
        goarch = "amd64"

    return goos, goarch

def _buf_download_releases_impl(ctx):
    version = ctx.attr.version
    repository_url = ctx.attr.repository_url
    sha256 = ctx.attr.sha256
    if not version:
        ctx.report_progress("Finding latest buf version")

        # Get the latest version from github. Refer: https://docs.github.com/en/rest/reference/releases
        ctx.download(
            url = "https://api.github.com/repos/bufbuild/buf/releases/latest",
            output = "version.json",
        )
        version_data = ctx.read("version.json")
        version = json.decode(version_data)["name"]

    version_number = version.removeprefix("v").split(".")
    major_version = int(version_number[0])
    minor_version = int(version_number[1])
    os, cpu = _detect_host_platform(ctx)
    if os not in ["linux", "darwin", "windows"] or cpu not in ["arm64", "amd64", "ppc64le", "s390x"]:
        fail("Unsupported operating system or cpu architecture ")
    if os == "linux" and cpu == "arm64":
        cpu = "aarch64"
    if cpu == "amd64":
        cpu = "x86_64"
    if cpu == "ppc64le" and (major_version < 1 or (major_version == 1 and minor_version < 54)):
        fail("Unsupported operating system or cpu architecture ")
    if cpu == "s390x" and (major_version < 1 or (major_version == 1 and minor_version < 56)):
        fail("Unsupported operating system or cpu architecture ")

    ctx.report_progress("Downloading buf release hash")
    url = "{}/{}/sha256.txt".format(repository_url, version)
    sha256 = ctx.download(
        url = url,
        sha256 = sha256,
        canonical_id = url,
        output = "sha256.txt",
    ).sha256
    ctx.file("WORKSPACE", "workspace(name = \"{name}\")".format(name = ctx.name))
    ctx.file("toolchain.bzl", _TOOLCHAIN_FILE)
    sha_list = ctx.read("sha256.txt").splitlines()
    for sha_line in sha_list:
        if sha_line.strip(" ").endswith(".tar.gz"):
            continue
        if sha_line.strip(" ").endswith(".zip"):
            continue
        (sum, _, bin) = sha_line.partition(" ")
        bin = bin.strip(" ")
        lower_case_bin = bin.lower()
        if lower_case_bin.find(os) == -1 or lower_case_bin.find(cpu) == -1:
            continue

        output = lower_case_bin[:lower_case_bin.find(os) - 1]
        if os == "windows":
            output += ".exe"

        ctx.report_progress("Downloading " + bin)
        url = "{}/{}/{}".format(repository_url, version, bin)
        download_info = ctx.download(
            url = url,
            sha256 = sum,
            executable = True,
            canonical_id = url,
            output = output,
        )

    if os == "darwin":
        os = "osx"

    ctx.file(
        "BUILD",
        _BUILD_FILE.format(
            os = os,
            cpu = cpu,
            rules_buf_repo_name = Label("//buf/repositories.bzl").workspace_name,
        ),
    )
    attrs = {"version": version, "repository_url": repository_url, "sha256": sha256}
    return update_attrs(ctx.attr, attrs.keys(), attrs)

buf_download_releases = repository_rule(
    implementation = _buf_download_releases_impl,
    attrs = {
        "version": attr.string(
            doc = "Buf release version",
        ),
        "repository_url": attr.string(
            doc = "Repository url base used for downloads",
            default = "https://github.com/bufbuild/buf/releases/download",
        ),
        "sha256": attr.string(
            doc = "Buf release sha256.txt checksum",
        ),
    },
)

# buildifier: disable=unnamed-macro
def rules_buf_toolchains(name = _TOOLCHAINS_REPO, version = None, sha256 = None, repository_url = None):
    """rules_buf_toolchains sets up toolchains for buf, protoc-gen-buf-lint, and protoc-gen-buf-breaking

    Args:
        name: The name of the toolchains repository. Defaults to "buf_toolchains"
        version: Release version, eg: `v.1.0.0-rc12`. If `None` defaults to latest
        sha256: The checksum sha256.txt file.
        repository_url: The repository url base used for downloads. Defaults to "https://github.com/bufbuild/buf/releases/download"
    """

    buf_download_releases(name = name, version = version, sha256 = sha256, repository_url = repository_url)

    _register_toolchains(name, "buf")
    _register_toolchains(name, "protoc-gen-buf-breaking")
    _register_toolchains(name, "protoc-gen-buf-lint")
