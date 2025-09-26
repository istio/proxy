# -*- coding: utf-8 -*-
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

# Tests for pkg_deb specific behavior

load("@rules_python//python:defs.bzl", "py_test")
load("//pkg:deb.bzl", "pkg_deb")
load("//pkg:mappings.bzl", "pkg_mklink")
load("//pkg:tar.bzl", "pkg_tar")
load("//tests:my_package_name.bzl", "my_package_naming")
load(":deb_tests.bzl", "package_naming_test")

package(default_applicable_licenses = ["//:license"])

genrule(
    name = "generate_files",
    outs = [
        "etc/nsswitch.conf",
        "usr/fizzbuzz",
    ],
    cmd = "for i in $(OUTS); do echo 1 >$$i; done",
)

my_package_naming(
    name = "my_package_variables",
    label = "some_value",
)

pkg_mklink(
    name = "java_link",
    link_name = "usr/bin/java",
    target = "/path/to/bin/java",
)

pkg_tar(
    name = "tar_input",
    srcs = [
        ":etc/nsswitch.conf",
        ":java_link",
        ":usr/fizzbuzz",
    ],
    extension = "tar.gz",
    mode = "0644",
    modes = {"usr/fizzbuzz": "0755"},
    owner = "42.24",
    ownername = "fizzbuzz.foobar",
    ownernames = {"etc/nsswitch.conf": "foobar.fizzbuzz"},
    owners = {"etc/nsswitch.conf": "24.42"},
    package_dir = "/",
    strip_prefix = ".",
)

pkg_deb(
    name = "test_deb",
    breaks = ["oldbrokenpkg"],
    built_using = "some_test_data (0.1.2)",
    conffiles = [
        "/etc/nsswitch.conf",
        "/etc/other",
    ],
    config = "config",
    data = ":tar_input",
    depends = [
        "dep1",
        "dep2",
    ],
    description = "toto ®, Й, ק ,م, ๗, あ, 叶, 葉, 말, ü and é\n more",
    distribution = "trusty",
    license = "Apache-2.0",
    maintainer = "soméone@somewhere.com",
    package = "fizzbuzz",
    preinst = "deb_preinst",
    provides = ["hello"],
    replaces = ["oldpkg"],
    templates = "templates",
    triggers = "deb_triggers",
    urgency = "low",
    version = "4.5.6",
)

py_test(
    name = "pkg_deb_test",
    size = "medium",
    srcs = [
        "pkg_deb_test.py",
    ],
    data = [
        # The target includes both the .deb and .changes files in DefaultInfo
        ":test_deb",
    ],
    imports = ["../.."],
    python_version = "PY3",
    deps = [
        "//pkg/private:archive",
        "@rules_python//python/runfiles",
    ],
)

package_naming_test(
    name = "naming_test",
    expected_name = "fizzbuzz_4.5.6_all.deb",
    target_under_test = ":test_deb",
)

py_test(
    name = "control_field_test",
    size = "small",
    srcs = [
        "control_field_test.py",
    ],
    imports = ["../.."],
    python_version = "PY3",
    deps = [
        "//pkg/private/deb:make_deb_lib",
    ],
)

# Test case for expanding $(var) constructions and for using ctx.var directly
pkg_deb(
    name = "deb_using_ctxvar",
    # This does not make sense for architecture, but for testing, compilation
    # mode is more stable than cpu.
    architecture = "$(COMPILATION_MODE)",
    config = "config",
    data = ":tar_input",
    description = "Compiled with $(COMPILATION_MODE)",
    #distribution = "trusty",
    maintainer = "soméone@somewhere.com",
    package = "fizzbuzz",
    version = "7",
)

package_naming_test(
    name = "expand_from_ctx_var",
    # Heads up. If the default compilation mode ever changes this will break.
    expected_name = "fizzbuzz_7_fastbuild.deb",
    target_under_test = ":deb_using_ctxvar",
)
