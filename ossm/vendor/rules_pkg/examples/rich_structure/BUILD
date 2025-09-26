# Copyright 2021 The Bazel Authors. All rights reserved.
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

load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files", "strip_prefix")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

# This is the top level BUILD for a hypothetical project Foo.  It has a client,
# a server, docs, and runtime directories needed by the server.
# We want to ship it for Linux, macOS, and Windows.
#
# This example shows various techniques for specifying how your source tree
# transforms into the installation tree. As such, it favors using a lot of
# distinct features, at the expense of uniformity.

pkg_files(
    name = "share_doc",
    srcs = [
        "//docs",
    ],
    # Where it should be in the final package
    prefix = "usr/share/doc/foo",
    # Required, but why?: see #354
    strip_prefix = strip_prefix.from_pkg(),
)

pkg_filegroup(
    name = "manpages",
    srcs = [
        "//src/client:manpages",
        "//src/server:manpages",
    ],
    prefix = "/usr/share",
)

pkg_tar(
    name = "foo_tar",
    srcs = [
        "README.txt",
        ":manpages",
        ":share_doc",
        "//resources/l10n:all",
        "//src/client:arch",
        "//src/server:arch",
    ],
)

pkg_zip(
    name = "foo_zip",
    srcs = [
        "README.txt",
        ":manpages",
        ":share_doc",
        "//resources/l10n:all",
        "//src/client:arch",
        "//src/server:arch",
    ],
)
