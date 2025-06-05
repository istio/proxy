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
load("@rules_python_internal//:rules_python_config.bzl", "config")

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
    for attr in ("testonly", "tags", "compatible_with", "restricted_to"):
        if attr in from_kwargs and attr not in into_kwargs:
            into_kwargs[attr] = from_kwargs[attr]
    return into_kwargs

# The implementation of the macros and tagging mechanism follows the example
# set by rules_cc and rules_java.

_MIGRATION_TAG = "__PYTHON_RULES_MIGRATION_DO_NOT_USE_WILL_BREAK__"

def add_migration_tag(attrs):
    """Add a special tag to `attrs` to aid migration off native rles.

    Args:
        attrs: dict of keyword args. The `tags` key will be modified in-place.

    Returns:
        The same `attrs` object, but modified.
    """
    if not config.enable_pystar:
        add_tag(attrs, _MIGRATION_TAG)
    return attrs

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

# Helper to make the provider definitions not crash under Bazel 5.4:
# Bazel 5.4 doesn't support the `init` arg of `provider()`, so we have to
# not pass that when using Bazel 5.4. But, not passing the `init` arg
# changes the return value from a two-tuple to a single value, which then
# breaks Bazel 6+ code.
# This isn't actually used under Bazel 5.4, so just stub out the values
# to get past the loading phase.
def define_bazel_6_provider(doc, fields, **kwargs):
    """Define a provider, or a stub for pre-Bazel 7."""
    if not IS_BAZEL_6_OR_HIGHER:
        return provider("Stub, not used", fields = []), None
    return provider(doc = doc, fields = fields, **kwargs)

IS_BAZEL_7_4_OR_HIGHER = hasattr(native, "legacy_globals")

IS_BAZEL_7_OR_HIGHER = hasattr(native, "starlark_doc_extract")

# Bazel 5.4 has a bug where every access of testing.ExecutionInfo is a
# different object that isn't equal to any other. This is fixed in bazel 6+.
IS_BAZEL_6_OR_HIGHER = testing.ExecutionInfo == testing.ExecutionInfo

_marker_rule_to_detect_bazel_6_4_or_higher = rule(implementation = lambda ctx: None)

# Bazel 6.4 and higher have a bug fix where rule names show up in the str()
# of a rule. See
# https://github.com/bazelbuild/bazel/commit/002490b9a2376f0b2ea4a37102c5e94fc50a65ba
# https://github.com/bazelbuild/bazel/commit/443cbcb641e17f7337ccfdecdfa5e69bc16cae55
# This technique is done instead of using native.bazel_version because,
# under stardoc, the native.bazel_version attribute is entirely missing, which
# prevents doc generation from being able to correctly generate docs.
IS_BAZEL_6_4_OR_HIGHER = "_marker_rule_to_detect_bazel_6_4_or_higher" in str(
    _marker_rule_to_detect_bazel_6_4_or_higher,
)
