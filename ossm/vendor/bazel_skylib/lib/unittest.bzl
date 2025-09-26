# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Unit testing support.

Unlike most Skylib files, this exports four modules:
* `unittest` contains functions to declare and define unit tests for ordinary
   Starlark functions;
* `analysistest` contains functions to declare and define tests for analysis
   phase behavior of a rule, such as a given target's providers or registered
   actions;
* `loadingtest` contains functions to declare and define tests for loading
   phase behavior, such as macros and `native.*`;
* `asserts` contains the assertions used within tests.

See https://bazel.build/extending/concepts for background about macros, rules,
and the different phases of a build.
"""

load(":new_sets.bzl", new_sets = "sets")
load(":partial.bzl", "partial")
load(":types.bzl", "types")

# The following function should only be called from WORKSPACE files and workspace macros.
# buildifier: disable=unnamed-macro
def register_unittest_toolchains():
    """Registers the toolchains for unittest users."""
    native.register_toolchains(
        "@bazel_skylib//toolchains/unittest:cmd_toolchain",
        "@bazel_skylib//toolchains/unittest:bash_toolchain",
    )

TOOLCHAIN_TYPE = "@bazel_skylib//toolchains/unittest:toolchain_type"

_UnittestToolchainInfo = provider(
    doc = "Execution platform information for rules in the bazel_skylib repository.",
    fields = [
        "file_ext",
        "success_templ",
        "failure_templ",
        "join_on",
        "escape_chars_with",
        "escape_other_chars_with",
    ],
)

def _unittest_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            unittest_toolchain_info = _UnittestToolchainInfo(
                file_ext = ctx.attr.file_ext,
                success_templ = ctx.attr.success_templ,
                failure_templ = ctx.attr.failure_templ,
                join_on = ctx.attr.join_on,
                escape_chars_with = ctx.attr.escape_chars_with,
                escape_other_chars_with = ctx.attr.escape_other_chars_with,
            ),
        ),
    ]

unittest_toolchain = rule(
    implementation = _unittest_toolchain_impl,
    attrs = {
        "failure_templ": attr.string(
            mandatory = True,
            doc = (
                "Test script template with a single `%s`. That " +
                "placeholder is replaced with the lines in the " +
                "failure message joined with the string " +
                "specified in `join_with`. The resulting script " +
                "should print the failure message and exit with " +
                "non-zero status."
            ),
        ),
        "file_ext": attr.string(
            mandatory = True,
            doc = (
                "File extension for test script, including leading dot."
            ),
        ),
        "join_on": attr.string(
            mandatory = True,
            doc = (
                "String used to join the lines in the failure " +
                "message before including the resulting string " +
                "in the script specified in `failure_templ`."
            ),
        ),
        "success_templ": attr.string(
            mandatory = True,
            doc = (
                "Test script generated when the test passes. " +
                "Should exit with status 0."
            ),
        ),
        "escape_chars_with": attr.string_dict(
            doc = (
                "Dictionary of characters that need escaping in " +
                "test failure message to prefix appended to escape " +
                "those characters. For example, " +
                '`{"%": "%", ">": "^"}` would replace `%` with ' +
                "`%%` and `>` with `^>` in the failure message " +
                "before that is included in `success_templ`."
            ),
        ),
        "escape_other_chars_with": attr.string(
            default = "",
            doc = (
                "String to prefix every character in test failure " +
                "message which is not a key in `escape_chars_with` " +
                "before including that in `success_templ`. For " +
                'example, `"\"` would prefix every character in ' +
                "the failure message (except those in the keys of " +
                "`escape_chars_with`) with `\\`."
            ),
        ),
    },
)

def _impl_function_name(impl):
    """Derives the name of the given rule implementation function.

    This can be used for better test feedback.

    Args:
      impl: the rule implementation function

    Returns:
      The name of the given function
    """

    # Starlark currently stringifies a function as "<function NAME>", so we use
    # that knowledge to parse the "NAME" portion out. If this behavior ever
    # changes, we'll need to update this.
    # TODO(bazel-team): Expose a ._name field on functions to avoid this.
    impl_name = str(impl)
    impl_name = impl_name.partition("<function ")[-1]
    return impl_name.rpartition(">")[0]

def _make(impl, attrs = {}, doc = "", toolchains = []):
    """Creates a unit test rule from its implementation function.

    Each unit test is defined in an implementation function that must then be
    associated with a rule so that a target can be built. This function handles
    the boilerplate to create and return a test rule and captures the
    implementation function's name so that it can be printed in test feedback.

    The optional `attrs` argument can be used to define dependencies for this
    test, in order to form unit tests of rules.

    The optional `toolchains` argument can be used to define toolchain
    dependencies for this test.

    An example of a unit test:

    ```
    def _your_test(ctx):
      env = unittest.begin(ctx)

      # Assert statements go here

      return unittest.end(env)

    your_test = unittest.make(_your_test)
    ```

    Recall that names of test rules must end in `_test`.

    Args:
      impl: The implementation function of the unit test.
      attrs: An optional dictionary to supplement the attrs passed to the
          unit test's `rule()` constructor.
      doc: A description of the rule that can be extracted by documentation generating tools.
      toolchains: An optional list to supplement the toolchains passed to
          the unit test's `rule()` constructor.

    Returns:
      A rule definition that should be stored in a global whose name ends in
      `_test`.
    """
    attrs = dict(attrs)
    attrs["_impl_name"] = attr.string(default = _impl_function_name(impl))

    return rule(
        impl,
        doc = doc,
        attrs = attrs,
        _skylark_testable = True,
        test = True,
        toolchains = toolchains + [TOOLCHAIN_TYPE],
    )

_ActionInfo = provider(
    doc = "Information relating to the target under test.",
    fields = ["actions", "bin_path"],
)

def _action_retrieving_aspect_impl(target, ctx):
    return [
        _ActionInfo(
            actions = target.actions,
            bin_path = ctx.bin_dir.path,
        ),
    ]

_action_retrieving_aspect = aspect(
    attr_aspects = [],
    implementation = _action_retrieving_aspect_impl,
)

# TODO(cparsons): Provide more full documentation on analysis testing in README.
def _make_analysis_test(
        impl,
        expect_failure = False,
        attrs = {},
        fragments = [],
        config_settings = {},
        extra_target_under_test_aspects = [],
        doc = ""):
    """Creates an analysis test rule from its implementation function.

    An analysis test verifies the behavior of a "real" rule target by examining
    and asserting on the providers given by the real target.

    Each analysis test is defined in an implementation function that must then be
    associated with a rule so that a target can be built. This function handles
    the boilerplate to create and return a test rule and captures the
    implementation function's name so that it can be printed in test feedback.

    An example of an analysis test:

    ```
    def _your_test(ctx):
      env = analysistest.begin(ctx)

      # Assert statements go here

      return analysistest.end(env)

    your_test = analysistest.make(_your_test)
    ```

    Recall that names of test rules must end in `_test`.

    Args:
      impl: The implementation function of the unit test.
      expect_failure: If true, the analysis test will expect the target_under_test
          to fail. Assertions can be made on the underlying failure using asserts.expect_failure
      attrs: An optional dictionary to supplement the attrs passed to the
          unit test's `rule()` constructor.
      fragments: An optional list of fragment names that can be used to give rules access to
          language-specific parts of configuration.
      config_settings: A dictionary of configuration settings to change for the target under
          test and its dependencies. This may be used to essentially change 'build flags' for
          the target under test, and may thus be utilized to test multiple targets with different
          flags in a single build
      extra_target_under_test_aspects: An optional list of aspects to apply to the target_under_test
          in addition to those set up by default for the test harness itself.
      doc: A description of the rule that can be extracted by documentation generating tools.

    Returns:
      A rule definition that should be stored in a global whose name ends in
      `_test`.
    """
    attrs = dict(attrs)
    attrs["_impl_name"] = attr.string(default = _impl_function_name(impl))

    changed_settings = dict(config_settings)
    if expect_failure:
        changed_settings["//command_line_option:allow_analysis_failures"] = "True"

    target_attr_kwargs = {}
    if changed_settings:
        test_transition = analysis_test_transition(
            settings = changed_settings,
        )
        target_attr_kwargs["cfg"] = test_transition

    attrs["target_under_test"] = attr.label(
        aspects = [_action_retrieving_aspect] + extra_target_under_test_aspects,
        mandatory = True,
        **target_attr_kwargs
    )

    return rule(
        impl,
        doc = doc,
        attrs = attrs,
        fragments = fragments,
        test = True,
        toolchains = [TOOLCHAIN_TYPE],
        analysis_test = True,
    )

def _suite(name, *test_rules):
    """Defines a `test_suite` target that contains multiple tests.

    After defining your test rules in a `.bzl` file, you need to create targets
    from those rules so that `blaze test` can execute them. Doing this manually
    in a BUILD file would consist of listing each test in your `load` statement
    and then creating each target one by one. To reduce duplication, we recommend
    writing a macro in your `.bzl` file to instantiate all targets, and calling
    that macro from your BUILD file so you only have to load one symbol.

    You can use this function to create the targets and wrap them in a single
    test_suite target. If a test rule requires no arguments, you can simply list
    it as an argument. If you wish to supply attributes explicitly, you can do so
    using `partial.make()`. For instance, in your `.bzl` file, you could write:

    ```
    def your_test_suite():
      unittest.suite(
          "your_test_suite",
          your_test,
          your_other_test,
          partial.make(yet_another_test, timeout = "short"),
      )
    ```

    Then, in your `BUILD` file, simply load the macro and invoke it to have all
    of the targets created:

    ```
    load("//path/to/your/package:tests.bzl", "your_test_suite")
    your_test_suite()
    ```

    If you pass _N_ unit test rules to `unittest.suite`, _N_ + 1 targets will be
    created: a `test_suite` target named `${name}` (where `${name}` is the name
    argument passed in here) and targets named `${name}_test_${i}`, where `${i}`
    is the index of the test in the `test_rules` list, which is used to uniquely
    name each target.

    Args:
      name: The name of the `test_suite` target, and the prefix of all the test
          target names.
      *test_rules: A list of test rules defines by `unittest.test`.
    """
    test_names = []
    for index, test_rule in enumerate(test_rules):
        test_name = "%s_test_%d" % (name, index)
        if partial.is_instance(test_rule):
            partial.call(test_rule, name = test_name)
        else:
            test_rule(name = test_name)
        test_names.append(test_name)

    native.test_suite(
        name = name,
        tests = [":%s" % t for t in test_names],
    )

def _begin(ctx):
    """Begins a unit test.

    This should be the first function called in a unit test implementation
    function. It initializes a "test environment" that is used to collect
    assertion failures so that they can be reported and logged at the end of the
    test.

    Args:
      ctx: The Starlark context. Pass the implementation function's `ctx` argument
          in verbatim.

    Returns:
      A test environment struct that must be passed to assertions and finally to
      `unittest.end`. Do not rely on internal details about the fields in this
      struct as it may change.
    """
    return struct(ctx = ctx, failures = [])

def _begin_analysis_test(ctx):
    """Begins an analysis test.

    This should be the first function called in an analysis test implementation
    function. It initializes a "test environment" that is used to collect
    assertion failures so that they can be reported and logged at the end of the
    test.

    Args:
      ctx: The Starlark context. Pass the implementation function's `ctx` argument
          in verbatim.

    Returns:
      A test environment struct that must be passed to assertions and finally to
      `analysistest.end`. Do not rely on internal details about the fields in this
      struct as it may change.
    """
    return struct(ctx = ctx, failures = [])

def _end_analysis_test(env):
    """Ends an analysis test and logs the results.

    This must be called and returned at the end of an analysis test implementation function so
    that the results are reported.

    Args:
      env: The test environment returned by `analysistest.begin`.

    Returns:
      A list of providers needed to automatically register the analysis test result.
    """
    return [AnalysisTestResultInfo(
        success = (len(env.failures) == 0),
        message = "\n".join(env.failures),
    )]

def _end(env):
    """Ends a unit test and logs the results.

    This must be called and returned at the end of a unit test implementation function so
    that the results are reported.

    Args:
      env: The test environment returned by `unittest.begin`.

    Returns:
      A list of providers needed to automatically register the test result.
    """

    tc = env.ctx.toolchains[TOOLCHAIN_TYPE].unittest_toolchain_info
    testbin = env.ctx.actions.declare_file(env.ctx.label.name + tc.file_ext)
    if env.failures:
        failure_message_lines = "\n".join(env.failures).split("\n")
        escaped_failure_message_lines = [
            "".join([
                tc.escape_chars_with.get(c, tc.escape_other_chars_with) + c
                for c in line.elems()
            ])
            for line in failure_message_lines
        ]
        cmd = tc.failure_templ % tc.join_on.join(escaped_failure_message_lines)
    else:
        cmd = tc.success_templ

    env.ctx.actions.write(
        output = testbin,
        content = cmd,
        is_executable = True,
    )
    return [DefaultInfo(executable = testbin)]

def _fail(env, msg):
    """Unconditionally causes the current test to fail.

    Args:
      env: The test environment returned by `unittest.begin`.
      msg: The message to log describing the failure.
    """
    full_msg = "In test %s: %s" % (env.ctx.attr._impl_name, msg)

    # There isn't a better way to output the message in Starlark, so use print.
    # buildifier: disable=print
    print(full_msg)
    env.failures.append(full_msg)

def _assert_true(
        env,
        condition,
        msg = "Expected condition to be true, but was false."):
    """Asserts that the given `condition` is true.

    Args:
      env: The test environment returned by `unittest.begin`.
      condition: A value that will be evaluated in a Boolean context.
      msg: An optional message that will be printed that describes the failure.
          If omitted, a default will be used.
    """
    if not condition:
        _fail(env, msg)

def _assert_false(
        env,
        condition,
        msg = "Expected condition to be false, but was true."):
    """Asserts that the given `condition` is false.

    Args:
      env: The test environment returned by `unittest.begin`.
      condition: A value that will be evaluated in a Boolean context.
      msg: An optional message that will be printed that describes the failure.
          If omitted, a default will be used.
    """
    if condition:
        _fail(env, msg)

def _assert_equals(env, expected, actual, msg = None):
    """Asserts that the given `expected` and `actual` values are equal.

    Args:
      env: The test environment returned by `unittest.begin`.
      expected: The expected value of some computation.
      actual: The actual value returned by some computation.
      msg: An optional message that will be printed that describes the failure.
          If omitted, a default will be used.
    """
    if expected != actual:
        expectation_msg = 'Expected "%s", but got "%s"' % (expected, actual)
        if msg:
            full_msg = "%s (%s)" % (msg, expectation_msg)
        else:
            full_msg = expectation_msg
        _fail(env, full_msg)

def _assert_set_equals(env, expected, actual, msg = None):
    """Asserts that the given `expected` and `actual` sets are equal.

    Args:
      env: The test environment returned by `unittest.begin`.
      expected: The expected set resulting from some computation.
      actual: The actual set returned by some computation.
      msg: An optional message that will be printed that describes the failure.
          If omitted, a default will be used.
    """
    if not new_sets.is_equal(expected, actual):
        missing = new_sets.difference(expected, actual)
        unexpected = new_sets.difference(actual, expected)
        expectation_msg = "Expected %s, but got %s" % (new_sets.str(expected), new_sets.str(actual))
        if new_sets.length(missing) > 0:
            expectation_msg += ", missing are %s" % (new_sets.str(missing))
        if new_sets.length(unexpected) > 0:
            expectation_msg += ", unexpected are %s" % (new_sets.str(unexpected))
        if msg:
            full_msg = "%s (%s)" % (msg, expectation_msg)
        else:
            full_msg = expectation_msg
        _fail(env, full_msg)

_assert_new_set_equals = _assert_set_equals

def _expect_failure(env, expected_failure_msg = ""):
    """Asserts that the target under test has failed with a given error message.

    This requires that the analysis test is created with `analysistest.make()` and
    `expect_failures = True` is specified.

    Args:
      env: The test environment returned by `analysistest.begin`.
      expected_failure_msg: The error message to expect as a result of analysis failures.
    """
    dep = _target_under_test(env)
    if AnalysisFailureInfo in dep:
        actual_errors = ""
        for cause in dep[AnalysisFailureInfo].causes.to_list():
            actual_errors += cause.message + "\n"
        if actual_errors.find(expected_failure_msg) < 0:
            expectation_msg = "Expected errors to contain '%s' but did not. " % expected_failure_msg
            expectation_msg += "Actual errors:%s" % actual_errors
            _fail(env, expectation_msg)
    else:
        _fail(env, "Expected failure of target_under_test, but found success")

def _target_actions(env):
    """Returns a list of actions registered by the target under test.

    Args:
      env: The test environment returned by `analysistest.begin`.

    Returns:
      A list of actions registered by the target under test
    """

    # Validate?
    return _target_under_test(env)[_ActionInfo].actions

def _target_bin_dir_path(env):
    """Returns ctx.bin_dir.path for the target under test.

    Args:
      env: The test environment returned by `analysistest.begin`.

    Returns:
      Output bin dir path string.
    """
    return _target_under_test(env)[_ActionInfo].bin_path

def _target_under_test(env):
    """Returns the target under test.

    Args:
      env: The test environment returned by `analysistest.begin`.

    Returns:
      The target under test.
    """
    result = getattr(env.ctx.attr, "target_under_test")
    if types.is_list(result):
        if result:
            return result[0]
        else:
            fail("test rule does not have a target_under_test")
    return result

def _loading_test_impl(ctx):
    tc = ctx.toolchains[TOOLCHAIN_TYPE].unittest_toolchain_info
    content = tc.success_templ
    if ctx.attr.failure_message:
        content = tc.failure_templ % ctx.attr.failure_message

    testbin = ctx.actions.declare_file("loading_test_" + ctx.label.name + tc.file_ext)
    ctx.actions.write(
        output = testbin,
        content = content,
        is_executable = True,
    )
    return [DefaultInfo(executable = testbin)]

_loading_test = rule(
    implementation = _loading_test_impl,
    attrs = {
        "failure_message": attr.string(),
    },
    toolchains = [TOOLCHAIN_TYPE],
    test = True,
)

def _loading_make(name):
    """Creates a loading phase test environment and test_suite.

    Args:
       name: name of the suite of tests to create

    Returns:
       loading phase environment passed to other loadingtest functions
    """
    native.test_suite(
        name = name + "_tests",
        tags = [name + "_test_case"],
    )
    return struct(name = name)

def _loading_assert_equals(env, test_case, expected, actual):
    """Creates a test case for asserting state at LOADING phase.

    Args:
      env:       Loading test env created from loadingtest.make
      test_case: Name of the test case
      expected:  Expected value to test
      actual:    Actual value received.

    Returns:
      None, creates test case
    """

    msg = None
    if expected != actual:
        msg = 'Expected "%s", but got "%s"' % (expected, actual)

    _loading_test(
        name = "%s_%s" % (env.name, test_case),
        failure_message = msg,
        tags = [env.name + "_test_case"],
    )

asserts = struct(
    expect_failure = _expect_failure,
    equals = _assert_equals,
    false = _assert_false,
    set_equals = _assert_set_equals,
    new_set_equals = _assert_new_set_equals,
    true = _assert_true,
)

unittest = struct(
    make = _make,
    suite = _suite,
    begin = _begin,
    end = _end,
    fail = _fail,
)

analysistest = struct(
    make = _make_analysis_test,
    begin = _begin_analysis_test,
    end = _end_analysis_test,
    fail = _fail,
    target_actions = _target_actions,
    target_bin_dir_path = _target_bin_dir_path,
    target_under_test = _target_under_test,
)

loadingtest = struct(
    make = _loading_make,
    equals = _loading_assert_equals,
)
