# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Rules for testing the providers of a target under test."""

load("@bazel_skylib//lib:types.bzl", "types")
load(
    "@bazel_skylib//lib:unittest.bzl",
    "analysistest",
    "unittest",
)
load("//swift:providers.bzl", "SwiftInfo")
load(
    "//test/rules:expected_files.bzl",
    "compare_expected_files",
    "normalize_collection",
)

# A sentinel value returned by `_evaluate_field` when a `None` value is
# encountered during the evaluation of a dotted path on any component other than
# the last component. This allows the caller to distinguish between a legitimate
# `None` value being returned by the entire path vs. an unexpected `None` in an
# earlier component.
#
# A `provider` is used here because it is a simple way of getting a known unique
# object from Bazel that cannot be equal to any other object.
_EVALUATE_FIELD_FAILED = provider(
    doc = "Sentinel value, not otherwise used.",
    fields = {},
)

def _evaluate_field(env, source, field):
    """Evaluates a field or field path on an object and returns its value.

    This function projects across collections. That is, if the result of
    evaluating a field along the path is a depset or a list, then the result
    will be normalized into a list and remaining fields in the path will be
    evaluated on every item in that list, not on the list itself.

    If a field path component in a projected collection is followed by an
    exclamation point, then this indicates that any `None` values produced at
    that stage of evaluation should be removed from the list before continuing.
    If evaluating the path fails because a `None` value is encountered anywhere
    before the last component and they are not filtered out, then an assertion
    failure is logged and the special value `EVALUATE_FIELD_FAILED` is returned.
    This value lets the caller short-circuit additional test logic that may not
    be relevant if evaluation is known to have failed.

    Args:
        env: The analysis test environment.
        source: The source object on which to evaluate the field or field path.
        field: The field or field path to evaluate. This can be a simple field
            name or a dotted path.

    Returns:
        The result of evaluating the field or field path on the source object.
        If a `None` value was encountered during evaluation of a field path
        component that was not the final component, then the special value
        `_EVALUATE_FIELD_FAILED` is returned.
    """

    def evaluate_component(source, component):
        if types.is_dict(source):
            return source.get(component)
        return getattr(source, component, None)

    components = field.split(".")

    for component in components:
        source = normalize_collection(source)
        filter_nones = component.endswith("!")
        if filter_nones:
            component = component[:-1]

        if types.is_list(source):
            if any([item == None for item in source]):
                unittest.fail(
                    env,
                    "Got 'None' evaluating '{}' on an element in '{}'.".format(
                        component,
                        field,
                    ),
                )
                return _EVALUATE_FIELD_FAILED

            # If the elements are lists or depsets, flatten the whole thing into
            # a single list.
            flattened = []
            for item in source:
                item = normalize_collection(item)
                if types.is_list(item):
                    flattened.extend(item)
                else:
                    flattened.append(item)
            source = [evaluate_component(item, component) for item in flattened]
            if filter_nones:
                source = [item for item in source if item != None]
        else:
            if source == None:
                unittest.fail(
                    env,
                    "Got 'None' evaluating '{}' in '{}'.".format(
                        component,
                        field,
                    ),
                )
                return _EVALUATE_FIELD_FAILED

            source = evaluate_component(source, component)
            if filter_nones:
                source = normalize_collection(source)
                if types.is_list(source):
                    source = [item for item in source if item != None]
                else:
                    unittest.fail(
                        env,
                        ("Expected to filter 'None' values evaluating '{}' " +
                         "on an element in '{}', but the result was not a " +
                         "collection.").format(component, field),
                    )
                    return _EVALUATE_FIELD_FAILED

    return source

def _lookup_provider_by_name(env, target, provider_name):
    """Returns a provider on a target, given its name.

    The `provider_test` rule needs to be able to specify which provider a field
    should be looked up on, but it can't take provider objects directly as
    attribute values, so we have to use strings and a fixed lookup table to find
    them.

    If the provider is not recognized or is not propagated by the target, then
    an assertion failure is logged and `None` is returned. This lets the caller
    short-circuit additional test logic that may not be relevant if the provider
    is not present.

    Args:
        env: The analysis test environment.
        target: The target whose provider should be looked up.
        provider_name: The name of the provider to return.

    Returns:
        The provider value, or `None` if it was not propagated by the target.
    """
    provider = None
    if provider_name == "CcInfo":
        provider = CcInfo
    elif provider_name == "DefaultInfo":
        provider = DefaultInfo
    elif provider_name == "OutputGroupInfo":
        provider = OutputGroupInfo
    elif provider_name == "SwiftInfo":
        provider = SwiftInfo
    elif provider_name == "apple_common.Objc":
        provider = apple_common.Objc

    if not provider:
        unittest.fail(
            env,
            "Provider '{}' is not supported.".format(provider_name),
        )
        return None

    if provider in target:
        return target[provider]
    return None

def _field_access_description(target, provider, field):
    """Returns a string denoting field access to a provider on a target.

    This function is used to generate a pretty string that can be used in
    assertion failure messages, of the form
    `<//package:target>[ProviderInfo].some.field.path`.

    Args:
        target: The target whose provider is being accessed.
        provider: The name of the provider being accessed.
        field: The field name or dotted field path being accessed.

    Returns:
        A string describing the field access that can be used in assertion
        failure messages.
    """
    return "<{}>[{}].{}".format(target.label, provider, field)

def _provider_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = ctx.attr.target_under_test

    # If configuration settings were provided, then we have a transition and
    # target_under_test will be a list. In that case, get the actual target by
    # pulling the first one out.
    if types.is_list(target_under_test):
        target_under_test = target_under_test[0]

    provider_name = ctx.attr.does_not_propagate_provider
    if provider_name:
        provider = _lookup_provider_by_name(
            env,
            target_under_test,
            provider_name,
        )
        if provider:
            unittest.fail(
                env,
                "Expected {} to not propagate '{}', but it did: {}".format(
                    target_under_test.label,
                    provider_name,
                    provider,
                ),
            )
        return analysistest.end(env)

    provider_name = ctx.attr.provider
    field = ctx.attr.field
    if not provider_name or not field:
        fail("Either 'does_not_propagate_provider' must be specified, or " +
             "both 'provider' and 'field' must be specified.")

    provider = _lookup_provider_by_name(env, target_under_test, provider_name)
    if not provider:
        unittest.fail(
            env,
            "Target '{}' did not provide '{}'.".format(
                target_under_test.label,
                provider_name,
            ),
        )
        return analysistest.end(env)

    actual = _evaluate_field(env, provider, field)
    if actual == _EVALUATE_FIELD_FAILED:
        return analysistest.end(env)

    access_description = _field_access_description(
        target_under_test,
        provider_name,
        field,
    )

    # TODO(allevato): Support other comparisons as they become needed.
    if ctx.attr.expected_files:
        compare_expected_files(
            env,
            access_description,
            ctx.attr.expected_files,
            actual,
        )

    return analysistest.end(env)

def make_provider_test_rule(config_settings = {}):
    """Returns a new `provider_test`-like rule with custom config settings.

    Args:
        config_settings: A dictionary of configuration settings and their values
            that should be applied during tests.

    Returns:
        A rule returned by `analysistest.make` that has the `provider_test`
        interface and the given config settings.
    """
    return analysistest.make(
        _provider_test_impl,
        attrs = {
            "does_not_propagate_provider": attr.string(
                mandatory = False,
                doc = """\
The name of a provider that is expected to not be propagated by the target under
test.

Currently, only the following providers are recognized:

*   `CcInfo`
*   `DefaultInfo`
*   `OutputGroupInfo`
*   `SwiftInfo`
*   `apple_common.Objc`
""",
            ),
            "expected_files": attr.string_list(
                mandatory = False,
                doc = """\
The expected list of files when evaluating the given provider's field.

This list can contain three types of strings:

*   A path suffix (`foo/bar/baz.ext`), denoting that a file whose path has the
    given suffix must be present.
*   A negated path suffix (`-foo/bar/baz.ext`), denoting that a file whose path
    has the given suffix must *not* be present.
*   A wildcard (`*`), denoting that the expected list of files can be a *subset*
    of the actual list. If the wildcard is omitted, the expected list of files
    must match the actual list completely; unmatched files will result in a test
    failure.

The use of path suffixes allows the test to be unconcerned about specific
configuration details, such as output directories for generated files.
""",
            ),
            "field": attr.string(
                mandatory = False,
                doc = """\
The field name or dotted field path of the provider that should be tested.

Evaluation of field path components is projected across collections. That is, if
the result of evaluating a field along the path is a depset or a list, then the
result will be normalized into a list and remaining fields in the path will be
evaluated on every item in that list, not on the list itself. Likewise, if such
a field path component is followed by `!`, then any `None` elements that may
have resulted during evaluation will be removed from the list before evaluating
the next component.

If a value along the field path is a dictionary and the next component
is a valid key in that dictionary, then the value of that dictionary key is
retrieved instead of it being treated as a struct field access.
""",
            ),
            "provider": attr.string(
                mandatory = False,
                doc = """\
The name of the provider expected to be propagated by the target under test, and
on which the field will be checked.

Currently, only the following providers are recognized:

*   `CcInfo`
*   `DefaultInfo`
*   `OutputGroupInfo`
*   `SwiftInfo`
*   `apple_common.Objc`
""",
            ),
        },
        config_settings = config_settings,
    )

# A default instantiation of the rule when no custom config settings are needed.
provider_test = make_provider_test_rule()
