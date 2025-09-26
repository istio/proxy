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
load("//python/private:repo_utils.bzl", "REPO_DEBUG_ENV_VAR", "REPO_VERBOSITY_ENV_VAR", "repo_utils")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:evaluate_markers.bzl", "evaluate_markers")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:parse_requirements.bzl", "select_requirement", _parse_requirements = "parse_requirements")  # buildifier: disable=bzl-visibility
load("//python/private/pypi:pep508_env.bzl", pep508_env = "env")  # buildifier: disable=bzl-visibility

def _mock_ctx():
    testdata = {
        "requirements_different_package_version": """\
foo==0.0.1+local \
    --hash=sha256:deadbeef
foo==0.0.1 \
    --hash=sha256:deadb00f
""",
        "requirements_direct": """\
foo[extra] @ https://some-url/package.whl
""",
        "requirements_direct_sdist": """
foo @ https://github.com/org/foo/downloads/foo-1.1.tar.gz
""",
        "requirements_extra_args": """\
--index-url=example.org

foo[extra]==0.0.1 \
    --hash=sha256:deadbeef
""",
        "requirements_git": """
foo @ git+https://github.com/org/foo.git@deadbeef
""",
        "requirements_linux": """\
foo==0.0.3 --hash=sha256:deadbaaf --hash=sha256:5d15t
""",
        # download_only = True
        "requirements_linux_download_only": """\
--platform=manylinux_2_17_x86_64
--python-version=39
--implementation=cp
--abi=cp39

foo==0.0.1 --hash=sha256:deadbeef
bar==0.0.1 --hash=sha256:deadb00f
""",
        "requirements_lock": """\
foo[extra]==0.0.1 --hash=sha256:deadbeef
""",
        "requirements_lock_dupe": """\
foo[extra,extra_2]==0.0.1 --hash=sha256:deadbeef
foo==0.0.1 --hash=sha256:deadbeef
foo[extra]==0.0.1 --hash=sha256:deadbeef
""",
        "requirements_marker": """\
foo[extra]==0.0.1 ;marker --hash=sha256:deadbeef
bar==0.0.1 --hash=sha256:deadbeef
""",
        "requirements_multi_version": """\
foo==0.0.1; python_full_version < '3.10.0' \
    --hash=sha256:deadbeef
foo==0.0.2; python_full_version >= '3.10.0' \
    --hash=sha256:deadb11f
""",
        "requirements_optional_hash": """
foo==0.0.4 @ https://example.org/foo-0.0.4.whl
foo==0.0.5 @ https://example.org/foo-0.0.5.whl --hash=sha256:deadbeef
""",
        "requirements_osx": """\
foo==0.0.3 --hash=sha256:deadbaaf --hash=sha256:deadb11f --hash=sha256:5d15t
""",
        "requirements_osx_download_only": """\
--platform=macosx_10_9_arm64
--python-version=39
--implementation=cp
--abi=cp39

foo==0.0.3 --hash=sha256:deadbaaf
""",
        "requirements_windows": """\
foo[extra]==0.0.2 --hash=sha256:deadbeef
bar==0.0.1 --hash=sha256:deadb00f
""",
    }

    return struct(
        os = struct(
            name = "linux",
            arch = "x86_64",
        ),
        read = lambda x: testdata[x],
    )

_tests = []

def parse_requirements(debug = False, **kwargs):
    return _parse_requirements(
        ctx = _mock_ctx(),
        logger = repo_utils.logger(struct(
            os = struct(
                environ = {
                    REPO_DEBUG_ENV_VAR: "1",
                    REPO_VERBOSITY_ENV_VAR: "TRACE" if debug else "INFO",
                },
            ),
        ), "unit-test"),
        **kwargs
    )

def _test_simple(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_lock": ["linux_x86_64", "windows_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo[extra]==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = [
                        "linux_x86_64",
                        "windows_x86_64",
                    ],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_simple)

def _test_direct_urls_integration(env):
    """Check that we are using the filename from index_sources."""
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_direct": ["linux_x86_64"],
            "requirements_direct_sdist": ["osx_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = True,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "foo-1.1.tar.gz",
                    requirement_line = "foo @ https://github.com/org/foo/downloads/foo-1.1.tar.gz",
                    sha256 = "",
                    target_platforms = ["osx_x86_64"],
                    url = "https://github.com/org/foo/downloads/foo-1.1.tar.gz",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "package.whl",
                    requirement_line = "foo[extra]",
                    sha256 = "",
                    target_platforms = ["linux_x86_64"],
                    url = "https://some-url/package.whl",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_direct_urls_integration)

def _test_extra_pip_args(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_extra_args": ["linux_x86_64"],
        },
        extra_pip_args = ["--trusted-host=example.org"],
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = ["--index-url=example.org", "--trusted-host=example.org"],
                    requirement_line = "foo[extra]==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = [
                        "linux_x86_64",
                    ],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_extra_pip_args)

def _test_dupe_requirements(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_lock_dupe": ["linux_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo[extra,extra_2]==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = ["linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_dupe_requirements)

def _test_multi_os(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_linux": ["linux_x86_64"],
            "requirements_windows": ["windows_x86_64"],
        },
    )

    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "bar",
            is_exposed = False,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "bar",
                    extra_pip_args = [],
                    requirement_line = "bar==0.0.1 --hash=sha256:deadb00f",
                    target_platforms = ["windows_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = True,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo==0.0.3 --hash=sha256:deadbaaf --hash=sha256:5d15t",
                    target_platforms = ["linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo[extra]==0.0.2 --hash=sha256:deadbeef",
                    target_platforms = ["windows_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])
    env.expect.that_str(
        select_requirement(
            got[1].srcs,
            platform = "windows_x86_64",
        ).requirement_line,
    ).equals("foo[extra]==0.0.2 --hash=sha256:deadbeef")

_tests.append(_test_multi_os)

def _test_multi_os_legacy(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_linux_download_only": ["cp39_linux_x86_64"],
            "requirements_osx_download_only": ["cp39_osx_aarch64"],
        },
    )

    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "bar",
            is_exposed = False,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "bar",
                    extra_pip_args = ["--platform=manylinux_2_17_x86_64", "--python-version=39", "--implementation=cp", "--abi=cp39"],
                    requirement_line = "bar==0.0.1 --hash=sha256:deadb00f",
                    target_platforms = ["cp39_linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = True,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = ["--platform=manylinux_2_17_x86_64", "--python-version=39", "--implementation=cp", "--abi=cp39"],
                    requirement_line = "foo==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = ["cp39_linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = ["--platform=macosx_10_9_arm64", "--python-version=39", "--implementation=cp", "--abi=cp39"],
                    requirement_line = "foo==0.0.3 --hash=sha256:deadbaaf",
                    target_platforms = ["cp39_osx_aarch64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_multi_os_legacy)

def _test_select_requirement_none_platform(env):
    got = select_requirement(
        [
            struct(
                some_attr = "foo",
                target_platforms = ["linux_x86_64"],
            ),
        ],
        platform = None,
    )
    env.expect.that_str(got.some_attr).equals("foo")

_tests.append(_test_select_requirement_none_platform)

def _test_env_marker_resolution(env):
    def _mock_eval_markers(_, input):
        ret = {
            "foo[extra]==0.0.1 ;marker --hash=sha256:deadbeef": ["cp311_windows_x86_64"],
        }

        env.expect.that_collection(input.keys()).contains_exactly(ret.keys())
        env.expect.that_collection(input.values()[0]).contains_exactly(["cp311_linux_super_exotic", "cp311_windows_x86_64"])
        return ret

    got = parse_requirements(
        requirements_by_platform = {
            "requirements_marker": ["cp311_linux_super_exotic", "cp311_windows_x86_64"],
        },
        evaluate_markers = _mock_eval_markers,
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "bar",
            is_exposed = True,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "bar",
                    extra_pip_args = [],
                    requirement_line = "bar==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = ["cp311_linux_super_exotic", "cp311_windows_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
        struct(
            name = "foo",
            is_exposed = False,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo[extra]==0.0.1 --hash=sha256:deadbeef",
                    target_platforms = ["cp311_windows_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_env_marker_resolution)

def _test_different_package_version(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_different_package_version": ["linux_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = True,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo==0.0.1 --hash=sha256:deadb00f",
                    target_platforms = ["linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo==0.0.1+local --hash=sha256:deadbeef",
                    target_platforms = ["linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_different_package_version)

def _test_optional_hash(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_optional_hash": ["linux_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = True,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo==0.0.4",
                    target_platforms = ["linux_x86_64"],
                    url = "https://example.org/foo-0.0.4.whl",
                    filename = "foo-0.0.4.whl",
                    sha256 = "",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo==0.0.5",
                    target_platforms = ["linux_x86_64"],
                    url = "https://example.org/foo-0.0.5.whl",
                    filename = "foo-0.0.5.whl",
                    sha256 = "deadbeef",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_optional_hash)

def _test_git_sources(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_git": ["linux_x86_64"],
        },
    )
    env.expect.that_collection(got).contains_exactly([
        struct(
            name = "foo",
            is_exposed = True,
            is_multiple_versions = False,
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    requirement_line = "foo @ git+https://github.com/org/foo.git@deadbeef",
                    target_platforms = ["linux_x86_64"],
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_git_sources)

def _test_overlapping_shas_with_index_results(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_linux": ["cp39_linux_x86_64"],
            "requirements_osx": ["cp39_osx_x86_64"],
        },
        platforms = {
            "cp39_linux_x86_64": struct(
                env = pep508_env(
                    python_version = "3.9.0",
                    os = "linux",
                    arch = "x86_64",
                ),
                whl_abi_tags = ["none"],
                whl_platform_tags = ["any"],
            ),
            "cp39_osx_x86_64": struct(
                env = pep508_env(
                    python_version = "3.9.0",
                    os = "osx",
                    arch = "x86_64",
                ),
                whl_abi_tags = ["none"],
                whl_platform_tags = ["macosx_*_x86_64"],
            ),
        },
        get_index_urls = lambda _, __: {
            "foo": struct(
                sdists = {
                    "5d15t": struct(
                        url = "sdist",
                        sha256 = "5d15t",
                        filename = "foo-0.0.1.tar.gz",
                        yanked = False,
                    ),
                },
                whls = {
                    "deadb11f": struct(
                        url = "super2",
                        sha256 = "deadb11f",
                        filename = "foo-0.0.1-py3-none-macosx_14_0_x86_64.whl",
                        yanked = False,
                    ),
                    "deadbaaf": struct(
                        url = "super2",
                        sha256 = "deadbaaf",
                        filename = "foo-0.0.1-py3-none-any.whl",
                        yanked = False,
                    ),
                },
            ),
        },
    )

    env.expect.that_collection(got).contains_exactly([
        struct(
            is_exposed = True,
            is_multiple_versions = True,
            name = "foo",
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "foo-0.0.1-py3-none-any.whl",
                    requirement_line = "foo==0.0.3",
                    sha256 = "deadbaaf",
                    target_platforms = ["cp39_linux_x86_64"],
                    url = "super2",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "foo-0.0.1-py3-none-macosx_14_0_x86_64.whl",
                    requirement_line = "foo==0.0.3",
                    sha256 = "deadb11f",
                    target_platforms = ["cp39_osx_x86_64"],
                    url = "super2",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_overlapping_shas_with_index_results)

def _test_get_index_urls_different_versions(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_multi_version": [
                "cp39_linux_x86_64",
                "cp310_linux_x86_64",
            ],
        },
        platforms = {
            "cp310_linux_x86_64": struct(
                env = pep508_env(
                    python_version = "3.10.0",
                    os = "linux",
                    arch = "x86_64",
                ),
                whl_abi_tags = ["none"],
                whl_platform_tags = ["any"],
            ),
            "cp39_linux_x86_64": struct(
                env = pep508_env(
                    python_version = "3.9.0",
                    os = "linux",
                    arch = "x86_64",
                ),
                whl_abi_tags = ["none"],
                whl_platform_tags = ["any"],
            ),
        },
        get_index_urls = lambda _, __: {
            "foo": struct(
                sdists = {},
                whls = {
                    "deadb11f": struct(
                        url = "super2",
                        sha256 = "deadb11f",
                        filename = "foo-0.0.2-py3-none-any.whl",
                        yanked = False,
                    ),
                    "deadbaaf": struct(
                        url = "super2",
                        sha256 = "deadbaaf",
                        filename = "foo-0.0.1-py3-none-any.whl",
                        yanked = False,
                    ),
                },
            ),
        },
        evaluate_markers = lambda _, requirements: evaluate_markers(
            requirements = requirements,
            platforms = {
                "cp310_linux_x86_64": struct(
                    env = {"python_full_version": "3.10.0"},
                ),
                "cp39_linux_x86_64": struct(
                    env = {"python_full_version": "3.9.0"},
                ),
            },
        ),
        debug = True,
    )

    env.expect.that_collection(got).contains_exactly([
        struct(
            is_exposed = True,
            is_multiple_versions = True,
            name = "foo",
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "",
                    requirement_line = "foo==0.0.1 --hash=sha256:deadbeef",
                    sha256 = "",
                    target_platforms = ["cp39_linux_x86_64"],
                    url = "",
                    yanked = False,
                ),
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "foo-0.0.2-py3-none-any.whl",
                    requirement_line = "foo==0.0.2",
                    sha256 = "deadb11f",
                    target_platforms = ["cp310_linux_x86_64"],
                    url = "super2",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_get_index_urls_different_versions)

def _test_get_index_urls_single_py_version(env):
    got = parse_requirements(
        requirements_by_platform = {
            "requirements_multi_version": [
                "cp310_linux_x86_64",
            ],
        },
        platforms = {
            "cp310_linux_x86_64": struct(
                env = pep508_env(
                    python_version = "3.10.0",
                    os = "linux",
                    arch = "x86_64",
                ),
                whl_abi_tags = ["none"],
                whl_platform_tags = ["any"],
            ),
        },
        get_index_urls = lambda _, __: {
            "foo": struct(
                sdists = {},
                whls = {
                    "deadb11f": struct(
                        url = "super2",
                        sha256 = "deadb11f",
                        filename = "foo-0.0.2-py3-none-any.whl",
                        yanked = False,
                    ),
                },
            ),
        },
        evaluate_markers = lambda _, requirements: evaluate_markers(
            requirements = requirements,
            platforms = {
                "cp310_linux_x86_64": struct(
                    env = {"python_full_version": "3.10.0"},
                ),
            },
        ),
        debug = True,
    )

    env.expect.that_collection(got).contains_exactly([
        struct(
            is_exposed = True,
            is_multiple_versions = True,
            name = "foo",
            srcs = [
                struct(
                    distribution = "foo",
                    extra_pip_args = [],
                    filename = "foo-0.0.2-py3-none-any.whl",
                    requirement_line = "foo==0.0.2",
                    sha256 = "deadb11f",
                    target_platforms = ["cp310_linux_x86_64"],
                    url = "super2",
                    yanked = False,
                ),
            ],
        ),
    ])

_tests.append(_test_get_index_urls_single_py_version)

def parse_requirements_test_suite(name):
    """Create the test suite.

    Args:
        name: the name of the test suite
    """
    test_suite(name = name, basic_tests = _tests)
