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

""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "subjects")
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/uv:uv_toolchain_info.bzl", "UvToolchainInfo")
load("//python/uv/private:uv.bzl", "process_modules")  # buildifier: disable=bzl-visibility
load("//python/uv/private:uv_toolchain.bzl", "uv_toolchain")  # buildifier: disable=bzl-visibility
load("//tests/support/platforms:platforms.bzl", "platform_targets")

_tests = []

def _mock_mctx(*modules, download = None, read = None):
    # Here we construct a fake minimal manifest file that we use to mock what would
    # be otherwise read from GH files
    manifest_files = {
        "different.json": {
            x: {
                "checksum": x + ".sha256",
                "kind": "executable-zip",
            }
            for x in ["linux", "osx"]
        } | {
            x + ".sha256": {
                "name": x + ".sha256",
                "target_triples": [x],
            }
            for x in ["linux", "osx"]
        },
        "manifest.json": {
            x: {
                "checksum": x + ".sha256",
                "kind": "executable-zip",
            }
            for x in ["linux", "os", "osx", "something_extra"]
        } | {
            x + ".sha256": {
                "name": x + ".sha256",
                "target_triples": [x],
            }
            for x in ["linux", "os", "osx", "something_extra"]
        },
    }

    fake_fs = {
        "linux.sha256": "deadbeef linux",
        "os.sha256": "deadbeef os",
        "osx.sha256": "deadb00f osx",
    } | {
        fname: json.encode({"artifacts": contents})
        for fname, contents in manifest_files.items()
    }

    return struct(
        path = str,
        download = download or (lambda *_, **__: struct(
            success = True,
            wait = lambda: struct(
                success = True,
            ),
        )),
        read = read or (lambda x: fake_fs[x]),
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

def _mod(*, name = None, default = [], configure = [], is_root = True):
    return struct(
        name = name,  # module_name
        tags = struct(
            default = default,
            configure = configure,
        ),
        is_root = is_root,
    )

def _process_modules(env, **kwargs):
    result = process_modules(hub_repo = struct, get_auth = lambda *_, **__: None, **kwargs)

    return env.expect.that_struct(
        struct(
            names = result.toolchain_names,
            implementations = result.toolchain_implementations,
            compatible_with = result.toolchain_compatible_with,
            target_settings = result.toolchain_target_settings,
        ),
        attrs = dict(
            names = subjects.collection,
            implementations = subjects.dict,
            compatible_with = subjects.dict,
            target_settings = subjects.dict,
        ),
    )

def _default(
        base_url = None,
        compatible_with = None,
        manifest_filename = None,
        platform = None,
        target_settings = None,
        version = None,
        netrc = None,
        auth_patterns = None,
        **kwargs):
    return struct(
        base_url = base_url,
        compatible_with = [] + (compatible_with or []),  # ensure that the type is correct
        manifest_filename = manifest_filename,
        platform = platform,
        target_settings = [] + (target_settings or []),  # ensure that the type is correct
        version = version,
        netrc = netrc,
        auth_patterns = {} | (auth_patterns or {}),  # ensure that the type is correct
        **kwargs
    )

def _configure(urls = None, sha256 = None, **kwargs):
    # We have the same attributes
    return _default(sha256 = sha256, urls = urls, **kwargs)

def _test_only_defaults(env):
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                        platform = "some_name",
                        compatible_with = ["@platforms//:incompatible"],
                    ),
                ],
            ),
        ),
    )

    # No defined platform means nothing gets registered
    uv.names().contains_exactly([
        "none",
    ])
    uv.implementations().contains_exactly({
        "none": labels.NONE,
    })
    uv.compatible_with().contains_exactly({
        "none": ["@platforms//:incompatible"],
    })
    uv.target_settings().contains_exactly({})

_tests.append(_test_only_defaults)

def _test_manual_url_spec(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                    ),
                    _default(
                        platform = "linux",
                        compatible_with = ["@platforms//os:linux"],
                    ),
                    # This will be ignored because urls are passed for some of
                    # the binaries.
                    _default(
                        platform = "osx",
                        compatible_with = ["@platforms//os:osx"],
                    ),
                ],
                configure = [
                    _configure(
                        platform = "linux",
                        urls = ["https://example.org/download.zip"],
                        sha256 = "deadbeef",
                    ),
                ],
            ),
            read = lambda *args, **kwargs: fail(args, kwargs),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_linux",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_linux": "@uv_1_0_0_linux//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_linux": ["@platforms//os:linux"],
    })
    uv.target_settings().contains_exactly({})
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_linux",
            "platform": "linux",
            "sha256": "deadbeef",
            "urls": ["https://example.org/download.zip"],
            "version": "1.0.0",
        },
    ])

_tests.append(_test_manual_url_spec)

def _test_defaults(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                        platform = "linux",
                        compatible_with = ["@platforms//os:linux"],
                        target_settings = ["//:my_flag"],
                    ),
                ],
                configure = [
                    _configure(),  # use defaults
                ],
            ),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_linux",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_linux": "@uv_1_0_0_linux//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_linux": ["@platforms//os:linux"],
    })
    uv.target_settings().contains_exactly({
        "1_0_0_linux": ["//:my_flag"],
    })
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_linux",
            "platform": "linux",
            "sha256": "deadbeef",
            "urls": ["https://example.org/1.0.0/linux"],
            "version": "1.0.0",
        },
    ])

_tests.append(_test_defaults)

def _test_default_building(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                    ),
                    _default(
                        platform = "linux",
                        compatible_with = ["@platforms//os:linux"],
                        target_settings = ["//:my_flag"],
                    ),
                    _default(
                        platform = "osx",
                        compatible_with = ["@platforms//os:osx"],
                    ),
                ],
                configure = [
                    _configure(),  # use defaults
                ],
            ),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_linux",
        "1_0_0_osx",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_linux": "@uv_1_0_0_linux//:uv_toolchain",
        "1_0_0_osx": "@uv_1_0_0_osx//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_linux": ["@platforms//os:linux"],
        "1_0_0_osx": ["@platforms//os:osx"],
    })
    uv.target_settings().contains_exactly({
        "1_0_0_linux": ["//:my_flag"],
    })
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_linux",
            "platform": "linux",
            "sha256": "deadbeef",
            "urls": ["https://example.org/1.0.0/linux"],
            "version": "1.0.0",
        },
        {
            "name": "uv_1_0_0_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.0/osx"],
            "version": "1.0.0",
        },
    ])

_tests.append(_test_default_building)

def _test_complex_configuring(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                        platform = "osx",
                        compatible_with = ["@platforms//os:os"],
                    ),
                ],
                configure = [
                    _configure(),  # use defaults
                    _configure(
                        version = "1.0.1",
                    ),  # use defaults
                    _configure(
                        version = "1.0.2",
                        base_url = "something_different",
                        manifest_filename = "different.json",
                    ),  # use defaults
                    _configure(
                        platform = "osx",
                        compatible_with = ["@platforms//os:different"],
                    ),
                    _configure(
                        version = "1.0.3",
                    ),
                    _configure(platform = "osx"),  # remove the default
                    _configure(
                        platform = "linux",
                        compatible_with = ["@platforms//os:linux"],
                    ),
                    _configure(
                        version = "1.0.4",
                        netrc = "~/.my_netrc",
                        auth_patterns = {"foo": "bar"},
                    ),  # use auth
                ],
            ),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_osx",
        "1_0_1_osx",
        "1_0_2_osx",
        "1_0_3_linux",
        "1_0_4_osx",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_osx": "@uv_1_0_0_osx//:uv_toolchain",
        "1_0_1_osx": "@uv_1_0_1_osx//:uv_toolchain",
        "1_0_2_osx": "@uv_1_0_2_osx//:uv_toolchain",
        "1_0_3_linux": "@uv_1_0_3_linux//:uv_toolchain",
        "1_0_4_osx": "@uv_1_0_4_osx//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_osx": ["@platforms//os:os"],
        "1_0_1_osx": ["@platforms//os:os"],
        "1_0_2_osx": ["@platforms//os:different"],
        "1_0_3_linux": ["@platforms//os:linux"],
        "1_0_4_osx": ["@platforms//os:os"],
    })
    uv.target_settings().contains_exactly({})
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.0/osx"],
            "version": "1.0.0",
        },
        {
            "name": "uv_1_0_1_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.1/osx"],
            "version": "1.0.1",
        },
        {
            "name": "uv_1_0_2_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["something_different/1.0.2/osx"],
            "version": "1.0.2",
        },
        {
            "name": "uv_1_0_3_linux",
            "platform": "linux",
            "sha256": "deadbeef",
            "urls": ["https://example.org/1.0.3/linux"],
            "version": "1.0.3",
        },
        {
            "auth_patterns": {"foo": "bar"},
            "name": "uv_1_0_4_osx",
            "netrc": "~/.my_netrc",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.4/osx"],
            "version": "1.0.4",
        },
    ])

_tests.append(_test_complex_configuring)

def _test_non_rules_python_non_root_is_ignored(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                        platform = "osx",
                        compatible_with = ["@platforms//os:os"],
                    ),
                ],
                configure = [
                    _configure(),  # use defaults
                ],
            ),
            _mod(
                name = "something",
                configure = [
                    _configure(version = "6.6.6"),  # use defaults whatever they are
                ],
            ),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_osx",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_osx": "@uv_1_0_0_osx//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_osx": ["@platforms//os:os"],
    })
    uv.target_settings().contains_exactly({})
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.0/osx"],
            "version": "1.0.0",
        },
    ])

_tests.append(_test_non_rules_python_non_root_is_ignored)

def _test_rules_python_does_not_take_precedence(env):
    calls = []
    uv = _process_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                default = [
                    _default(
                        base_url = "https://example.org",
                        manifest_filename = "manifest.json",
                        version = "1.0.0",
                        platform = "osx",
                        compatible_with = ["@platforms//os:os"],
                    ),
                ],
                configure = [
                    _configure(),  # use defaults
                ],
            ),
            _mod(
                name = "rules_python",
                configure = [
                    _configure(
                        version = "1.0.0",
                        base_url = "https://foobar.org",
                        platform = "osx",
                        compatible_with = ["@platforms//os:osx"],
                    ),
                ],
            ),
        ),
        uv_repository = lambda **kwargs: calls.append(kwargs),
    )

    uv.names().contains_exactly([
        "1_0_0_osx",
    ])
    uv.implementations().contains_exactly({
        "1_0_0_osx": "@uv_1_0_0_osx//:uv_toolchain",
    })
    uv.compatible_with().contains_exactly({
        "1_0_0_osx": ["@platforms//os:os"],
    })
    uv.target_settings().contains_exactly({})
    env.expect.that_collection(calls).contains_exactly([
        {
            "name": "uv_1_0_0_osx",
            "platform": "osx",
            "sha256": "deadb00f",
            "urls": ["https://example.org/1.0.0/osx"],
            "version": "1.0.0",
        },
    ])

_tests.append(_test_rules_python_does_not_take_precedence)

_analysis_tests = []

def _test_toolchain_precedence(name):
    analysis_test(
        name = name,
        impl = _test_toolchain_precedence_impl,
        target = "//python/uv:current_toolchain",
        config_settings = {
            "//command_line_option:extra_toolchains": [
                str(Label("//tests/uv/uv_toolchains:all")),
            ],
            "//command_line_option:platforms": str(platform_targets.LINUX_AARCH64),
        },
    )

def _test_toolchain_precedence_impl(env, target):
    # Check that the forwarded UvToolchainInfo looks vaguely correct.
    uv_info = env.expect.that_target(target).provider(
        UvToolchainInfo,
        factory = lambda v, meta: v,
    )
    env.expect.that_str(str(uv_info.label)).contains("//tests/uv/uv:fake_foof")

_analysis_tests.append(_test_toolchain_precedence)

def uv_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(
        name = name,
        basic_tests = _tests,
        tests = _analysis_tests,
    )

    uv_toolchain(
        name = "fake_bar",
        uv = ":BUILD.bazel",
        version = "0.0.1",
    )

    uv_toolchain(
        name = "fake_foof",
        uv = ":BUILD.bazel",
        version = "0.0.1",
    )
