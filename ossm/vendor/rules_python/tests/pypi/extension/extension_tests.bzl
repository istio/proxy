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

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "subjects")
load("//python/private/pypi:extension.bzl", "build_config", "parse_modules")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:whl_config_setting.bzl", "whl_config_setting")  # buildifier: disable=bzl-visibility
load(":pip_parse.bzl", _parse = "pip_parse")

_tests = []

def _mock_mctx(*modules, os_name = "unittest", arch_name = "exotic", environ = {}, read = None):
    return struct(
        os = struct(
            environ = environ,
            name = os_name,
            arch = arch_name,
        ),
        read = read or (lambda _: """\
simple==0.0.1 \
    --hash=sha256:deadbeef \
    --hash=sha256:deadbaaf"""),
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

def _mod(*, name, default = [], parse = [], override = [], whl_mods = [], is_root = True):
    return struct(
        name = name,
        tags = struct(
            parse = parse,
            override = override,
            whl_mods = whl_mods,
            default = default or [
                _default(
                    platform = "{}_{}{}".format(os, cpu, freethreaded),
                    os_name = os,
                    arch_name = cpu,
                    config_settings = [
                        "@platforms//os:{}".format(os),
                        "@platforms//cpu:{}".format(cpu),
                    ],
                    whl_abi_tags = ["cp{major}{minor}t"] if freethreaded else ["abi3", "cp{major}{minor}"],
                    whl_platform_tags = whl_platform_tags,
                )
                for (os, cpu, freethreaded), whl_platform_tags in {
                    ("linux", "x86_64", ""): ["linux_x86_64", "manylinux_*_x86_64"],
                    ("linux", "x86_64", "_freethreaded"): ["linux_x86_64", "manylinux_*_x86_64"],
                    ("linux", "aarch64", ""): ["linux_aarch64", "manylinux_*_aarch64"],
                    ("osx", "aarch64", ""): ["macosx_*_arm64"],
                    ("windows", "aarch64", ""): ["win_arm64"],
                }.items()
            ],
        ),
        is_root = is_root,
    )

def _parse_modules(env, enable_pipstar = 0, **kwargs):
    return env.expect.that_struct(
        parse_modules(
            enable_pipstar = enable_pipstar,
            **kwargs
        ),
        attrs = dict(
            exposed_packages = subjects.dict,
            hub_group_map = subjects.dict,
            hub_whl_map = subjects.dict,
            whl_libraries = subjects.dict,
            whl_mods = subjects.dict,
        ),
    )

def _build_config(env, enable_pipstar = 0, **kwargs):
    return env.expect.that_struct(
        build_config(
            enable_pipstar = enable_pipstar,
            enable_pipstar_extract = True,
            **kwargs
        ),
        attrs = dict(
            auth_patterns = subjects.dict,
            enable_pipstar = subjects.bool,
            netrc = subjects.str,
            platforms = subjects.dict,
        ),
    )

def _default(
        *,
        arch_name = None,
        auth_patterns = None,
        config_settings = None,
        env = None,
        marker = None,
        netrc = None,
        os_name = None,
        platform = None,
        whl_platform_tags = None,
        whl_abi_tags = None):
    return struct(
        arch_name = arch_name,
        auth_patterns = auth_patterns or {},
        config_settings = config_settings,
        env = env or {},
        marker = marker or "",
        netrc = netrc,
        os_name = os_name,
        platform = platform,
        whl_abi_tags = whl_abi_tags or [],
        whl_platform_tags = whl_platform_tags or [],
    )

def _test_simple(env):
    pypi = _parse_modules(
        env,
        module_ctx = _mock_mctx(
            _mod(
                name = "rules_python",
                parse = [
                    _parse(
                        hub_name = "pypi",
                        python_version = "3.15",
                        requirements_lock = "requirements.txt",
                    ),
                ],
            ),
            os_name = "linux",
            arch_name = "x86_64",
        ),
        available_interpreters = {
            "python_3_15_host": "unit_test_interpreter_target",
        },
        minor_mapping = {"3.15": "3.15.19"},
    )

    pypi.exposed_packages().contains_exactly({"pypi": ["simple"]})
    pypi.hub_group_map().contains_exactly({"pypi": {}})
    pypi.hub_whl_map().contains_exactly({"pypi": {
        "simple": {
            "pypi_315_simple": [
                whl_config_setting(
                    version = "3.15",
                ),
            ],
        },
    }})
    pypi.whl_libraries().contains_exactly({
        "pypi_315_simple": {
            "config_load": "@pypi//:config.bzl",
            "dep_template": "@pypi//{name}:{target}",
            "python_interpreter_target": "unit_test_interpreter_target",
            "requirement": "simple==0.0.1 --hash=sha256:deadbeef --hash=sha256:deadbaaf",
        },
    })
    pypi.whl_mods().contains_exactly({})

_tests.append(_test_simple)

def _test_build_pipstar_platform(env):
    config = _build_config(
        env,
        module_ctx = _mock_mctx(
            _mod(
                name = "rules_python",
                default = [
                    _default(
                        platform = "myplat",
                        os_name = "linux",
                        arch_name = "x86_64",
                        config_settings = [
                            "@platforms//os:linux",
                            "@platforms//cpu:x86_64",
                        ],
                    ),
                    _default(),
                    _default(
                        platform = "myplat2",
                        os_name = "linux",
                        arch_name = "x86_64",
                        config_settings = [
                            "@platforms//os:linux",
                            "@platforms//cpu:x86_64",
                        ],
                    ),
                    _default(platform = "myplat2"),
                    _default(
                        netrc = "my_netrc",
                        auth_patterns = {"foo": "bar"},
                    ),
                ],
            ),
        ),
        enable_pipstar = True,
    )
    config.auth_patterns().contains_exactly({"foo": "bar"})
    config.netrc().equals("my_netrc")
    config.enable_pipstar().equals(True)
    config.platforms().contains_exactly({
        "myplat": struct(
            name = "myplat",
            os_name = "linux",
            arch_name = "x86_64",
            config_settings = [
                "@platforms//os:linux",
                "@platforms//cpu:x86_64",
            ],
            env = {"implementation_name": "cpython"},
            marker = "",
            whl_abi_tags = ["none", "abi3", "cp{major}{minor}"],
            whl_platform_tags = ["any"],
        ),
    })

_tests.append(_test_build_pipstar_platform)

def extension_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
