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

"""Rule to test that the content of an archive has particular properties.

This is available for integration testing, when people want to verify that all
the files they expect are in an archive. Or possibly, they want to verify that
some files do not appear.

The execution time is O(# expected patterns * size of archive).
"""

load("@rules_python//python:defs.bzl", "py_test")

# Attribute names common to all build rules. See https://bazel.build/reference/be/common-definitions
COMMON_BUILD_ATTR_NAMES = [
    "tags",
    "target_compatible_with",
    "testonly",
    "visibility",
]

# Attribute names common to all test rules. See https://bazel.build/reference/be/common-definitions#common-attributes-tests
COMMON_TEST_ATTR_NAMES = COMMON_BUILD_ATTR_NAMES + [
    "size",
    "timeout",
    "flaky",
]

def _gen_verify_archive_test_main_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file._template,
        output = ctx.outputs.out,
        # @unsorted-dict-items
        substitutions = {
            "${TEST_NAME}": ctx.attr.test_name,
            "${TARGET}": ctx.files.target[0].short_path,
            "${MUST_CONTAIN}": str(ctx.attr.must_contain),
            "${MUST_CONTAIN_REGEX}": str(ctx.attr.must_contain_regex),
            "${MUST_NOT_CONTAIN}": str(ctx.attr.must_not_contain),
            "${MUST_NOT_CONTAIN_REGEX}": str(ctx.attr.must_not_contain_regex),
            "${MIN_SIZE}": str(ctx.attr.min_size),
            "${MAX_SIZE}": str(ctx.attr.max_size),
            "${VERIFY_LINKS}": str(ctx.attr.verify_links),
        },
    )
    return [
        DefaultInfo(files = depset([ctx.outputs.out])),
    ]

_gen_verify_archive_test_main = rule(
    implementation = _gen_verify_archive_test_main_impl,
    # @unsorted-dict-items
    attrs = {
        "out": attr.output(mandatory = True),
        "test_name": attr.string(mandatory = True),
        "target": attr.label(
            doc = "Archive to test",
            allow_single_file = True,
            mandatory = True,
        ),
        "must_contain": attr.string_list(
            doc = "List of paths which all must appear in the archive.",
        ),
        "must_contain_regex": attr.string_list(
            doc = "List of regexes which all must appear in the archive.",
        ),
        "must_not_contain": attr.string_list(
            doc = """List of paths that must not be in the archive.""",
        ),
        "must_not_contain_regex": attr.string_list(
            doc = """List of regexes that must not be in the archive.""",
        ),
        "min_size": attr.int(
            doc = """Minimum number of entries in the archive.""",
        ),
        "max_size": attr.int(
            doc = """Maximum number of entries in the archive.""",
        ),
        "verify_links": attr.string_dict(
            doc = """Dict keyed by paths which must appear, and be symlinks to their values.""",
        ),

        # Implicit dependencies.
        "_template": attr.label(
            default = Label("//pkg:verify_archive_test_main.py.tpl"),
            allow_single_file = True,
        ),
    },
)

# buildifier: disable=function-docstring-args
def verify_archive_test(
        name,
        target,
        must_contain = None,
        must_contain_regex = None,
        must_not_contain = None,
        must_not_contain_regex = None,
        min_size = 1,
        max_size = -1,
        verify_links = None,
        **kwargs):
    """Tests that an archive contains specific file patterns.

    This test is used to verify that an archive contains the expected content.

    Args:
      target: A target archive.
      must_contain: A list of paths which must appear in the archive.
      must_contain_regex: A list of path regexes which must appear in the archive.
      must_not_contain: A list of paths which must not appear in the archive.
      must_not_contain_regex: A list of path regexes which must not appear in the archive.
      min_size: The minimum number of entries which must be in the archive.
      max_size: The maximum number of entries which must be in the archive.
      verify_links: Dict keyed by paths which must appear, and be symlinks to their values.
      **kwargs: The args to be passed to the underlying rules, if supported.
                See https://github.com/bazelbuild/rules_pkg/blob/main/pkg/verify_archive.bzl for the full list.
    """
    test_src = name + "__internal_main.py"
    _gen_verify_archive_test_main(
        name = name + "_internal_main",
        target = target,
        test_name = name.replace("-", "_") + "Test",
        out = test_src,
        must_contain = must_contain,
        must_contain_regex = must_contain_regex,
        must_not_contain = must_not_contain,
        must_not_contain_regex = must_not_contain_regex,
        min_size = min_size,
        max_size = max_size,
        verify_links = verify_links,
        **{key: kwargs[key] for key in COMMON_BUILD_ATTR_NAMES if key in kwargs}
    )
    py_test(
        name = name,
        srcs = [":" + test_src],
        main = test_src,
        data = [target],
        python_version = "PY3",
        **{key: kwargs[key] for key in COMMON_TEST_ATTR_NAMES if key in kwargs}
    )
