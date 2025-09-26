# Copyright 2025 The Bazel Authors. All rights reserved.
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

load("@bazel_skylib//rules:native_binary.bzl", "native_test")
load("//python/uv:lock.bzl", "lock")
load("//tests/support:py_reconfig.bzl", "py_reconfig_test")

def lock_test_suite(name):
    """The test suite with various lock-related integration tests

    Args:
        name: {type}`str` the name of the test suite
    """
    lock(
        name = "requirements",
        srcs = ["testdata/requirements.in"],
        constraints = [
            "testdata/constraints.txt",
            "testdata/constraints2.txt",
        ],
        build_constraints = [
            "testdata/build_constraints.txt",
            "testdata/build_constraints2.txt",
        ],
        # It seems that the CI remote executors for the RBE do not have network
        # connectivity due to current CI setup.
        tags = ["no-remote-exec"],
        out = "testdata/requirements.txt",
    )

    lock(
        name = "requirements_new_file",
        srcs = ["testdata/requirements.in"],
        out = "does_not_exist.txt",
        # It seems that the CI remote executors for the RBE do not have network
        # connectivity due to current CI setup.
        tags = ["no-remote-exec"],
    )

    py_reconfig_test(
        name = "requirements_run_tests",
        env = {
            "BUILD_WORKSPACE_DIRECTORY": "foo",
        },
        srcs = ["lock_run_test.py"],
        deps = [
            "//python/runfiles",
        ],
        data = [
            "requirements_new_file.update",
            "requirements_new_file.run",
            "requirements.update",
            "requirements.run",
            "testdata/requirements.txt",
        ],
        main = "lock_run_test.py",
        tags = [
            "requires-network",
            # FIXME @aignas 2025-03-19: it seems that the RBE tests are failing
            # to execute the `requirements.run` targets that require network.
            #
            # We could potentially dump the required `.html` files and somehow
            # provide it to the `uv`, but may rely on internal uv handling of
            # `--index-url`.
            "no-remote-exec",
        ],
        # FIXME @aignas 2025-03-19: It seems that currently:
        # 1. The Windows runners are not compatible with the `uv` Windows binaries.
        # 2. The Python launcher is having trouble launching scripts from within the Python test.
        target_compatible_with = select({
            "@platforms//os:windows": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
    )

    # document and check that this actually works
    native_test(
        name = "requirements_test",
        src = ":requirements.update",
        target_compatible_with = select({
            "@platforms//os:windows": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
    )

    native.test_suite(
        name = name,
        tests = [
            ":requirements_test",
            ":requirements_run_tests",
        ],
    )
