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

"""Functionality shared by multiple pieces of code."""

load("@bazel_skylib//lib:types.bzl", "types")
load("//python/private:py_internal.bzl", "py_internal")

def copy_propagating_kwargs(from_kwargs, into_kwargs = None):
    """Copies args that must be compatible between two targets with a dependency relationship.

    This is intended for when one target depends on another, so they must have
    compatible settings such as `testonly` and `compatible_with`. This usually
    happens when a macro generates multiple targets, some of which depend
    on one another, so their settings must be compatible.

    Args:
        from_kwargs: keyword args dict whose common kwarg will be copied.
        into_kwargs: optional keyword args dict that the values from `from_kwargs`
            will be copied into. The values in this dict will take precedence
            over the ones in `from_kwargs` (i.e., if this has `testonly` already
            set, then it won't be overwritten).
            NOTE: THIS WILL BE MODIFIED IN-PLACE.

    Returns:
        Keyword args to use for the depender target derived from the dependency
        target. If `into_kwargs` was passed in, then that same object is
        returned; this is to facilitate easy `**` expansion.
    """
    if into_kwargs == None:
        into_kwargs = {}

    # Include tags because people generally expect tags to propagate.
    for attr in ("testonly", "tags", "compatible_with", "restricted_to", "target_compatible_with"):
        if attr in from_kwargs and attr not in into_kwargs:
            into_kwargs[attr] = from_kwargs[attr]
    return into_kwargs

# The implementation of the macros and tagging mechanism follows the example
# set by rules_cc and rules_java.

def add_tag(attrs, tag):
    """Adds `tag` to `attrs["tags"]`.

    Args:
        attrs: dict of keyword args. It is modified in place.
        tag: str, the tag to add.
    """
    if "tags" in attrs and attrs["tags"] != None:
        tags = attrs["tags"]

        # Preserve the input type: this allows a test verifying the underlying
        # rule can accept the tuple for the tags argument.
        if types.is_tuple(tags):
            attrs["tags"] = tags + (tag,)
        else:
            # List concatenation is necessary because the original value
            # may be a frozen list.
            attrs["tags"] = tags + [tag]
    else:
        attrs["tags"] = [tag]

def is_importable_name(name):
    # Requires Bazel 8+
    if hasattr(py_internal, "regex_match"):
        # ?U means activates unicode matching (Python allows most unicode
        # in module names / identifiers).
        # \w matches alphanumeric and underscore.
        # NOTE: regex_match has an implicit ^ and $
        return py_internal.regex_match(name, "(?U)\\w+")
    else:
        # Otherwise, use a rough hueristic that should catch most cases.
        return (
            "." not in name and
            "-" not in name
        )
