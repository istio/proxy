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

"""Tests for `swift_library.private_deps`."""

load("//test/rules:provider_test.bzl", "make_provider_test_rule")

# Force private deps support to be enabled at analysis time, regardless of
# whether the active toolchain actually supports it.
private_deps_provider_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": ["swift.supports_private_deps"],
    },
)

# Test with enabled `swift.add_target_name_to_output` feature
private_deps_provider_target_name_test = make_provider_test_rule(
    config_settings = {
        "//command_line_option:features": [
            "swift.supports_private_deps",
            "swift.add_target_name_to_output",
        ],
    },
)

def private_deps_test_suite(name, tags = []):
    """Test suite for propagation behavior of `swift_library.private_deps`.

    Args:
        name: The base name to be used in targets created by this macro.
        tags: Additional tags to apply to each test.
    """
    all_tags = [name] + tags

    # Each of the two leaf libraries should propagate their own modules.
    private_deps_provider_test(
        name = "{}_private_swift_swiftmodules".format(name),
        expected_files = [
            "test_fixtures_private_deps_private_swift.swiftmodule",
        ],
        field = "transitive_modules.swift!.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:private_swift",
    )

    private_deps_provider_test(
        name = "{}_public_swift_swiftmodules".format(name),
        expected_files = [
            "test_fixtures_private_deps_public_swift.swiftmodule",
        ],
        field = "transitive_modules.swift!.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:public_swift",
    )

    # The client module should propagate its own module and the one from `deps`,
    # but not the one from `private_deps`.
    private_deps_provider_test(
        name = "{}_client_swift_deps_swiftmodules".format(name),
        expected_files = [
            "test_fixtures_private_deps_client_swift_deps.swiftmodule",
            "test_fixtures_private_deps_public_swift.swiftmodule",
            "-test_fixtures_private_deps_private_swift.swiftmodule",
        ],
        field = "transitive_modules.swift!.swiftmodule",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:client_swift_deps",
    )

    # With private deps that are C++ libraries, we shouldn't propagate the
    # compilation context of the private deps. That means the public deps'
    # headers will be repropagated by Swift library, but not the private ones.
    private_deps_provider_test(
        name = "{}_client_cc_deps_headers".format(name),
        expected_files = [
            "test/fixtures/private_deps/public.h",
            "-test/fixtures/private_deps/private.h",
            # Some C++ toolchains implicitly propagate standard library headers,
            # so we can't look for an exact match here.
            "*",
        ],
        field = "compilation_context.headers",
        provider = "CcInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:client_cc_deps",
    )

    # Likewise, we shouldn't repropagate the C++ private deps' module maps.
    private_deps_provider_test(
        name = "{}_client_cc_deps_modulemaps".format(name),
        expected_files = [
            "/test/fixtures/private_deps/public_cc_modulemap/_/module.modulemap",
            "-/test/fixtures/private_deps/private_cc_modulemap_/module.modulemap",
        ],
        field = "transitive_modules.clang!.module_map!",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:client_cc_deps",
    )

    private_deps_provider_target_name_test(
        name = "{}_client_cc_deps_modulemaps_target_name".format(name),
        expected_files = [
            "/test/fixtures/private_deps/public_cc_modulemap/_/module.modulemap",
            "-/test/fixtures/private_deps/private_cc_modulemap/_/module.modulemap",
        ],
        field = "transitive_modules.clang!.module_map!",
        provider = "SwiftInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:client_cc_deps",
    )

    # Make sure we don't also lose linking information when handling C++ private
    # deps. All libraries should be propagated, even if their compilation
    # contexts aren't.
    private_deps_provider_test(
        name = "{}_client_cc_deps_libraries".format(name),
        expected_files = [
            "/test/fixtures/private_deps/libprivate_cc.a",
            "/test/fixtures/private_deps/libpublic_cc.a",
            # There may be other libraries here, like implicit toolchain
            # dependencies, which we need to ignore.
            "*",
        ],
        field = "linking_context.linker_inputs.libraries.static_library!",
        provider = "CcInfo",
        tags = all_tags,
        target_under_test = "//test/fixtures/private_deps:client_cc_deps",
    )

    native.test_suite(
        name = name,
        tags = all_tags,
    )
