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
"""A few small helpers for working with Markdown."""

def markdown_link(link_text, href):
    """Creates a markdown link.

    Args:
      link_text: The text to display for the link.
      href: The href for the link.

    Returns:
      A markdown link.
    """
    return "[" + link_text + "](" + href + ")"

def xref_substitutions(match_text_patterns):
    """Creates a dictionary of substitutions for use for linkification of text.

    Example:
    ```
    # Produces a dictionary containing:
    #   {
    #     "foo": "[foo](http://foo.com)"
    #     "bar": "[bar](http://bar.com)"
    #   }
    substitutions = xref_substitutions({
        "foo": "http://foo.com",
        "bar": "http://bar.com",
    })
    ```

    Args:
      match_text_patterns: A dictionary mapping string literals to the links they should point to.

    Returns:
      A dictionary of string literals mapped to their linkified substitutions.
    """
    return {
        match_text: markdown_link(match_text, href)
        for match_text, href in match_text_patterns.items()
    }
