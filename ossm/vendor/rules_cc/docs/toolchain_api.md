<!-- Generated with Stardoc: http://skydoc.bazel.build -->

This is a list of rules/macros that should be exported as documentation.

<a id="cc_action_type"></a>

## cc_action_type

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_action_type")

cc_action_type(<a href="#cc_action_type-name">name</a>, <a href="#cc_action_type-action_name">action_name</a>)
</pre>

A type of action (eg. c_compile, assemble, strip).

[`cc_action_type`](#cc_action_type) rules are used to associate arguments and tools together to
perform a specific action. Bazel prescribes a set of known action types that are used to drive
typical C/C++/ObjC actions like compiling, linking, and archiving. The set of well-known action
types can be found in [@rules_cc//cc/toolchains/actions:BUILD](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/actions/BUILD).

It's possible to create project-specific action types for use in toolchains. Be careful when
doing this, because every toolchain that encounters the action will need to be configured to
support the custom action type. If your project is a library, avoid creating new action types as
it will reduce compatibility with existing toolchains and increase setup complexity for users.

Example:
```
load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES")
load("@rules_cc//cc/toolchains:actions.bzl", "cc_action_type")

cc_action_type(
    name = "cpp_compile",
    action_name =  = ACTION_NAMES.cpp_compile,
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_action_type-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_action_type-action_name"></a>action_name |  -   | String | required |  |


<a id="cc_action_type_set"></a>

## cc_action_type_set

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_action_type_set")

cc_action_type_set(<a href="#cc_action_type_set-name">name</a>, <a href="#cc_action_type_set-actions">actions</a>, <a href="#cc_action_type_set-allow_empty">allow_empty</a>)
</pre>

Represents a set of actions.

This is a convenience rule to allow for more compact representation of a group of action types.
Use this anywhere a [`cc_action_type`](#cc_action_type) is accepted.

Example:
```
load("@rules_cc//cc/toolchains:actions.bzl", "cc_action_type_set")

cc_action_type_set(
    name = "link_executable_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_link_executable",
        "@rules_cc//cc/toolchains/actions:lto_index_for_executable",
    ],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_action_type_set-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_action_type_set-actions"></a>actions |  A list of cc_action_type or cc_action_type_set   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="cc_action_type_set-allow_empty"></a>allow_empty |  -   | Boolean | optional |  `False`  |


<a id="cc_args_list"></a>

## cc_args_list

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_args_list")

cc_args_list(<a href="#cc_args_list-name">name</a>, <a href="#cc_args_list-args">args</a>)
</pre>

An ordered list of cc_args.

This is a convenience rule to allow you to group a set of multiple [`cc_args`](#cc_args) into a
single list. This particularly useful for toolchain behaviors that require different flags for
different actions.

Note: The order of the arguments in `args` is preserved to support order-sensitive flags.

Example usage:
```
load("@rules_cc//cc/toolchains:cc_args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:args_list.bzl", "cc_args_list")

cc_args(
    name = "gc_sections",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = ["-Wl,--gc-sections"],
)

cc_args(
    name = "function_sections",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = ["-ffunction-sections"],
)

cc_args_list(
    name = "gc_functions",
    args = [
        ":function_sections",
        ":gc_sections",
    ],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_args_list-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_args_list-args"></a>args |  (ordered) cc_args to include in this list.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="cc_external_feature"></a>

## cc_external_feature

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_external_feature")

cc_external_feature(<a href="#cc_external_feature-name">name</a>, <a href="#cc_external_feature-feature_name">feature_name</a>, <a href="#cc_external_feature-overridable">overridable</a>)
</pre>

A declaration that a [feature](https://bazel.build/docs/cc-toolchain-config-reference#features) with this name is defined elsewhere.

This rule communicates that a feature has been defined externally to make it possible to reference
features that live outside the rule-based cc toolchain ecosystem. This allows various toolchain
rules to reference the external feature without accidentally re-defining said feature.

This rule is currently considered a private API of the toolchain rules to encourage the Bazel
ecosystem to migrate to properly defining their features as rules.

Example:
```
load("@rules_cc//cc/toolchains:external_feature.bzl", "cc_external_feature")

# rules_rust defines a feature that is disabled whenever rust artifacts are being linked using
# the cc toolchain to signal that incompatible flags should be disabled as well.
cc_external_feature(
    name = "rules_rust_unsupported_feature",
    feature_name = "rules_rust_unsupported_feature",
    overridable = False,
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_external_feature-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_external_feature-feature_name"></a>feature_name |  The name of the feature   | String | required |  |
| <a id="cc_external_feature-overridable"></a>overridable |  Whether the feature can be overridden   | Boolean | required |  |


<a id="cc_feature"></a>

## cc_feature

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_feature")

cc_feature(<a href="#cc_feature-name">name</a>, <a href="#cc_feature-args">args</a>, <a href="#cc_feature-feature_name">feature_name</a>, <a href="#cc_feature-implies">implies</a>, <a href="#cc_feature-mutually_exclusive">mutually_exclusive</a>, <a href="#cc_feature-overrides">overrides</a>, <a href="#cc_feature-requires_any_of">requires_any_of</a>)
</pre>

A dynamic set of toolchain flags that create a singular [feature](https://bazel.build/docs/cc-toolchain-config-reference#features) definition.

A feature is basically a dynamically toggleable [`cc_args_list`](#cc_args_list). There are a variety of
dependencies and compatibility requirements that must be satisfied to enable a
[`cc_feature`](#cc_feature). Once those conditions are met, the arguments in [`cc_feature.args`](#cc_feature-args)
are expanded and added to the command-line.

A feature may be enabled or disabled through the following mechanisms:
* Via command-line flags, or a `.bazelrc` file via the
  [`--features` flag](https://bazel.build/reference/command-line-reference#flag--features)
* Through inter-feature relationships (via [`cc_feature.implies`](#cc_feature-implies)) where one
  feature may implicitly enable another.
* Individual rules (e.g. `cc_library`) or `package` definitions may elect to manually enable or
  disable features through the
  [`features` attribute](https://bazel.build/reference/be/common-definitions#common.features).

Note that a feature may alternate between enabled and disabled dynamically over the course of a
build. Because of their toggleable nature, it's generally best to avoid adding arguments to a
[`cc_toolchain`](#cc_toolchain) as a [`cc_feature`](#cc_feature) unless strictly necessary. Instead, prefer to express arguments
via [`cc_toolchain.args`](#cc_toolchain-args) whenever possible.

You should use a [`cc_feature`](#cc_feature) when any of the following apply:
* You need the flags to be dynamically toggled over the course of a build.
* You want build files to be able to configure the flags in question. For example, a
  binary might specify `features = ["optimize_for_size"]` to create a small
  binary instead of optimizing for performance.
* You need to carry forward Starlark toolchain behaviors. If you're migrating a
  complex Starlark-based toolchain definition to these rules, many of the
  workflows and flags were likely based on features.

If you only need to configure flags via the Bazel command-line, instead
consider adding a
[`bool_flag`](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/common_settings_doc.md#bool_flag)
paired with a [`config_setting`](https://bazel.build/reference/be/general#config_setting)
and then make your [`cc_args`](#cc_args) rule `select` on the `config_setting`.

For more details about how Bazel handles features, see the official Bazel
documentation at
https://bazel.build/docs/cc-toolchain-config-reference#features.

Example:
```
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# A feature that enables LTO, which may be incompatible when doing interop with various
# languages (e.g. rust, go), or may need to be disabled for particular `cc_binary` rules
# for various reasons.
cc_feature(
    name = "lto",
    feature_name = "lto",
    args = [":lto_args"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_feature-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_feature-args"></a>args |  A list of [`cc_args`](#cc_args) or [`cc_args_list`](#cc_args_list) labels that are expanded when this feature is enabled.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_feature-feature_name"></a>feature_name |  The name of the feature that this rule implements.<br><br>The feature name is a string that will be used in the `features` attribute of rules to enable them (eg. `cc_binary(..., features = ["opt"])`.<br><br>While two features with the same `feature_name` may not be bound to the same toolchain, they can happily live alongside each other in the same BUILD file.<br><br>Example: <pre><code>cc_feature(&#10;    name = "sysroot_macos",&#10;    feature_name = "sysroot",&#10;    ...&#10;)&#10;&#10;cc_feature(&#10;    name = "sysroot_linux",&#10;    feature_name = "sysroot",&#10;    ...&#10;)</code></pre>   | String | optional |  `""`  |
| <a id="cc_feature-implies"></a>implies |  List of features enabled along with this feature.<br><br>Warning: If any of the features cannot be enabled, this feature is silently disabled.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_feature-mutually_exclusive"></a>mutually_exclusive |  A list of things that this feature is mutually exclusive with.<br><br>It can be either: * A feature, in which case the two features are mutually exclusive. * A [`cc_mutually_exclusive_category`](#cc_mutually_exclusive_category), in which case all features that write     `mutually_exclusive = [":category"]` are mutually exclusive with each other.<br><br>If this feature has a side-effect of implementing another feature, it can be useful to list that feature here to ensure they aren't enabled at the same time.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_feature-overrides"></a>overrides |  A declaration that this feature overrides a known feature.<br><br>In the example below, if you missed the "overrides" attribute, it would complain that the feature "opt" was defined twice.<br><br>Example: <pre><code>load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")&#10;&#10;cc_feature(&#10;    name = "opt",&#10;    feature_name = "opt",&#10;    args = [":size_optimized"],&#10;    overrides = "@rules_cc//cc/toolchains/features:opt",&#10;)</code></pre>   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="cc_feature-requires_any_of"></a>requires_any_of |  A list of feature sets that define toolchain compatibility.<br><br>If *at least one* of the listed [`cc_feature_set`](#cc_feature_set)s are fully satisfied (all features exist in the toolchain AND are currently enabled), this feature is deemed compatible and may be enabled.<br><br>Note: Even if `cc_feature.requires_any_of` is satisfied, a feature is not enabled unless another mechanism (e.g. command-line flags, `cc_feature.implies`, `cc_toolchain_config.enabled_features`) signals that the feature should actually be enabled.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="cc_feature_constraint"></a>

## cc_feature_constraint

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_feature_constraint")

cc_feature_constraint(<a href="#cc_feature_constraint-name">name</a>, <a href="#cc_feature_constraint-all_of">all_of</a>, <a href="#cc_feature_constraint-none_of">none_of</a>)
</pre>

Defines a compound relationship between features.

This rule can be used with [`cc_args.require_any_of`](#cc_args-require_any_of) to specify that a set
of arguments are only enabled when a constraint is met. Both `all_of` and `none_of` must be
satisfied simultaneously.

This is basically a [`cc_feature_set`](#cc_feature_set) that supports `none_of` expressions. This extra flexibility
is why this rule may only be used by [`cc_args.require_any_of`](#cc_args-require_any_of).

Example:
```
load("@rules_cc//cc/toolchains:feature_constraint.bzl", "cc_feature_constraint")

# A constraint that requires a `linker_supports_thinlto` feature to be enabled,
# AND a `no_optimization` to be disabled.
cc_feature_constraint(
    name = "thinlto_constraint",
    all_of = [":linker_supports_thinlto"],
    none_of = [":no_optimization"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_feature_constraint-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_feature_constraint-all_of"></a>all_of |  -   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_feature_constraint-none_of"></a>none_of |  -   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="cc_feature_set"></a>

## cc_feature_set

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_feature_set")

cc_feature_set(<a href="#cc_feature_set-name">name</a>, <a href="#cc_feature_set-all_of">all_of</a>)
</pre>

Defines a set of features.

This may be used by both [`cc_feature`](#cc_feature) and [`cc_args`](#cc_args) rules, and is effectively a way to express
a logical `AND` operation across multiple required features.

Example:
```
load("@rules_cc//cc/toolchains:feature_set.bzl", "cc_feature_set")

cc_feature_set(
    name = "thin_lto_requirements",
    all_of = [
        ":thin_lto",
        ":opt",
    ],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_feature_set-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_feature_set-all_of"></a>all_of |  A set of features   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="cc_mutually_exclusive_category"></a>

## cc_mutually_exclusive_category

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_mutually_exclusive_category")

cc_mutually_exclusive_category(<a href="#cc_mutually_exclusive_category-name">name</a>)
</pre>

A rule used to categorize [`cc_feature`](#cc_feature) definitions for which only one can be enabled.

This is used by [`cc_feature.mutually_exclusive`](#cc_feature-mutually_exclusive) to express groups
of [`cc_feature`](#cc_feature) definitions that are inherently incompatible with each other and must be treated as
mutually exclusive.

Warning: These groups are keyed by name, so two [`cc_mutually_exclusive_category`](#cc_mutually_exclusive_category) definitions of the
same name in different packages will resolve to the same logical group.

Example:
```
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")
load("@rules_cc//cc/toolchains:mutually_exclusive_category.bzl", "cc_mutually_exclusive_category")

cc_mutually_exclusive_category(
    name = "opt_level",
)

cc_feature(
    name = "speed_optimized",
    mutually_exclusive = [":opt_level"],
)

cc_feature(
    name = "size_optimized",
    mutually_exclusive = [":opt_level"],
)

cc_feature(
    name = "unoptimized",
    mutually_exclusive = [":opt_level"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_mutually_exclusive_category-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


<a id="cc_tool"></a>

## cc_tool

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_tool")

cc_tool(<a href="#cc_tool-name">name</a>, <a href="#cc_tool-src">src</a>, <a href="#cc_tool-data">data</a>, <a href="#cc_tool-allowlist_include_directories">allowlist_include_directories</a>, <a href="#cc_tool-capabilities">capabilities</a>)
</pre>

Declares a tool for use by toolchain actions.

[`cc_tool`](#cc_tool) rules are used in a [`cc_tool_map`](#cc_tool_map) rule to ensure all files and
metadata required to run a tool are available when constructing a [`cc_toolchain`](#cc_toolchain).

In general, include all files that are always required to run a tool (e.g. libexec/** and
cross-referenced tools in bin/*) in the [data](#cc_tool-data) attribute. If some files are only
required when certain flags are passed to the tool, consider using a [`cc_args`](#cc_args) rule to
bind the files to the flags that require them. This reduces the overhead required to properly
enumerate a sandbox with all the files required to run a tool, and ensures that there isn't
unintentional leakage across configurations and actions.

Example:
```
load("@rules_cc//cc/toolchains:tool.bzl", "cc_tool")

cc_tool(
    name = "clang_tool",
    src = "@llvm_toolchain//:bin/clang",
    # Suppose clang needs libc to run.
    data = ["@llvm_toolchain//:lib/x86_64-linux-gnu/libc.so.6"]
    tags = ["requires-network"],
    capabilities = ["@rules_cc//cc/toolchains/capabilities:supports_pic"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_tool-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_tool-src"></a>src |  The underlying binary that this tool represents.<br><br>Usually just a single prebuilt (eg. @toolchain//:bin/clang), but may be any executable label.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="cc_tool-data"></a>data |  Additional files that are required for this tool to run.<br><br>Frequently, clang and gcc require additional files to execute as they often shell out to other binaries (e.g. `cc1`).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_tool-allowlist_include_directories"></a>allowlist_include_directories |  Include paths implied by using this tool.<br><br>Compilers may include a set of built-in headers that are implicitly available unless flags like `-nostdinc` are provided. Bazel checks that all included headers are properly provided by a dependency or allowlisted through this mechanism.<br><br>As a rule of thumb, only use this if Bazel is complaining about absolute paths in your toolchain and you've ensured that the toolchain is compiling with the `-no-canonical-prefixes` and/or `-fno-canonical-system-headers` arguments.<br><br>This can help work around errors like: `the source file 'main.c' includes the following non-builtin files with absolute paths (if these are builtin files, make sure these paths are in your toolchain)`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="cc_tool-capabilities"></a>capabilities |  Declares that a tool is capable of doing something.<br><br>For example, `@rules_cc//cc/toolchains/capabilities:supports_pic`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="cc_tool_capability"></a>

## cc_tool_capability

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_tool_capability")

cc_tool_capability(<a href="#cc_tool_capability-name">name</a>, <a href="#cc_tool_capability-feature_name">feature_name</a>)
</pre>

A capability is an optional feature that a tool supports.

For example, not all compilers support PIC, so to handle this, we write:

```
cc_tool(
    name = "clang",
    src = "@host_tools/bin/clang",
    capabilities = [
        "@rules_cc//cc/toolchains/capabilities:supports_pic",
    ],
)

cc_args(
    name = "pic",
    requires = [
        "@rules_cc//cc/toolchains/capabilities:supports_pic"
    ],
    args = ["-fPIC"],
)
```

This ensures that `-fPIC` is added to the command-line only when we are using a
tool that supports PIC.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_tool_capability-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_tool_capability-feature_name"></a>feature_name |  The name of the feature to generate for this capability   | String | optional |  `""`  |


<a id="cc_args"></a>

## cc_args

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_args")

cc_args(*, <a href="#cc_args-name">name</a>, <a href="#cc_args-actions">actions</a>, <a href="#cc_args-allowlist_include_directories">allowlist_include_directories</a>, <a href="#cc_args-args">args</a>, <a href="#cc_args-data">data</a>, <a href="#cc_args-env">env</a>, <a href="#cc_args-format">format</a>, <a href="#cc_args-iterate_over">iterate_over</a>,
        <a href="#cc_args-nested">nested</a>, <a href="#cc_args-requires_not_none">requires_not_none</a>, <a href="#cc_args-requires_none">requires_none</a>, <a href="#cc_args-requires_true">requires_true</a>, <a href="#cc_args-requires_false">requires_false</a>, <a href="#cc_args-requires_equal">requires_equal</a>,
        <a href="#cc_args-requires_equal_value">requires_equal_value</a>, <a href="#cc_args-requires_any_of">requires_any_of</a>, <a href="#cc_args-kwargs">**kwargs</a>)
</pre>

Action-specific arguments for use with a [`cc_toolchain`](#cc_toolchain).

This rule is the fundamental building building block for every toolchain tool invocation. Each
argument expressed in a toolchain tool invocation (e.g. `gcc`, `llvm-ar`) is declared in a
[`cc_args`](#cc_args) rule that applies an ordered list of arguments to a set of toolchain
actions. [`cc_args`](#cc_args) rules can be added unconditionally to a
[`cc_toolchain`](#cc_toolchain), conditionally via `select()` statements, or dynamically via an
intermediate [`cc_feature`](#cc_feature).

Conceptually, this is similar to the old `CFLAGS`, `CPPFLAGS`, etc. environment variables that
many build systems use to determine which flags to use for a given action. The significant
difference is that [`cc_args`](#cc_args) rules are declared in a structured way that allows for
significantly more powerful and sharable toolchain configurations. Also, due to Bazel's more
granular action types, it's possible to bind flags to very specific actions (e.g. LTO indexing
for an executable vs a dynamic library) multiple different actions (e.g. C++ compile and link
simultaneously).

Example usage:
```
load("@rules_cc//cc/toolchains:args.bzl", "cc_args")

# Basic usage: a trivial flag.
#
# An example of expressing `-Werror` as a [`cc_args`](#cc_args) rule.
cc_args(
    name = "warnings_as_errors",
    actions = [
        # Applies to all C/C++ compile actions.
        "@rules_cc//cc/toolchains/actions:compile_actions",
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
        "@rules_cc//cc/toolchains/actions:link_actions",
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
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = ["-L{search_dir}"],
    iterate_over = "@rules_cc//cc/toolchains/variables:library_search_directories",
    requires_not_none = "@rules_cc//cc/toolchains/variables:library_search_directories",
    format = {
        "search_dir": "@rules_cc//cc/toolchains/variables:library_search_directories",
    },
)
```

For more extensive examples, see the usages here:
    https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_args-name"></a>name |  (str) The name of the target.   |  none |
| <a id="cc_args-actions"></a>actions |  (List[Label]) A list of labels of [`cc_action_type`](#cc_action_type) or [`cc_action_type_set`](#cc_action_type_set) rules that dictate which actions these arguments should be applied to.   |  `None` |
| <a id="cc_args-allowlist_include_directories"></a>allowlist_include_directories |  (List[Label]) A list of include paths that are implied by using this rule. These must point to a skylib [directory](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/directory_doc.md#directory) or [subdirectory](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/directory_subdirectory_doc.md#subdirectory) rule. Some flags (e.g. --sysroot) imply certain include paths are available despite not explicitly specifying a normal include path flag (`-I`, `-isystem`, etc.). Bazel checks that all included headers are properly provided by a dependency or allowlisted through this mechanism.<br><br>As a rule of thumb, only use this if Bazel is complaining about absolute paths in your toolchain and you've ensured that the toolchain is compiling with the `-no-canonical-prefixes` and/or `-fno-canonical-system-headers` arguments.<br><br>This can help work around errors like: `the source file 'main.c' includes the following non-builtin files with absolute paths (if these are builtin files, make sure these paths are in your toolchain)`.   |  `None` |
| <a id="cc_args-args"></a>args |  (List[str]) The command-line arguments that are applied by using this rule. This is mutually exclusive with [nested](#cc_args-nested).   |  `None` |
| <a id="cc_args-data"></a>data |  (List[Label]) A list of runtime data dependencies that are required for these arguments to work as intended.   |  `None` |
| <a id="cc_args-env"></a>env |  (Dict[str, str]) Environment variables that should be set when the tool is invoked.   |  `None` |
| <a id="cc_args-format"></a>format |  (Dict[str, Label]) A mapping of format strings to the label of the corresponding [`cc_variable`](#cc_variable) that the value should be pulled from. All instances of `{variable_name}` will be replaced with the expanded value of `variable_name` in this dictionary. The complete list of possible variables can be found in https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD. It is not possible to declare custom variables--these are inherent to Bazel itself.   |  `{}` |
| <a id="cc_args-iterate_over"></a>iterate_over |  (Label) The label of a [`cc_variable`](#cc_variable) that should be iterated over. This is intended for use with built-in variables that are lists.   |  `None` |
| <a id="cc_args-nested"></a>nested |  (List[Label]) A list of [`cc_nested_args`](#cc_nested_args) rules that should be expanded to command-line arguments when this rule is used. This is mutually exclusive with [args](#cc_args-args).   |  `None` |
| <a id="cc_args-requires_not_none"></a>requires_not_none |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for existence before expanding this rule. If the variable is None, this rule will be ignored.   |  `None` |
| <a id="cc_args-requires_none"></a>requires_none |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for non-existence before expanding this rule. If the variable is not None, this rule will be ignored.   |  `None` |
| <a id="cc_args-requires_true"></a>requires_true |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for truthiness before expanding this rule. If the variable is false, this rule will be ignored.   |  `None` |
| <a id="cc_args-requires_false"></a>requires_false |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for falsiness before expanding this rule. If the variable is true, this rule will be ignored.   |  `None` |
| <a id="cc_args-requires_equal"></a>requires_equal |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for equality before expanding this rule. If the variable is not equal to (requires_equal_value)[#cc_args-requires_equal_value], this rule will be ignored.   |  `None` |
| <a id="cc_args-requires_equal_value"></a>requires_equal_value |  (str) The value to compare (requires_equal)[#cc_args-requires_equal] against.   |  `None` |
| <a id="cc_args-requires_any_of"></a>requires_any_of |  (List[Label]) These arguments will be used in a tool invocation when at least one of the [cc_feature_constraint](#cc_feature_constraint) entries in this list are satisfied. If omitted, this flag set will be enabled unconditionally.   |  `None` |
| <a id="cc_args-kwargs"></a>kwargs |  [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.   |  none |


<a id="cc_nested_args"></a>

## cc_nested_args

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_nested_args")

cc_nested_args(*, <a href="#cc_nested_args-name">name</a>, <a href="#cc_nested_args-args">args</a>, <a href="#cc_nested_args-data">data</a>, <a href="#cc_nested_args-format">format</a>, <a href="#cc_nested_args-iterate_over">iterate_over</a>, <a href="#cc_nested_args-nested">nested</a>, <a href="#cc_nested_args-requires_not_none">requires_not_none</a>, <a href="#cc_nested_args-requires_none">requires_none</a>,
               <a href="#cc_nested_args-requires_true">requires_true</a>, <a href="#cc_nested_args-requires_false">requires_false</a>, <a href="#cc_nested_args-requires_equal">requires_equal</a>, <a href="#cc_nested_args-requires_equal_value">requires_equal_value</a>, <a href="#cc_nested_args-kwargs">**kwargs</a>)
</pre>

Nested arguments for use in more complex [`cc_args`](#cc_args) expansions.

While this rule is very similar in shape to [`cc_args`](#cc_args), it is intended to be used as a
dependency of [`cc_args`](#cc_args) to provide additional arguments that should be applied to the
same actions as defined by the parent [`cc_args`](#cc_args) rule. The key motivation for this rule
is to allow for more complex variable-based argument expensions.

Prefer expressing collections of arguments as [`cc_args`](#cc_args) and
[`cc_args_list`](#cc_args_list) rules when possible.

For living examples of how this rule is used, see the usages here:
    https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args/runtime_library_search_directories/BUILD
    https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/args/libraries_to_link/BUILD

Note: These examples are non-trivial, but they illustrate when it is absolutely necessary to
use this rule.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_nested_args-name"></a>name |  (str) The name of the target.   |  none |
| <a id="cc_nested_args-args"></a>args |  (List[str]) The command-line arguments that are applied by using this rule. This is mutually exclusive with [nested](#cc_nested_args-nested).   |  `None` |
| <a id="cc_nested_args-data"></a>data |  (List[Label]) A list of runtime data dependencies that are required for these arguments to work as intended.   |  `None` |
| <a id="cc_nested_args-format"></a>format |  (Dict[str, Label]) A mapping of format strings to the label of the corresponding [`cc_variable`](#cc_variable) that the value should be pulled from. All instances of `{variable_name}` will be replaced with the expanded value of `variable_name` in this dictionary. The complete list of possible variables can be found in https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD. It is not possible to declare custom variables--these are inherent to Bazel itself.   |  `{}` |
| <a id="cc_nested_args-iterate_over"></a>iterate_over |  (Label) The label of a [`cc_variable`](#cc_variable) that should be iterated over. This is intended for use with built-in variables that are lists.   |  `None` |
| <a id="cc_nested_args-nested"></a>nested |  (List[Label]) A list of [`cc_nested_args`](#cc_nested_args) rules that should be expanded to command-line arguments when this rule is used. This is mutually exclusive with [args](#cc_nested_args-args).   |  `None` |
| <a id="cc_nested_args-requires_not_none"></a>requires_not_none |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for existence before expanding this rule. If the variable is None, this rule will be ignored.   |  `None` |
| <a id="cc_nested_args-requires_none"></a>requires_none |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for non-existence before expanding this rule. If the variable is not None, this rule will be ignored.   |  `None` |
| <a id="cc_nested_args-requires_true"></a>requires_true |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for truthiness before expanding this rule. If the variable is false, this rule will be ignored.   |  `None` |
| <a id="cc_nested_args-requires_false"></a>requires_false |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for falsiness before expanding this rule. If the variable is true, this rule will be ignored.   |  `None` |
| <a id="cc_nested_args-requires_equal"></a>requires_equal |  (Label) The label of a [`cc_variable`](#cc_variable) that should be checked for equality before expanding this rule. If the variable is not equal to (requires_equal_value)[#cc_nested_args-requires_equal_value], this rule will be ignored.   |  `None` |
| <a id="cc_nested_args-requires_equal_value"></a>requires_equal_value |  (str) The value to compare (requires_equal)[#cc_nested_args-requires_equal] against.   |  `None` |
| <a id="cc_nested_args-kwargs"></a>kwargs |  [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.   |  none |


<a id="cc_tool_map"></a>

## cc_tool_map

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_tool_map")

cc_tool_map(<a href="#cc_tool_map-name">name</a>, <a href="#cc_tool_map-tools">tools</a>, <a href="#cc_tool_map-kwargs">**kwargs</a>)
</pre>

A toolchain configuration rule that maps toolchain actions to tools.

A [`cc_tool_map`](#cc_tool_map) aggregates all the tools that may be used for a given toolchain
and maps them to their corresponding actions. Conceptually, this is similar to the
`CXX=/path/to/clang++` environment variables that most build systems use to determine which
tools to use for a given action. To simplify usage, some actions have been grouped together (for
example,
[@rules_cc//cc/toolchains/actions:cpp_compile_actions](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/actions/BUILD)) to
logically express "all the C++ compile actions".

In Bazel, there is a little more granularity to the mapping, so the mapping doesn't follow the
traditional `CXX`, `AR`, etc. naming scheme. For a comprehensive list of all the well-known
actions, see @rules_cc//cc/toolchains/actions:BUILD.

Example usage:
```
load("@rules_cc//cc/toolchains:tool_map.bzl", "cc_tool_map")

cc_tool_map(
    name = "all_tools",
    tools = {
        "@rules_cc//cc/toolchains/actions:assembly_actions": ":asm",
        "@rules_cc//cc/toolchains/actions:c_compile": ":clang",
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions": ":clang++",
        "@rules_cc//cc/toolchains/actions:link_actions": ":lld",
        "@rules_cc//cc/toolchains/actions:objcopy_embed_data": ":llvm-objcopy",
        "@rules_cc//cc/toolchains/actions:strip": ":llvm-strip",
        "@rules_cc//cc/toolchains/actions:ar_actions": ":llvm-ar",
    },
)
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_tool_map-name"></a>name |  (str) The name of the target.   |  none |
| <a id="cc_tool_map-tools"></a>tools |  (Dict[Label, Label]) A mapping between [`cc_action_type`](#cc_action_type)/[`cc_action_type_set`](#cc_action_type_set) targets and the [`cc_tool`](#cc_tool) or executable target that implements that action.   |  none |
| <a id="cc_tool_map-kwargs"></a>kwargs |  [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.   |  none |


<a id="cc_toolchain"></a>

## cc_toolchain

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_toolchain")

cc_toolchain(*, <a href="#cc_toolchain-name">name</a>, <a href="#cc_toolchain-tool_map">tool_map</a>, <a href="#cc_toolchain-args">args</a>, <a href="#cc_toolchain-known_features">known_features</a>, <a href="#cc_toolchain-enabled_features">enabled_features</a>, <a href="#cc_toolchain-libc_top">libc_top</a>, <a href="#cc_toolchain-module_map">module_map</a>,
             <a href="#cc_toolchain-dynamic_runtime_lib">dynamic_runtime_lib</a>, <a href="#cc_toolchain-static_runtime_lib">static_runtime_lib</a>, <a href="#cc_toolchain-supports_header_parsing">supports_header_parsing</a>, <a href="#cc_toolchain-supports_param_files">supports_param_files</a>,
             <a href="#cc_toolchain-compiler">compiler</a>, <a href="#cc_toolchain-kwargs">**kwargs</a>)
</pre>

A C/C++ toolchain configuration.

This rule is the core declaration of a complete C/C++ toolchain. It collects together
tool configuration, which arguments to pass to each tool, and how
[features](https://bazel.build/docs/cc-toolchain-config-reference#features)
(dynamically-toggleable argument lists) interact.

A single [`cc_toolchain`](#cc_toolchain) may support a wide variety of platforms and configurations through
[configurable build attributes](https://bazel.build/docs/configurable-attributes) and
[feature relationships](https://bazel.build/docs/cc-toolchain-config-reference#feature-relationships).

Arguments are applied to commandline invocation of tools in the following order:

1. Arguments in the order they are listed in listed in [`args`](#cc_toolchain-args).
2. Any legacy/built-in features that have been implicitly or explicitly enabled.
3. User-defined features in the order they are listed in
   [`known_features`](#cc_toolchain-known_features).

When building a [`cc_toolchain`](#cc_toolchain) configuration, it's important to understand how `select`
statements will be evaluated:

* Most attributes and dependencies of a [`cc_toolchain`](#cc_toolchain) are evaluated under the target platform.
  This means that a `@platforms//os:linux` constraint will be satisfied when
  the final compiled binaries are intended to be ran from a Linux machine. This means that
  a different operating system (e.g. Windows) may be cross-compiling to linux.
* The [`cc_tool_map`](#cc_tool_map) rule performs a transition to the exec platform when evaluating tools. This
  means that a if a `@platforms//os:linux` constraint is satisfied in a
  `select` statement on a [`cc_tool`](#cc_tool), that means the machine that will run the tool is a Linux
  machine. This means that a Linux machine may be cross-compiling to a different OS
  like Windows.

Generated rules:
    {name}: A [`cc_toolchain`](#cc_toolchain) for this toolchain.
    _{name}_config: A `cc_toolchain_config` for this toolchain.
    _{name}_*_files: Generated rules that group together files for
        "ar_files", "as_files", "compiler_files", "coverage_files",
        "dwp_files", "linker_files", "objcopy_files", and "strip_files"
        normally enumerated as part of the [`cc_toolchain`](#cc_toolchain) rule.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_toolchain-name"></a>name |  (str) The name of the label for the toolchain.   |  none |
| <a id="cc_toolchain-tool_map"></a>tool_map |  (Label) The [`cc_tool_map`](#cc_tool_map) that specifies the tools to use for various toolchain actions.   |  `None` |
| <a id="cc_toolchain-args"></a>args |  (List[Label]) A list of [`cc_args`](#cc_args) and `cc_arg_list` to apply across this toolchain.   |  `[]` |
| <a id="cc_toolchain-known_features"></a>known_features |  (List[Label]) A list of [`cc_feature`](#cc_feature) rules that this toolchain supports. Whether or not these [features](https://bazel.build/docs/cc-toolchain-config-reference#features) are enabled may change over the course of a build. See the documentation for [`cc_feature`](#cc_feature) for more information.   |  `[]` |
| <a id="cc_toolchain-enabled_features"></a>enabled_features |  (List[Label]) A list of [`cc_feature`](#cc_feature) rules whose initial state should be `enabled`. Note that it is still possible for these [features](https://bazel.build/docs/cc-toolchain-config-reference#features) to be disabled over the course of a build through other mechanisms. See the documentation for [`cc_feature`](#cc_feature) for more information.   |  `[]` |
| <a id="cc_toolchain-libc_top"></a>libc_top |  (Label) A collection of artifacts for libc passed as inputs to compile/linking actions. See [`cc_toolchain.libc_top`](https://bazel.build/reference/be/c-cpp#cc_toolchain.libc_top) for more information.   |  `None` |
| <a id="cc_toolchain-module_map"></a>module_map |  (Label) Module map artifact to be used for modular builds. See [`cc_toolchain.module_map`](https://bazel.build/reference/be/c-cpp#cc_toolchain.module_map) for more information.   |  `None` |
| <a id="cc_toolchain-dynamic_runtime_lib"></a>dynamic_runtime_lib |  (Label) Dynamic library to link when the `static_link_cpp_runtimes` and `dynamic_linking_mode` [features](https://bazel.build/docs/cc-toolchain-config-reference#features) are both enabled. See [`cc_toolchain.dynamic_runtime_lib`](https://bazel.build/reference/be/c-cpp#cc_toolchain.dynamic_runtime_lib) for more information.   |  `None` |
| <a id="cc_toolchain-static_runtime_lib"></a>static_runtime_lib |  (Label) Static library to link when the `static_link_cpp_runtimes` and `static_linking_mode` [features](https://bazel.build/docs/cc-toolchain-config-reference#features) are both enabled. See [`cc_toolchain.dynamic_runtime_lib`](https://bazel.build/reference/be/c-cpp#cc_toolchain.dynamic_runtime_lib) for more information.   |  `None` |
| <a id="cc_toolchain-supports_header_parsing"></a>supports_header_parsing |  (bool) Whether or not this toolchain supports header parsing actions. See [`cc_toolchain.supports_header_parsing`](https://bazel.build/reference/be/c-cpp#cc_toolchain.supports_header_parsing) for more information.   |  `False` |
| <a id="cc_toolchain-supports_param_files"></a>supports_param_files |  (bool) Whether or not this toolchain supports linking via param files. See [`cc_toolchain.supports_param_files`](https://bazel.build/reference/be/c-cpp#cc_toolchain.supports_param_files) for more information.   |  `False` |
| <a id="cc_toolchain-compiler"></a>compiler |  (str) The type of compiler used by this toolchain (e.g. "gcc", "clang"). The current toolchain's compiler is exposed to `@rules_cc//cc/private/toolchain:compiler (compiler_flag)` as a flag value.   |  `""` |
| <a id="cc_toolchain-kwargs"></a>kwargs |  [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to all rules created by this macro.   |  none |


<a id="cc_variable"></a>

## cc_variable

<pre>
load("@rules_cc//cc/toolchains/impl:documented_api.bzl", "cc_variable")

cc_variable(<a href="#cc_variable-name">name</a>, <a href="#cc_variable-type">type</a>, <a href="#cc_variable-kwargs">**kwargs</a>)
</pre>

Exposes a toolchain variable to use in toolchain argument expansions.

This internal rule exposes [toolchain variables](https://bazel.build/docs/cc-toolchain-config-reference#cctoolchainconfiginfo-build-variables)
that may be expanded in [`cc_args`](#cc_args) or [`cc_nested_args`](#cc_nested_args)
rules. Because these varaibles merely expose variables inherrent to Bazel,
it's not possible to declare custom variables.

For a full list of available variables, see
[@rules_cc//cc/toolchains/varaibles:BUILD](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/variables/BUILD).

Example:
```
load("@rules_cc//cc/toolchains/impl:variables.bzl", "cc_variable")

# Defines two targets, ":foo" and ":foo.bar"
cc_variable(
    name = "foo",
    type = types.list(types.struct(bar = types.string)),
)
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_variable-name"></a>name |  (str) The name of the outer variable, and the rule.   |  none |
| <a id="cc_variable-type"></a>type |  The type of the variable, constructed using `types` factory in [@rules_cc//cc/toolchains/impl:variables.bzl](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/impl/variables.bzl).   |  none |
| <a id="cc_variable-kwargs"></a>kwargs |  [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes) that should be applied to this rule.   |  none |


