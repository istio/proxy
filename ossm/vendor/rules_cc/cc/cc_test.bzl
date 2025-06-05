# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""cc_test rule"""

# TODO(bazel-team): To avoid breaking changes, if the below are no longer
# forwarding to native rules, flag @bazel_tools//tools/cpp:link_extra_libs
# should either: (a) alias the flag @rules_cc//:link_extra_libs, or (b) be
# added as a dependency to @rules_cc//:link_extra_lib. The intermediate library
# @bazel_tools//tools/cpp:link_extra_lib should either be added as a dependency
# to @rules_cc//:link_extra_lib, or removed entirely (if possible).
_LINK_EXTRA_LIB = Label("//:link_extra_lib")

def cc_test(**attrs):
    """Bazel cc_test rule.

    https://docs.bazel.build/versions/main/be/c-cpp.html#cc_test

    Args:
      **attrs: Rule attributes
    """

    is_library = "linkshared" in attrs and attrs["linkshared"]

    # Executable builds also include the "link_extra_lib" library.
    if not is_library:
        if "deps" in attrs and attrs["deps"] != None:
            attrs["deps"] = attrs["deps"] + [_LINK_EXTRA_LIB]
        else:
            attrs["deps"] = [_LINK_EXTRA_LIB]

    # buildifier: disable=native-cc-test
    native.cc_test(**attrs)
