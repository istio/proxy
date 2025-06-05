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
"""Implementation of py_api."""

_PY_COMMON_API_LABEL = Label("//python/private/api:py_common_api")

ApiImplInfo = provider(
    doc = "Provider to hold an API implementation",
    fields = {
        "impl": """
:type: struct

The implementation of the API being provided. The object it contains
will depend on the target that is providing the API struct.
""",
    },
)

def _py_common_get(ctx):
    """Get the py_common API instance.

    NOTE: to use this function, the rule must have added `py_common.API_ATTRS`
    to its attributes.

    Args:
        ctx: {type}`ctx` current rule ctx

    Returns:
        {type}`PyCommonApi`
    """

    # A generic provider is used to decouple the API implementations from
    # the loading phase of the rules using an implementation.
    return ctx.attr._py_common_api[ApiImplInfo].impl

py_common = struct(
    get = _py_common_get,
    API_ATTRS = {
        "_py_common_api": attr.label(
            default = _PY_COMMON_API_LABEL,
            providers = [ApiImplInfo],
        ),
    },
)
