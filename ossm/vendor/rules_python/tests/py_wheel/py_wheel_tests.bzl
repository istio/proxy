# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Test for py_wheel."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:packaging.bzl", "py_wheel")
load("//python/private:py_wheel_normalize_pep440.bzl", "normalize_pep440")  # buildifier: disable=bzl-visibility

_basic_tests = []
_tests = []

def _test_metadata(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_subject",
        distribution = "mydist_" + name,
        version = "0.0.0",
    )
    analysis_test(
        name = name,
        impl = _test_metadata_impl,
        target = name + "_subject",
    )

def _test_metadata_impl(env, target):
    action = env.expect.that_target(target).action_generating(
        "{package}/{name}.metadata.txt",
    )
    action.content().split("\n").contains_exactly([
        env.expect.meta.format_str("Name: mydist_{test_name}"),
        "Metadata-Version: 2.1",
        "",
    ])

_tests.append(_test_metadata)

def _test_data(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_data",
        distribution = "mydist_" + name,
        version = "0.0.0",
        data_files = {
            "source_name": "scripts/wheel_name",
        },
    )
    analysis_test(
        name = name,
        impl = _test_data_impl,
        target = name + "_data",
    )

def _test_data_impl(env, target):
    action = env.expect.that_target(target).action_named(
        "PyWheel",
    )
    action.contains_at_least_args(["--data_files", "scripts/wheel_name;tests/py_wheel/source_name"])
    action.contains_at_least_inputs(["tests/py_wheel/source_name"])

_tests.append(_test_data)

def _test_data_bad_path(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_data",
        distribution = "mydist_" + name,
        version = "0.0.0",
        data_files = {
            "source_name": "unsupported_path/wheel_name",
        },
    )
    analysis_test(
        name = name,
        impl = _test_data_bad_path_impl,
        target = name + "_data",
        expect_failure = True,
    )

def _test_data_bad_path_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("target data file must start with"),
    )

_tests.append(_test_data_bad_path)

def _test_data_bad_path_but_right_prefix(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_data",
        distribution = "mydist_" + name,
        version = "0.0.0",
        data_files = {
            "source_name": "scripts2/wheel_name",
        },
    )
    analysis_test(
        name = name,
        impl = _test_data_bad_path_but_right_prefix_impl,
        target = name + "_data",
        expect_failure = True,
    )

def _test_data_bad_path_but_right_prefix_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("target data file must start with"),
    )

_tests.append(_test_data_bad_path_but_right_prefix)

def _test_content_type_from_attr(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_subject",
        distribution = "mydist_" + name,
        version = "0.0.0",
        description_content_type = "text/x-rst",
    )
    analysis_test(
        name = name,
        impl = _test_content_type_from_attr_impl,
        target = name + "_subject",
    )

def _test_content_type_from_attr_impl(env, target):
    action = env.expect.that_target(target).action_generating(
        "{package}/{name}.metadata.txt",
    )
    action.content().split("\n").contains(
        "Description-Content-Type: text/x-rst",
    )

_tests.append(_test_content_type_from_attr)

def _test_content_type_from_description(name):
    rt_util.helper_target(
        py_wheel,
        name = name + "_subject",
        distribution = "mydist_" + name,
        version = "0.0.0",
        description_file = "desc.md",
    )
    analysis_test(
        name = name,
        impl = _test_content_type_from_description_impl,
        target = name + "_subject",
    )

def _test_content_type_from_description_impl(env, target):
    action = env.expect.that_target(target).action_generating(
        "{package}/{name}.metadata.txt",
    )
    action.content().split("\n").contains(
        "Description-Content-Type: text/markdown",
    )

_tests.append(_test_content_type_from_description)

def _test_pep440_normalization(env):
    prefixes = ["v", "  v", " \t\r\nv"]
    epochs = {
        "": ["", "0!", "00!"],
        "1!": ["1!", "001!"],
        "200!": ["200!", "00200!"],
    }
    releases = {
        "0.1": ["0.1", "0.01"],
        "2023.7.19": ["2023.7.19", "2023.07.19"],
    }
    pres = {
        "": [""],
        "a0": ["a", ".a", "-ALPHA0", "_alpha0", ".a0"],
        "a4": ["alpha4", ".a04"],
        "b0": ["b", ".b", "-BETA0", "_beta0", ".b0"],
        "b5": ["beta05", ".b5"],
        "rc0": ["C", "_c0", "RC", "_rc0", "-preview_0"],
    }
    explicit_posts = {
        "": [""],
        ".post0": [],
        ".post1": [".post1", "-r1", "_rev1"],
    }
    implicit_posts = [[".post1", "-1"], [".post2", "-2"]]
    devs = {
        "": [""],
        ".dev0": ["dev", "-DEV", "_Dev-0"],
        ".dev9": ["DEV9", ".dev09", ".dev9"],
        ".dev{BUILD_TIMESTAMP}": [
            "-DEV{BUILD_TIMESTAMP}",
            "_dev_{BUILD_TIMESTAMP}",
        ],
    }
    locals = {
        "": [""],
        "+ubuntu.7": ["+Ubuntu_7", "+ubuntu-007"],
        "+ubuntu.r007": ["+Ubuntu_R007"],
    }
    epochs = [
        [normalized_epoch, input_epoch]
        for normalized_epoch, input_epochs in epochs.items()
        for input_epoch in input_epochs
    ]
    releases = [
        [normalized_release, input_release]
        for normalized_release, input_releases in releases.items()
        for input_release in input_releases
    ]
    pres = [
        [normalized_pre, input_pre]
        for normalized_pre, input_pres in pres.items()
        for input_pre in input_pres
    ]
    explicit_posts = [
        [normalized_post, input_post]
        for normalized_post, input_posts in explicit_posts.items()
        for input_post in input_posts
    ]
    pres_and_posts = [
        [normalized_pre + normalized_post, input_pre + input_post]
        for normalized_pre, input_pre in pres
        for normalized_post, input_post in explicit_posts
    ] + [
        [normalized_pre + normalized_post, input_pre + input_post]
        for normalized_pre, input_pre in pres
        for normalized_post, input_post in implicit_posts
        if input_pre == "" or input_pre[-1].isdigit()
    ]
    devs = [
        [normalized_dev, input_dev]
        for normalized_dev, input_devs in devs.items()
        for input_dev in input_devs
    ]
    locals = [
        [normalized_local, input_local]
        for normalized_local, input_locals in locals.items()
        for input_local in input_locals
    ]
    postfixes = ["", "  ", " \t\r\n"]
    i = 0
    for nepoch, iepoch in epochs:
        for nrelease, irelease in releases:
            for nprepost, iprepost in pres_and_posts:
                for ndev, idev in devs:
                    for nlocal, ilocal in locals:
                        prefix = prefixes[i % len(prefixes)]
                        postfix = postfixes[(i // len(prefixes)) % len(postfixes)]
                        env.expect.that_str(
                            normalize_pep440(
                                prefix + iepoch + irelease + iprepost +
                                idev + ilocal + postfix,
                            ),
                        ).equals(
                            nepoch + nrelease + nprepost + ndev + nlocal,
                        )
                        i += 1

_basic_tests.append(_test_pep440_normalization)

def py_wheel_test_suite(name):
    test_suite(
        name = name,
        basic_tests = _basic_tests,
        tests = _tests,
    )
