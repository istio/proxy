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

"""Repository rule used to install cypress binary.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def cypress_repositories(
        name,
        version,
        linux_urls = [],
        linux_sha256 = "",
        darwin_urls = [],
        darwin_sha256 = "",
        darwin_arm64_urls = [],
        darwin_arm64_sha256 = "",
        windows_urls = [],
        windows_sha256 = ""):
    """
    Repository rule used to install cypress binary.

    Args:
        name: Name of the external workspace where the cypress binary lives
        version: Version of cypress binary to use. Should match package.json
        linux_urls: (Optional) URLs at which the cypress binary for linux distros of linux can be downloaded. If omitted, https://cdn.cypress.io/desktop will be used.
        linux_sha256: (Optional) SHA-256 of the linux cypress binary
        darwin_urls: (Optional) URLs at which the cypress binary for darwin can be downloaded. If omitted, https://cdn.cypress.io/desktop will be used.
        darwin_sha256: (Optional) SHA-256 of the darwin cypress binary
        darwin_arm64_urls: (Optional) URLs at which the cypress binary for darwin arm64 can be downloaded. If omitted, https://cdn.cypress.io/desktop will be used (note: as of this writing (11/2021), Cypress does not have native arm64 builds, and this URL will link to the x86_64 build to run under Rosetta).
        darwin_arm64_sha256: (Optional) SHA-256 of the darwin arm64 cypress binary
        windows_urls: (Optional) URLs at which the cypress binary for windows distros of linux can be downloaded. If omitted, https://cdn.cypress.io/desktop will be used.
        windows_sha256: (Optional) SHA-256 of the windows cypress binary
    """
    http_archive(
        name = "cypress_windows".format(name),
        sha256 = windows_sha256,
        urls = windows_urls + [
            "https://cdn.cypress.io/desktop/{}/win32-x64/cypress.zip".format(version),
        ],
        build_file_content = """
filegroup(
    name = "files",
    srcs = ["Cypress"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "bin",
    srcs = ["Cypress/Cypress.exe"],
    visibility = ["//visibility:public"],
)
""",
    )

    http_archive(
        name = "cypress_darwin".format(name),
        sha256 = darwin_sha256,
        # Cypress checks that the binary path matches **/Contents/MacOS/Cypress so we do not strip that particular prefix.
        urls = darwin_urls + [
            "https://cdn.cypress.io/desktop/{}/darwin-x64/cypress.zip".format(version),
        ],
        build_file_content = """
filegroup(
    name = "files",
    srcs = ["Cypress.app"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "bin",
    # Cypress checks that the binary path matches **/Contents/MacOS/Cypress
    srcs = ["Cypress.app/Contents/MacOS/Cypress"],
    visibility = ["//visibility:public"],
)
""",
    )

    http_archive(
        name = "cypress_darwin_arm64".format(name),
        sha256 = darwin_arm64_sha256,
        # Cypress checks that the binary path matches **/Contents/MacOS/Cypress so we do not strip that particular prefix.
        urls = darwin_arm64_urls + [
            # Note: there is currently no arm64 builds of cypress, so here we'll default to
            # the x64 version so apple silicon macs can run the binary using Rosetta.
            # Once a native arm64 build is available, this should be updated
            "https://cdn.cypress.io/desktop/{}/darwin-x64/cypress.zip".format(version),
        ],
        build_file_content = """
filegroup(
    name = "files",
    srcs = ["Cypress.app"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "bin",
    # Cypress checks that the binary path matches **/Contents/MacOS/Cypress
    srcs = ["Cypress.app/Contents/MacOS/Cypress"],
    visibility = ["//visibility:public"],
)
""",
    )

    http_archive(
        name = "cypress_linux".format(name),
        sha256 = linux_sha256,
        urls = linux_urls + [
            "https://cdn.cypress.io/desktop/{}/linux-x64/cypress.zip".format(version),
        ],
        build_file_content = """
filegroup(
    name = "files",
    srcs = ["Cypress"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "bin",
    srcs = ["Cypress/Cypress"],
    visibility = ["//visibility:public"],
)
""",
    )

    # This needs to be setup so toolchains can access nodejs for all different versions
    for os_name in ["windows", "darwin", "darwin_arm64", "linux"]:
        toolchain_label = Label("@build_bazel_rules_nodejs//toolchains/cypress:cypress_{}_toolchain".format(os_name))
        native.register_toolchains("@{}//{}:{}".format(toolchain_label.workspace_name, toolchain_label.package, toolchain_label.name))
