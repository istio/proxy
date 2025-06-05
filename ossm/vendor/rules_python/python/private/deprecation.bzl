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

"""Helper functions to deprecation utilities.
"""

load("@rules_python_internal//:rules_python_config.bzl", "config")

_DEPRECATION_MESSAGE = """
The '{name}' symbol in '{old_load}'
is deprecated. It is an alias to the regular rule; use it directly instead:

load("{new_load}", "{name}")

{snippet}
"""

def _symbol(kwargs, *, symbol_name, new_load, old_load, snippet = ""):
    """An internal function to propagate the deprecation warning.

    This is not an API that should be used outside `rules_python`.

    Args:
        kwargs: Arguments to modify.
        symbol_name: {type}`str` the symbol name that is deprecated.
        new_load: {type}`str` the new load location under `//`.
        old_load: {type}`str` the symbol import location that we are deprecating.
        snippet: {type}`str` the usage snippet of the new symbol.

    Returns:
        The kwargs to be used in the macro creation.
    """

    if config.enable_deprecation_warnings:
        deprecation = _DEPRECATION_MESSAGE.format(
            name = symbol_name,
            old_load = old_load,
            new_load = new_load,
            snippet = snippet,
        )
        if kwargs.get("deprecation"):
            deprecation = kwargs.get("deprecation") + "\n\n" + deprecation
        kwargs["deprecation"] = deprecation
    return kwargs

with_deprecation = struct(
    symbol = _symbol,
)
