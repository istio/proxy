# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unit tests for //python/extensions:python.bzl bzlmod extension."""

load("@pythons_hub//:versions.bzl", "MINOR_MAPPING")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:python.bzl", "parse_modules")  # buildifier: disable=bzl-visibility
load("//python/private:repo_utils.bzl", "repo_utils")  # buildifier: disable=bzl-visibility

_tests = []

def _mock_mctx(*modules, environ = {}, mocked_files = {}):
    return struct(
        path = lambda x: struct(exists = x in mocked_files, _file = x),
        read = lambda x, watch = None: mocked_files[x._file if "_file" in dir(x) else x],
        getenv = environ.get,
        os = struct(environ = environ),
        modules = [
            struct(
                name = modules[0].name,
                tags = modules[0].tags,
                is_root = modules[0].is_root,
            ),
        ] + [
            struct(
                name = mod.name,
                tags = mod.tags,
                is_root = False,
            )
            for mod in modules[1:]
        ],
    )

# todo: change is_root to false by default. most modules aren't root
def _mod(*, name, defaults = [], toolchain = [], override = [], single_version_override = [], single_version_platform_override = [], is_root = False):
    return struct(
        name = name,
        tags = struct(
            defaults = defaults,
            toolchain = toolchain,
            override = override,
            single_version_override = single_version_override,
            single_version_platform_override = single_version_platform_override,
        ),
        is_root = is_root,
    )

def _defaults(python_version = None, python_version_env = None, python_version_file = None):
    return struct(
        python_version = python_version,
        python_version_env = python_version_env,
        python_version_file = python_version_file,
    )

def _toolchain(python_version, *, is_default = False, **kwargs):
    return struct(
        is_default = is_default,
        python_version = python_version,
        **kwargs
    )

def _override(
        auth_patterns = {},
        available_python_versions = [],
        base_url = "",
        minor_mapping = {},
        netrc = "",
        register_all_versions = False):
    return struct(
        auth_patterns = auth_patterns,
        available_python_versions = available_python_versions,
        base_url = base_url,
        minor_mapping = minor_mapping,
        netrc = netrc,
        register_all_versions = register_all_versions,
    )

def _rules_python_module(is_root = False):
    """A mock of what the real rules_python MODULE.bazel looks like."""
    return _mod(
        name = "rules_python",
        defaults = [_defaults(python_version = "3.11")],
        toolchain = [_toolchain("3.11")],
        is_root = is_root,
    )

def _single_version_override(
        python_version = "",
        sha256 = {},
        urls = [],
        patch_strip = 0,
        patches = [],
        strip_prefix = "python",
        distutils_content = "",
        distutils = None):
    if not python_version:
        fail("missing mandatory args: python_version ({})".format(python_version))

    return struct(
        python_version = python_version,
        sha256 = sha256,
        urls = urls,
        patch_strip = patch_strip,
        patches = patches,
        strip_prefix = strip_prefix,
        distutils_content = distutils_content,
        distutils = distutils,
    )

def _single_version_platform_override(
        coverage_tool = None,
        patch_strip = 0,
        patches = [],
        platform = "",
        python_version = "",
        sha256 = "",
        strip_prefix = "python",
        urls = []):
    if not platform or not python_version:
        fail("missing mandatory args: platform ({}) and python_version ({})".format(platform, python_version))

    return struct(
        sha256 = sha256,
        urls = urls,
        strip_prefix = strip_prefix,
        platform = platform,
        coverage_tool = coverage_tool,
        python_version = python_version,
        patch_strip = patch_strip,
        patches = patches,
        target_compatible_with = [],
        target_settings = [],
        os_name = "",
        arch = "",
    )

def _test_default_from_rules_python_when_rules_python_is_root(env):
    """Verify that rules_python (as root module) default is applied."""
    py = parse_modules(
        module_ctx = _mock_mctx(
            _rules_python_module(is_root = True),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    # The value there should be consistent in bzlmod with the automatically
    # calculated value Please update the MINOR_MAPPING in //python:versions.bzl
    # when this part starts failing.
    env.expect.that_dict(py.config.minor_mapping).contains_exactly(MINOR_MAPPING)
    env.expect.that_collection(py.config.kwargs).has_size(0)
    env.expect.that_collection(py.config.default.keys()).contains_exactly([
        "base_url",
        "tool_versions",
        "platforms",
    ])
    env.expect.that_str(py.default_python_version).equals("3.11")

    want_toolchain = struct(
        name = "python_3_11",
        python_version = "3.11",
        register_coverage_tool = False,
    )
    env.expect.that_collection(py.toolchains).contains_exactly([want_toolchain])

_tests.append(_test_default_from_rules_python_when_rules_python_is_root)

def _test_default_from_rules_python_when_rules_python_is_not_root(env):
    """Verify that rules_python default applies when rules_python is not the root module."""
    py = parse_modules(
        module_ctx = _mock_mctx(
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.11")

    want_toolchain = struct(
        name = "python_3_11",
        python_version = "3.11",
        register_coverage_tool = False,
    )
    env.expect.that_collection(py.toolchains).contains_exactly([want_toolchain])

_tests.append(_test_default_from_rules_python_when_rules_python_is_not_root)

def _test_default_with_patch_version(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(name = "alpha", toolchain = [_toolchain("3.11.2")], is_root = True),
            _rules_python_module(is_root = True),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.11.2")

    want_toolchain = struct(
        name = "python_3_11_2",
        python_version = "3.11.2",
        register_coverage_tool = False,
    )
    env.expect.that_collection(py.toolchains).contains_at_least([want_toolchain])

_tests.append(_test_default_with_patch_version)

def _test_toolchain_ordering(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                toolchain = [
                    _toolchain("3.10"),
                    _toolchain("3.10.15"),
                    _toolchain(MINOR_MAPPING["3.10"]),
                    _toolchain("3.10.13"),
                    _toolchain("3.11.1"),
                    _toolchain("3.11.10"),
                    _toolchain(MINOR_MAPPING["3.11"], is_default = True),
                ],
                is_root = True,
            ),
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )
    got_versions = [
        t.python_version
        for t in py.toolchains
    ]

    env.expect.that_str(py.default_python_version).equals(MINOR_MAPPING["3.11"])
    env.expect.that_dict(py.config.minor_mapping).contains_exactly(MINOR_MAPPING)
    env.expect.that_collection(got_versions).contains_exactly([
        # First the full-version toolchains that are in minor_mapping
        # so that they get matched first if only the `python_version` is in MINOR_MAPPING
        #
        # The default version is always set in the `python_version` flag, so know, that
        # the default match will be somewhere in the first bunch.
        "3.10",
        MINOR_MAPPING["3.10"],
        "3.11",
        MINOR_MAPPING["3.11"],
        # Next, the rest, where we will match things based on the `python_version` being
        # the same
        "3.10.15",
        "3.10.13",
        "3.11.1",
        "3.11.10",
    ]).in_order()

_tests.append(_test_toolchain_ordering)

def _test_default_from_defaults(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                defaults = [_defaults(python_version = "3.11")],
                toolchain = [_toolchain("3.10"), _toolchain("3.11"), _toolchain("3.12")],
                is_root = True,
            ),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.11")

    want_toolchains = [
        struct(
            name = "python_3_" + minor_version,
            python_version = "3." + minor_version,
            register_coverage_tool = False,
        )
        for minor_version in ["10", "11", "12"]
    ]
    env.expect.that_collection(py.toolchains).contains_exactly(want_toolchains)

_tests.append(_test_default_from_defaults)

def _test_default_from_defaults_env(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                defaults = [_defaults(python_version = "3.11", python_version_env = "PYENV_VERSION")],
                toolchain = [_toolchain("3.10"), _toolchain("3.11"), _toolchain("3.12")],
                is_root = True,
            ),
            environ = {"PYENV_VERSION": "3.12"},
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.12")

    want_toolchains = [
        struct(
            name = "python_3_" + minor_version,
            python_version = "3." + minor_version,
            register_coverage_tool = False,
        )
        for minor_version in ["10", "11", "12"]
    ]
    env.expect.that_collection(py.toolchains).contains_exactly(want_toolchains)

_tests.append(_test_default_from_defaults_env)

def _test_default_from_defaults_file(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                defaults = [_defaults(python_version_file = "@@//:.python-version")],
                toolchain = [_toolchain("3.10"), _toolchain("3.11"), _toolchain("3.12")],
                is_root = True,
            ),
            mocked_files = {"@@//:.python-version": "3.12\n"},
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.12")

    want_toolchains = [
        struct(
            name = "python_3_" + minor_version,
            python_version = "3." + minor_version,
            register_coverage_tool = False,
        )
        for minor_version in ["10", "11", "12"]
    ]
    env.expect.that_collection(py.toolchains).contains_exactly(want_toolchains)

_tests.append(_test_default_from_defaults_file)

def _test_default_from_single_toolchain(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                toolchain = [_toolchain("3.12")],
                is_root = True,
            ),
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )
    env.expect.that_str(py.default_python_version).equals("3.12")

_tests.append(_test_default_from_single_toolchain)

def _test_defaults_overrides_single_toolchain(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                defaults = [
                    # This relies on rules_python registering 3.11
                    _defaults(python_version = "3.11"),
                ],
                toolchain = [_toolchain("3.12")],
                is_root = True,
            ),
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )
    env.expect.that_str(py.default_python_version).equals("3.11")

_tests.append(_test_defaults_overrides_single_toolchain)

def _test_defaults_overrides_toolchains_setting_is_default(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_root_module",
                defaults = [_defaults(python_version = "3.13")],
                toolchain = [
                    _toolchain("3.13"),
                    _toolchain("3.12", is_default = True),
                ],
                is_root = True,
            ),
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )
    env.expect.that_str(py.default_python_version).equals("3.13")

_tests.append(_test_defaults_overrides_toolchains_setting_is_default)

def _test_first_occurance_of_the_toolchain_wins(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(name = "my_module", is_root = True, toolchain = [_toolchain("3.12")]),
            _mod(name = "some_module", toolchain = [_toolchain("3.12", configure_coverage_tool = True)]),
            _rules_python_module(),
            environ = {
                "RULES_PYTHON_BZLMOD_DEBUG": "1",
            },
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.12")

    my_module_toolchain = struct(
        name = "python_3_12",
        python_version = "3.12",
        # NOTE: coverage stays disabled even though `some_module` was
        # configuring something else.
        register_coverage_tool = False,
    )
    rules_python_toolchain = struct(
        name = "python_3_11",
        python_version = "3.11",
        register_coverage_tool = False,
    )
    env.expect.that_collection(py.toolchains).contains_exactly([
        rules_python_toolchain,
        my_module_toolchain,  # default toolchain is last
    ]).in_order()

    env.expect.that_dict(py.debug_info).contains_exactly({
        "toolchains_registered": [
            {"module": {"is_root": True, "name": "my_module"}, "name": "python_3_12"},
            {"module": {"is_root": False, "name": "rules_python"}, "name": "python_3_11"},
        ],
    })

_tests.append(_test_first_occurance_of_the_toolchain_wins)

def _test_auth_overrides(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                toolchain = [_toolchain("3.12")],
                override = [
                    _override(
                        netrc = "/my/netrc",
                        auth_patterns = {"foo": "bar"},
                    ),
                ],
                is_root = True,
            ),
            _rules_python_module(),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_dict(py.config.default).contains_at_least({
        "auth_patterns": {"foo": "bar"},
        "netrc": "/my/netrc",
    })
    env.expect.that_str(py.default_python_version).equals("3.12")

    my_module_toolchain = struct(
        name = "python_3_12",
        python_version = "3.12",
        register_coverage_tool = False,
    )
    rules_python_toolchain = struct(
        name = "python_3_11",
        python_version = "3.11",
        register_coverage_tool = False,
    )
    env.expect.that_collection(py.toolchains).contains_exactly([
        rules_python_toolchain,
        my_module_toolchain,
    ]).in_order()

_tests.append(_test_auth_overrides)

def _test_add_new_version(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                is_root = True,
                toolchain = [_toolchain("3.13")],
                single_version_override = [
                    _single_version_override(
                        python_version = "3.13.0",
                        sha256 = {
                            "aarch64-unknown-linux-gnu": "deadbeef",
                        },
                        urls = ["example.org"],
                        patch_strip = 0,
                        patches = [],
                        strip_prefix = "prefix",
                        distutils_content = "",
                        distutils = None,
                    ),
                ],
                single_version_platform_override = [
                    _single_version_platform_override(
                        sha256 = "deadb00f",
                        urls = ["something.org", "else.org"],
                        strip_prefix = "python",
                        platform = "aarch64-unknown-linux-gnu",
                        coverage_tool = "specific_cov_tool",
                        python_version = "3.13.99",
                        patch_strip = 2,
                        patches = ["specific-patch.txt"],
                    ),
                ],
                override = [
                    _override(
                        base_url = "",
                        available_python_versions = ["3.12.4", "3.13.0", "3.13.1", "3.13.99"],
                        minor_mapping = {
                            "3.13": "3.13.99",
                        },
                    ),
                ],
            ),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.13")
    env.expect.that_collection(py.config.default["tool_versions"].keys()).contains_exactly([
        "3.12.4",
        "3.13.0",
        "3.13.1",
        "3.13.99",
    ])
    env.expect.that_dict(py.config.default["tool_versions"]["3.13.0"]).contains_exactly({
        "sha256": {"aarch64-unknown-linux-gnu": "deadbeef"},
        "strip_prefix": {"aarch64-unknown-linux-gnu": "prefix"},
        "url": {"aarch64-unknown-linux-gnu": ["example.org"]},
    })
    env.expect.that_dict(py.config.default["tool_versions"]["3.13.99"]).contains_exactly({
        "coverage_tool": {"aarch64-unknown-linux-gnu": "specific_cov_tool"},
        "patch_strip": {"aarch64-unknown-linux-gnu": 2},
        "patches": {"aarch64-unknown-linux-gnu": ["specific-patch.txt"]},
        "sha256": {"aarch64-unknown-linux-gnu": "deadb00f"},
        "strip_prefix": {"aarch64-unknown-linux-gnu": "python"},
        "url": {"aarch64-unknown-linux-gnu": ["something.org", "else.org"]},
    })
    env.expect.that_dict(py.config.minor_mapping).contains_exactly({
        "3.12": "3.12.4",  # The `minor_mapping` will be overriden only for the missing keys
        "3.13": "3.13.99",
    })
    env.expect.that_collection(py.toolchains).contains_exactly([
        struct(
            name = "python_3_13",
            python_version = "3.13",
            register_coverage_tool = False,
        ),
    ])

_tests.append(_test_add_new_version)

def _test_register_all_versions(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                is_root = True,
                toolchain = [_toolchain("3.13")],
                single_version_override = [
                    _single_version_override(
                        python_version = "3.13.0",
                        sha256 = {
                            "aarch64-unknown-linux-gnu": "deadbeef",
                        },
                        urls = ["example.org"],
                    ),
                ],
                single_version_platform_override = [
                    _single_version_platform_override(
                        sha256 = "deadb00f",
                        urls = ["something.org"],
                        platform = "aarch64-unknown-linux-gnu",
                        python_version = "3.13.99",
                    ),
                ],
                override = [
                    _override(
                        base_url = "",
                        available_python_versions = ["3.12.4", "3.13.0", "3.13.1", "3.13.99"],
                        register_all_versions = True,
                    ),
                ],
            ),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.13")
    env.expect.that_collection(py.config.default["tool_versions"].keys()).contains_exactly([
        "3.12.4",
        "3.13.0",
        "3.13.1",
        "3.13.99",
    ])
    env.expect.that_dict(py.config.minor_mapping).contains_exactly({
        # The mapping is calculated automatically
        "3.12": "3.12.4",
        "3.13": "3.13.99",
    })
    env.expect.that_collection(py.toolchains).contains_exactly([
        struct(
            name = name,
            python_version = version,
            register_coverage_tool = False,
        )
        for name, version in {
            "python_3_12": "3.12",
            "python_3_12_4": "3.12.4",
            "python_3_13": "3.13",
            "python_3_13_0": "3.13.0",
            "python_3_13_1": "3.13.1",
            "python_3_13_99": "3.13.99",
        }.items()
    ])

_tests.append(_test_register_all_versions)

def _test_ignore_unsupported_versions(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                is_root = True,
                toolchain = [
                    _toolchain("3.11"),
                    _toolchain("3.12"),
                    _toolchain("3.13", is_default = True),
                ],
                single_version_override = [
                    _single_version_override(
                        python_version = "3.13.0",
                        sha256 = {
                            "aarch64-unknown-linux-gnu": "deadbeef",
                        },
                        urls = ["example.org"],
                    ),
                ],
                single_version_platform_override = [
                    _single_version_platform_override(
                        sha256 = "deadb00f",
                        urls = ["something.org"],
                        platform = "aarch64-unknown-linux-gnu",
                        python_version = "3.13.99",
                    ),
                ],
                override = [
                    _override(
                        base_url = "",
                        available_python_versions = ["3.12.4", "3.13.0", "3.13.1"],
                        minor_mapping = {
                            "3.12": "3.12.4",
                            "3.13": "3.13.1",
                        },
                    ),
                ],
            ),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.13")
    env.expect.that_collection(py.config.default["tool_versions"].keys()).contains_exactly([
        "3.12.4",
        "3.13.0",
        "3.13.1",
    ])
    env.expect.that_dict(py.config.minor_mapping).contains_exactly({
        # The mapping is calculated automatically
        "3.12": "3.12.4",
        "3.13": "3.13.1",
    })
    env.expect.that_collection(py.toolchains).contains_exactly([
        struct(
            name = name,
            python_version = version,
            register_coverage_tool = False,
        )
        for name, version in {
            # NOTE: that '3.11' wont be actually registered and present in the
            # `tool_versions` above.
            "python_3_11": "3.11",
            "python_3_12": "3.12",
            "python_3_13": "3.13",
        }.items()
    ])

_tests.append(_test_ignore_unsupported_versions)

def _test_add_patches(env):
    py = parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                is_root = True,
                toolchain = [_toolchain("3.13")],
                single_version_override = [
                    _single_version_override(
                        python_version = "3.13.0",
                        sha256 = {
                            "aarch64-apple-darwin": "deadbeef",
                            "aarch64-unknown-linux-gnu": "deadbeef",
                        },
                        urls = ["example.org"],
                        patch_strip = 1,
                        patches = ["common.txt"],
                        strip_prefix = "prefix",
                        distutils_content = "",
                        distutils = None,
                    ),
                ],
                single_version_platform_override = [
                    _single_version_platform_override(
                        sha256 = "deadb00f",
                        urls = ["something.org", "else.org"],
                        strip_prefix = "python",
                        platform = "aarch64-unknown-linux-gnu",
                        coverage_tool = "specific_cov_tool",
                        python_version = "3.13.0",
                        patch_strip = 2,
                        patches = ["specific-patch.txt"],
                    ),
                ],
                override = [
                    _override(
                        base_url = "",
                        available_python_versions = ["3.13.0"],
                        minor_mapping = {
                            "3.13": "3.13.0",
                        },
                    ),
                ],
            ),
        ),
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )

    env.expect.that_str(py.default_python_version).equals("3.13")
    env.expect.that_dict(py.config.default["tool_versions"]).contains_exactly({
        "3.13.0": {
            "coverage_tool": {"aarch64-unknown-linux-gnu": "specific_cov_tool"},
            "patch_strip": {"aarch64-apple-darwin": 1, "aarch64-unknown-linux-gnu": 2},
            "patches": {
                "aarch64-apple-darwin": ["common.txt"],
                "aarch64-unknown-linux-gnu": ["specific-patch.txt"],
            },
            "sha256": {"aarch64-apple-darwin": "deadbeef", "aarch64-unknown-linux-gnu": "deadb00f"},
            "strip_prefix": {"aarch64-apple-darwin": "prefix", "aarch64-unknown-linux-gnu": "python"},
            "url": {
                "aarch64-apple-darwin": ["example.org"],
                "aarch64-unknown-linux-gnu": ["something.org", "else.org"],
            },
        },
    })
    env.expect.that_dict(py.config.minor_mapping).contains_exactly({
        "3.13": "3.13.0",
    })
    env.expect.that_collection(py.toolchains).contains_exactly([
        struct(
            name = "python_3_13",
            python_version = "3.13",
            register_coverage_tool = False,
        ),
    ])

_tests.append(_test_add_patches)

def _test_fail_two_overrides(env):
    errors = []
    parse_modules(
        module_ctx = _mock_mctx(
            _mod(
                name = "my_module",
                is_root = True,
                toolchain = [_toolchain("3.13")],
                override = [
                    _override(base_url = "foo"),
                    _override(base_url = "bar"),
                ],
            ),
        ),
        _fail = errors.append,
        logger = repo_utils.logger(verbosity_level = 0, name = "python"),
    )
    env.expect.that_collection(errors).contains_exactly([
        "Only a single 'python.override' can be present",
    ])

_tests.append(_test_fail_two_overrides)

def _test_single_version_override_errors(env):
    for test in [
        struct(
            overrides = [
                _single_version_override(python_version = "3.12.4", distutils_content = "foo"),
                _single_version_override(python_version = "3.12.4", distutils_content = "foo"),
            ],
            want_error = "Only a single 'python.single_version_override' can be present for '3.12.4'",
        ),
    ]:
        errors = []
        parse_modules(
            module_ctx = _mock_mctx(
                _mod(
                    name = "my_module",
                    is_root = True,
                    toolchain = [_toolchain("3.13")],
                    single_version_override = test.overrides,
                ),
            ),
            _fail = errors.append,
            logger = repo_utils.logger(verbosity_level = 0, name = "python"),
        )
        env.expect.that_collection(errors).contains_exactly([test.want_error])

_tests.append(_test_single_version_override_errors)

def _test_single_version_platform_override_errors(env):
    for test in [
        struct(
            overrides = [
                _single_version_platform_override(python_version = "3.12.4", platform = "foo", coverage_tool = "foo"),
                _single_version_platform_override(python_version = "3.12.4", platform = "foo", coverage_tool = "foo"),
            ],
            want_error = "Only a single 'python.single_version_platform_override' can be present for '(\"3.12.4\", \"foo\")'",
        ),
        struct(
            overrides = [
                _single_version_platform_override(python_version = "3.12", platform = "foo"),
            ],
            want_error = "The 'python_version' attribute needs to specify the full version in at least 'X.Y.Z' format, got: '3.12'",
        ),
        struct(
            overrides = [
                _single_version_platform_override(python_version = "foo", platform = "foo"),
            ],
            want_error = "Failed to parse PEP 440 version identifier 'foo'. Parse error at 'foo'",
        ),
    ]:
        errors = []
        parse_modules(
            module_ctx = _mock_mctx(
                _mod(
                    name = "my_module",
                    toolchain = [_toolchain("3.13")],
                    single_version_platform_override = test.overrides,
                    is_root = True,
                ),
            ),
            _fail = lambda *a: errors.append(" ".join(a)),
            logger = repo_utils.logger(verbosity_level = 0, name = "python"),
        )
        env.expect.that_collection(errors).contains_exactly([test.want_error])

_tests.append(_test_single_version_platform_override_errors)

# TODO @aignas 2024-09-03: add failure tests:
# * incorrect platform failure
# * missing python_version failure

def python_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
