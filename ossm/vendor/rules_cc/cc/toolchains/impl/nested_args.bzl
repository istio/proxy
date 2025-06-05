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
"""Helper functions for working with args."""

load("@bazel_skylib//rules/directory:providers.bzl", "DirectoryInfo")
load("//cc:cc_toolchain_config_lib.bzl", "flag_group", "variable_with_value")
load("//cc/toolchains:cc_toolchain_info.bzl", "NestedArgsInfo", "VariableInfo")
load(":collect.bzl", "collect_files", "collect_provider")

visibility([
    "//cc/toolchains",
    "//tests/rule_based_toolchain/...",
])

REQUIRES_MUTUALLY_EXCLUSIVE_ERR = "requires_none, requires_not_none, requires_true, requires_false, and requires_equal are mutually exclusive"
REQUIRES_NOT_NONE_ERR = "requires_not_none only works on options"
REQUIRES_NONE_ERR = "requires_none only works on options"
REQUIRES_TRUE_ERR = "requires_true only works on bools"
REQUIRES_FALSE_ERR = "requires_false only works on bools"
REQUIRES_EQUAL_ERR = "requires_equal only works on strings"
REQUIRES_EQUAL_VALUE_ERR = "When requires_equal is provided, you must also provide requires_equal_value to specify what it should be equal to"
FORMAT_ARGS_ERR = "format only works on string, file, or directory type variables"

# @unsorted-dict-items.
NESTED_ARGS_ATTRS = {
    "args": attr.string_list(
        doc = """json-encoded arguments to be added to the command-line.

Usage:
cc_args(
    ...,
    args = ["--foo={foo}"],
    format = {
        "//cc/toolchains/variables:foo": "foo"
    },
)

This is equivalent to flag_group(flags = ["--foo", "%{foo}"])

Mutually exclusive with nested.
""",
    ),
    "nested": attr.label_list(
        providers = [NestedArgsInfo],
        doc = """nested_args that should be added on the command-line.

Mutually exclusive with args.""",
    ),
    "data": attr.label_list(
        allow_files = True,
        doc = """Files required to add this argument to the command-line.

For example, a flag that sets the header directory might add the headers in that
directory as additional files.
""",
    ),
    "format": attr.label_keyed_string_dict(
        doc = "Variables to be used in substitutions",
    ),
    "iterate_over": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.iterate_over"),
    "requires_not_none": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.expand_if_available"),
    "requires_none": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.expand_if_not_available"),
    "requires_true": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.expand_if_true"),
    "requires_false": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.expand_if_false"),
    "requires_equal": attr.label(providers = [VariableInfo], doc = "Replacement for flag_group.expand_if_equal"),
    "requires_equal_value": attr.string(),
}

def _var(target):
    if target == None:
        return None
    return target[VariableInfo].name

# TODO: Consider replacing this with a subrule in the future. However, maybe not
# for a long time, since it'll break compatibility with all bazel versions < 7.
def nested_args_provider_from_ctx(ctx):
    """Gets the nested args provider from a rule that has NESTED_ARGS_ATTRS.

    Args:
        ctx: The rule context
    Returns:
        NestedArgsInfo
    """
    return nested_args_provider(
        label = ctx.label,
        args = ctx.attr.args,
        format = ctx.attr.format,
        nested = collect_provider(ctx.attr.nested, NestedArgsInfo),
        files = collect_files(ctx.attr.data + getattr(ctx.attr, "allowlist_include_directories", [])),
        iterate_over = ctx.attr.iterate_over,
        requires_not_none = _var(ctx.attr.requires_not_none),
        requires_none = _var(ctx.attr.requires_none),
        requires_true = _var(ctx.attr.requires_true),
        requires_false = _var(ctx.attr.requires_false),
        requires_equal = _var(ctx.attr.requires_equal),
        requires_equal_value = ctx.attr.requires_equal_value,
    )

def nested_args_provider(
        *,
        label,
        args = [],
        nested = [],
        format = {},
        files = depset([]),
        iterate_over = None,
        requires_not_none = None,
        requires_none = None,
        requires_true = None,
        requires_false = None,
        requires_equal = None,
        requires_equal_value = "",
        fail = fail):
    """Creates a validated NestedArgsInfo.

    Does not validate types, as you can't know the type of a variable until
    you have a cc_args wrapping it, because the outer layers can change that
    type using iterate_over.

    Args:
        label: (Label) The context we are currently evaluating in. Used for
          error messages.
        args: (List[str]) The command-line arguments to add.
        nested: (List[NestedArgsInfo]) command-line arguments to expand.
        format: (dict[Target, str]) A mapping from target to format string name
        files: (depset[File]) Files required for this set of command-line args.
        iterate_over: (Optional[Target]) Target for the variable to iterate over
        requires_not_none: (Optional[str]) If provided, this NestedArgsInfo will
          be ignored if the variable is None
        requires_none: (Optional[str]) If provided, this NestedArgsInfo will
          be ignored if the variable is not None
        requires_true: (Optional[str]) If provided, this NestedArgsInfo will
          be ignored if the variable is false
        requires_false: (Optional[str]) If provided, this NestedArgsInfo will
          be ignored if the variable is true
        requires_equal: (Optional[str]) If provided, this NestedArgsInfo will
          be ignored if the variable is not equal to requires_equal_value.
        requires_equal_value: (str) The value to compare the requires_equal
          variable with
        fail: A fail function. Use only for testing.
    Returns:
        NestedArgsInfo
    """
    if bool(args) and bool(nested):
        fail("Args and nested are mutually exclusive")

    replacements = {}
    if iterate_over:
        # Since the user didn't assign a name to iterate_over, allow them to
        # reference it as "--foo={}"
        replacements[""] = iterate_over

    # Intentionally ensure that {} clashes between an explicit user format
    # string "" and the implicit one provided by iterate_over.
    for target, name in format.items():
        if name in replacements:
            fail("Both %s and %s have the format string name %r" % (
                target.label,
                replacements[name].label,
                name,
            ))
        replacements[name] = target

    # Intentionally ensure that we do not have to use the variable provided by
    # iterate_over in the format string.
    # For example, a valid use case is:
    # cc_args(
    #     nested = ":nested",
    #     iterate_over = "//cc/toolchains/variables:libraries_to_link",
    # )
    # cc_nested_args(
    #     args = ["{}"],
    #     iterate_over = "//cc/toolchains/variables:libraries_to_link.object_files",
    # )
    args = format_args(args, replacements, must_use = format.values(), fail = fail)

    transitive_files = [ea.files for ea in nested]
    transitive_files.append(files)

    has_value = [attr for attr in [
        requires_not_none,
        requires_none,
        requires_true,
        requires_false,
        requires_equal,
    ] if attr != None]

    # We may want to reconsider this down the line, but it's easier to open up
    # an API than to lock down an API.
    if len(has_value) > 1:
        fail(REQUIRES_MUTUALLY_EXCLUSIVE_ERR)

    kwargs = {}

    if args:
        kwargs["flags"] = args

    requires_types = {}
    if nested:
        kwargs["flag_groups"] = [ea.legacy_flag_group for ea in nested]

    unwrap_options = []

    if iterate_over:
        kwargs["iterate_over"] = _var(iterate_over)

    if requires_not_none:
        kwargs["expand_if_available"] = requires_not_none
        requires_types.setdefault(requires_not_none, []).append(struct(
            msg = REQUIRES_NOT_NONE_ERR,
            valid_types = ["option"],
            after_option_unwrap = False,
        ))
        unwrap_options.append(requires_not_none)
    elif requires_none:
        kwargs["expand_if_not_available"] = requires_none
        requires_types.setdefault(requires_none, []).append(struct(
            msg = REQUIRES_NONE_ERR,
            valid_types = ["option"],
            after_option_unwrap = False,
        ))
    elif requires_true:
        kwargs["expand_if_true"] = requires_true
        requires_types.setdefault(requires_true, []).append(struct(
            msg = REQUIRES_TRUE_ERR,
            valid_types = ["bool"],
            after_option_unwrap = True,
        ))
        unwrap_options.append(requires_true)
    elif requires_false:
        kwargs["expand_if_false"] = requires_false
        requires_types.setdefault(requires_false, []).append(struct(
            msg = REQUIRES_FALSE_ERR,
            valid_types = ["bool"],
            after_option_unwrap = True,
        ))
        unwrap_options.append(requires_false)
    elif requires_equal:
        if not requires_equal_value:
            fail(REQUIRES_EQUAL_VALUE_ERR)
        kwargs["expand_if_equal"] = variable_with_value(
            name = requires_equal,
            value = requires_equal_value,
        )
        unwrap_options.append(requires_equal)
        requires_types.setdefault(requires_equal, []).append(struct(
            msg = REQUIRES_EQUAL_ERR,
            valid_types = ["string"],
            after_option_unwrap = True,
        ))

    for arg in format:
        if VariableInfo in arg:
            requires_types.setdefault(arg[VariableInfo].name, []).append(struct(
                msg = FORMAT_ARGS_ERR,
                valid_types = ["string", "file", "directory"],
                after_option_unwrap = True,
            ))

    return NestedArgsInfo(
        label = label,
        nested = nested,
        files = depset(transitive = transitive_files),
        iterate_over = _var(iterate_over),
        unwrap_options = unwrap_options,
        requires_types = requires_types,
        legacy_flag_group = flag_group(**kwargs),
    )

def _escape(s):
    return s.replace("%", "%%")

def _format_target(target, fail = fail):
    if VariableInfo in target:
        return "%%{%s}" % target[VariableInfo].name
    elif DirectoryInfo in target:
        return _escape(target[DirectoryInfo].path)

    files = target[DefaultInfo].files.to_list()
    if len(files) == 1:
        return _escape(files[0].path)

    fail("%s should be either a variable, a directory, or a single file." % target.label)

def format_args(args, format, must_use = [], fail = fail):
    """Lists all of the variables referenced by an argument.

    Eg: format_args(["--foo", "--bar={bar}"], {"bar": VariableInfo(name="bar")})
      => ["--foo", "--bar=%{bar}"]

    Args:
      args: (List[str]) The command-line arguments.
      format: (Dict[str, Target]) A mapping of substitutions from key to target.
      must_use: (List[str]) A list of substitutions that must be used.
      fail: The fail function. Used for tests

    Returns:
      A string defined to be compatible with flag groups.
    """
    formatted = []
    used_vars = {}

    for arg in args:
        upto = 0
        out = []
        has_format = False

        # This should be "while true". I used this number because it's an upper
        # bound of the number of iterations.
        for _ in range(len(arg)):
            if upto >= len(arg):
                break

            # Escaping via "{{" and "}}"
            if arg[upto] in "{}" and upto + 1 < len(arg) and arg[upto + 1] == arg[upto]:
                out.append(arg[upto])
                upto += 2
            elif arg[upto] == "{":
                chunks = arg[upto + 1:].split("}", 1)
                if len(chunks) != 2:
                    fail("Unmatched { in %r" % arg)
                variable = chunks[0]

                if variable not in format:
                    fail('Unknown variable %r in format string %r. Try using cc_args(..., format = {"//path/to:variable": %r})' % (variable, arg, variable))
                elif has_format:
                    fail("The format string %r contained multiple variables, which is unsupported." % arg)
                else:
                    used_vars[variable] = None
                    has_format = True
                    out.append(_format_target(format[variable], fail = fail))
                    upto += len(variable) + 2

            elif arg[upto] == "}":
                fail("Unexpected } in %r" % arg)
            else:
                out.append(_escape(arg[upto]))
                upto += 1

        formatted.append("".join(out))

    unused_vars = [var for var in must_use if var not in used_vars]
    if unused_vars:
        fail("The variable %r was not used in the format string." % unused_vars[0])

    return formatted
