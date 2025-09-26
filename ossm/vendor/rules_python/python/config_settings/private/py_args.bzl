# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""A helper to extract default args for the transition rule."""

def py_args(name, kwargs):
    """A helper to extract common py_binary and py_test args

    See https://bazel.build/reference/be/python#py_binary and
    https://bazel.build/reference/be/python#py_test for the list
    that should be returned

    Args:
        name: The name of the target.
        kwargs: The kwargs to be extracted from; MODIFIED IN-PLACE.

    Returns:
        A dict with the extracted arguments
    """
    return dict(
        args = kwargs.pop("args", None),
        data = kwargs.pop("data", None),
        env = kwargs.pop("env", None),
        srcs = kwargs.pop("srcs", None),
        deps = kwargs.pop("deps", None),
        # See https://bazel.build/reference/be/python#py_binary.main
        # for default logic.
        # NOTE: This doesn't match the exact way a regular py_binary searches for
        # it's main amongst the srcs, but is close enough for most cases.
        main = kwargs.pop("main", name + ".py"),
    )
