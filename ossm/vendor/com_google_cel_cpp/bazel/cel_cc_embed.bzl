# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Provides the `cel_cc_embed` build rule.
"""

def cel_cc_embed(name, src, testonly = False):
    native.genrule(
        name = name,
        srcs = [src],
        outs = ["{}.inc".format(name)],
        cmd = "$(location //bazel:cel_cc_embed) --in=$< --out=$@",
        tools = ["//bazel:cel_cc_embed"],
        testonly = testonly,
    )
