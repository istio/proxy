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
"""All providers for rule-based bazel toolchain config."""

load("@bazel_skylib//rules/directory:providers.bzl", "DirectoryInfo")
load("//cc/toolchains/impl:args_utils.bzl", "validate_nested_args")
load(
    "//cc/toolchains/impl:collect.bzl",
    "collect_action_types",
    "collect_files",
    "collect_provider",
)
load(
    "//cc/toolchains/impl:nested_args.bzl",
    "NESTED_ARGS_ATTRS",
    "nested_args_provider_from_ctx",
)
load(
    ":cc_toolchain_info.bzl",
    "ActionTypeSetInfo",
    "ArgsInfo",
    "ArgsListInfo",
    "BuiltinVariablesInfo",
    "FeatureConstraintInfo",
)

visibility("public")

def _cc_args_impl(ctx):
    actions = collect_action_types(ctx.attr.actions)

    nested = None
    if ctx.attr.args or ctx.attr.nested:
        nested = nested_args_provider_from_ctx(ctx)
        validate_nested_args(
            variables = ctx.attr._variables[BuiltinVariablesInfo].variables,
            nested_args = nested,
            actions = actions.to_list(),
            label = ctx.label,
        )
        files = nested.files
    else:
        files = collect_files(ctx.attr.data + ctx.attr.allowlist_include_directories)

    requires = collect_provider(ctx.attr.requires_any_of, FeatureConstraintInfo)

    args = ArgsInfo(
        label = ctx.label,
        actions = actions,
        requires_any_of = tuple(requires),
        nested = nested,
        env = ctx.attr.env,
        files = files,
        allowlist_include_directories = depset(
            direct = [d[DirectoryInfo] for d in ctx.attr.allowlist_include_directories],
        ),
    )
    return [
        args,
        ArgsListInfo(
            label = ctx.label,
            args = tuple([args]),
            files = files,
            by_action = tuple([
                struct(action = action, args = tuple([args]), files = files)
                for action in actions.to_list()
            ]),
            allowlist_include_directories = args.allowlist_include_directories,
        ),
    ]

_cc_args = rule(
    implementation = _cc_args_impl,
    attrs = {
        "actions": attr.label_list(
            providers = [ActionTypeSetInfo],
            mandatory = True,
            doc = """See documentation for cc_args macro wrapper.""",
        ),
        "allowlist_include_directories": attr.label_list(
            providers = [DirectoryInfo],
            doc = """See documentation for cc_args macro wrapper.""",
        ),
        "env": attr.string_dict(
            doc = """See documentation for cc_args macro wrapper.""",
        ),
        "requires_any_of": attr.label_list(
            providers = [FeatureConstraintInfo],
            doc = """See documentation for cc_args macro wrapper.""",
        ),
        "_variables": attr.label(
            default = "//cc/toolchains/variables:variables",
        ),
    } | NESTED_ARGS_ATTRS,
    provides = [ArgsInfo],
    doc = """Declares a list of arguments bound to a set of actions.

Roughly equivalent to ctx.actions.args()

Examples:
    cc_args(
        name = "warnings_as_errors",
        args = ["-Werror"],
    )
""",
)

def cc_args(
        *,
        name,
        actions = None,
        allowlist_include_directories = None,
        args = None,
        data = None,
        env = None,
        format = {},
        iterate_over = None,
        nested = None,
        requires_not_none = None,
        requires_none = None,
        requires_true = None,
        requires_false = None,
        requires_equal = None,
        requires_equal_value = None,
        requires_any_of = None,
        **kwargs):
    """Action-specific arguments for use with a `cc_toolchain`.

    This rule is the fundamental building building block for every toolchain tool invocation. Each
    argument expressed in a toolchain tool invocation (e.g. `gcc`, `llvm-ar`) is declared in a
    `cc_args` rule that applies an ordered list of arguments to a set of toolchain
    actions. `cc_args` rules can be added unconditionally to a
    `cc_toolchain`, conditionally via `select()` statements, or dynamically via an
    intermediate `cc_feature`.

    Conceptually, this is similar to the old `CFLAGS`, `CPPFLAGS`, etc. environment variables that
    many build systems use to determine which flags to use for a given action. The significant
    difference is that `cc_args` rules are declared in a structured way that allows for
    significantly more powerful and sharable toolchain configurations. Also, due to Bazel's more
    granular action types, it's possible to bind flags to very specific actions (e.g. LTO indexing
    for an executable vs a dynamic library) multiple different actions (e.g. C++ compile and link
    simultaneously).

    Example usage:
    ```
    load("//cc/toolchains:args.bzl", "cc_args")

    # Basic usage: a trivial flag.
    #
    # An example of expressing `-Werror` as a `cc_args` rule.
    cc_args(
        name = "warnings_as_errors",
        actions = [
            # Applies to all C/C++ compile actions.
            "//cc/toolchains/actions:compile_actions",
        ],
        args = ["-Werror"],
    )

    # Basic usage: ordered flags.
    #
    # An example of linking against libc++, which uses two flags that must be applied in order.
    cc_args(
        name = "link_libcxx",
        actions = [
            # Applies to all link actions.
            "//cc/toolchains/actions:link_actions",
        ],
        # On tool invocation, this appears as `-Xlinker -lc++`. Nothing will ever end up between
        # the two flags.
        args = [
            "-Xlinker",
            "-lc++",
        ],
    )

    # Advanced usage: built-in variable expansions.
    #
    # Expands to `-L/path/to/search_dir` for each directory in the built-in variable
    # `library_search_directories`. This variable is managed internally by Bazel through inherent
    # behaviors of Bazel and the interactions between various C/C++ build rules.
    cc_args(
        name = "library_search_directories",
        actions = [
            "//cc/toolchains/actions:link_actions",
        ],
        args = ["-L{search_dir}"],
        iterate_over = "//cc/toolchains/variables:library_search_directories",
        requires_not_none = "//cc/toolchains/variables:library_search_directories",
        format = {
            "search_dir": "//cc/toolchains/variables:library_search_directories",
        },
    )
    ```

    For more extensive examples, see the usages here:
        https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args

    Args:
        name: (str) The name of the target.
        actions: (List[Label]) A list of labels of `cc_action_type` or
            `cc_action_type_set` rules that dictate which actions these
            arguments should be applied to.
        allowlist_include_directories: (List[Label]) A list of include paths that are implied by
            using this rule. These must point to a skylib
            [directory](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/directory_doc.md#directory)
            or [subdirectory](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/directory_subdirectory_doc.md#subdirectory) rule.
            Some flags (e.g. --sysroot) imply certain include paths are available despite
            not explicitly specifying a normal include path flag (`-I`, `-isystem`, etc.).
            Bazel checks that all included headers are properly provided by a dependency or
            allowlisted through this mechanism.

            As a rule of thumb, only use this if Bazel is complaining about absolute paths in
            your toolchain and you've ensured that the toolchain is compiling with the
            `-no-canonical-prefixes` and/or `-fno-canonical-system-headers` arguments.

            This can help work around errors like:
            `the source file 'main.c' includes the following non-builtin files with absolute paths
            (if these are builtin files, make sure these paths are in your toolchain)`.
        args: (List[str]) The command-line arguments that are applied by using this rule. This is
            mutually exclusive with [nested](#cc_args-nested).
        data: (List[Label]) A list of runtime data dependencies that are required for these
            arguments to work as intended.
        env: (Dict[str, str]) Environment variables that should be set when the tool is invoked.
        format: (Dict[str, Label]) A mapping of format strings to the label of the corresponding
            `cc_variable` that the value should be pulled from. All instances of
            `{variable_name}` will be replaced with the expanded value of `variable_name` in this
            dictionary. The complete list of possible variables can be found in
            https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD.
            It is not possible to declare custom variables--these are inherent to Bazel itself.
        iterate_over: (Label) The label of a `cc_variable` that should be iterated over. This is
            intended for use with built-in variables that are lists.
        nested: (List[Label]) A list of `cc_nested_args` rules that should be
            expanded to command-line arguments when this rule is used. This is mutually exclusive
            with [args](#cc_args-args).
        requires_not_none: (Label) The label of a `cc_variable` that should be checked
            for existence before expanding this rule. If the variable is None, this rule will be
            ignored.
        requires_none: (Label) The label of a `cc_variable` that should be checked for
            non-existence before expanding this rule. If the variable is not None, this rule will be
            ignored.
        requires_true: (Label) The label of a `cc_variable` that should be checked for
            truthiness before expanding this rule. If the variable is false, this rule will be
            ignored.
        requires_false: (Label) The label of a `cc_variable` that should be checked
            for falsiness before expanding this rule. If the variable is true, this rule will be
            ignored.
        requires_equal: (Label) The label of a `cc_variable` that should be checked
            for equality before expanding this rule. If the variable is not equal to
            (requires_equal_value)[#cc_args-requires_equal_value], this rule will be ignored.
        requires_equal_value: (str) The value to compare (requires_equal)[#cc_args-requires_equal]
            against.
        requires_any_of: (List[Label]) These arguments will be used
            in a tool invocation when at least one of the [cc_feature_constraint](#cc_feature_constraint)
            entries in this list are satisfied. If omitted, this flag set will be enabled
            unconditionally.
        **kwargs: [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.
    """
    return _cc_args(
        name = name,
        actions = actions,
        allowlist_include_directories = allowlist_include_directories,
        args = args,
        data = data,
        env = env,
        # We flip the key/value pairs in the dictionary here because Bazel doesn't have a
        # string-keyed label dict attribute type.
        format = {k: v for v, k in format.items()},
        iterate_over = iterate_over,
        nested = nested,
        requires_not_none = requires_not_none,
        requires_none = requires_none,
        requires_true = requires_true,
        requires_false = requires_false,
        requires_equal = requires_equal,
        requires_equal_value = requires_equal_value,
        requires_any_of = requires_any_of,
        **kwargs
    )
