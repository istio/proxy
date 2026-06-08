<!-- Generated with Stardoc, Do Not Edit! -->

Bazel rules to define Swift libraries and executable binaries.

Users should load these rules from `.bzl` files under the `swift` and `proto`
directories. Do not import definitions from the `internal` subdirectory
directly.

For example:

```build
load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")
load("@build_bazel_rules_swift//proto:swift_proto_library.bzl", "swift_proto_library")
```
On this page:

  * [swift_binary](#swift_binary)
  * [swift_compiler_plugin](#swift_compiler_plugin)
  * [universal_swift_compiler_plugin](#universal_swift_compiler_plugin)
  * [swift_compiler_plugin_import](#swift_compiler_plugin_import)
  * [swift_cross_import_overlay](#swift_cross_import_overlay)
  * [swift_feature_allowlist](#swift_feature_allowlist)
  * [swift_import](#swift_import)
  * [swift_interop_hint](#swift_interop_hint)
  * [swift_library](#swift_library)
  * [swift_library_group](#swift_library_group)
  * [mixed_language_library](#mixed_language_library)
  * [swift_module_alias](#swift_module_alias)
  * [swift_module_mapping](#swift_module_mapping)
  * [swift_module_mapping_test](#swift_module_mapping_test)
  * [swift_package_configuration](#swift_package_configuration)
  * [swift_test](#swift_test)
  * [swift_proto_library](#swift_proto_library)
  * [swift_proto_library_group](#swift_proto_library_group)
  * [swift_proto_compiler](#swift_proto_compiler)
  * [deprecated_swift_grpc_library](#deprecated_swift_grpc_library)
  * [deprecated_swift_proto_library](#deprecated_swift_proto_library)

<a id="swift_binary"></a>

## swift_binary

<pre>
swift_binary(<a href="#swift_binary-name">name</a>, <a href="#swift_binary-deps">deps</a>, <a href="#swift_binary-srcs">srcs</a>, <a href="#swift_binary-data">data</a>, <a href="#swift_binary-copts">copts</a>, <a href="#swift_binary-defines">defines</a>, <a href="#swift_binary-linkopts">linkopts</a>, <a href="#swift_binary-malloc">malloc</a>, <a href="#swift_binary-module_name">module_name</a>, <a href="#swift_binary-package_name">package_name</a>,
             <a href="#swift_binary-plugins">plugins</a>, <a href="#swift_binary-stamp">stamp</a>, <a href="#swift_binary-swiftc_inputs">swiftc_inputs</a>)
</pre>

Compiles and links Swift code into an executable binary.

On Linux, this rule produces an executable binary for the desired target
architecture.

On Apple platforms, this rule produces a _single-architecture_ binary; it does
not produce fat binaries. As such, this rule is mainly useful for creating Swift
tools intended to run on the local build machine.

If you want to create a multi-architecture binary or a bundled application,
please use one of the platform-specific application rules in
[rules_apple](https://github.com/bazelbuild/rules_apple) instead of
`swift_binary`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_binary-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_binary-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_binary-srcs"></a>srcs |  A list of `.swift` source files that will be compiled into the library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_binary-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_binary-copts"></a>copts |  Additional compiler options that should be passed to `swiftc`. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_binary-defines"></a>defines |  A list of defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, **not** `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   | List of strings | optional |  `[]`  |
| <a id="swift_binary-linkopts"></a>linkopts |  Additional linker options that should be passed to `clang`. These strings are subject to `$(location ...)` expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_binary-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Swift binaries are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="swift_binary-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_binary-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same package_name can access APIs using the 'package' access control modifier in Swift 5.9+.   | String | optional |  `""`  |
| <a id="swift_binary-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling this module and any modules that directly depend on it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_binary-stamp"></a>stamp |  Enable or disable link stamping; that is, whether to encode build information into the binary. Possible values are:<br><br>* `stamp = 1`: Stamp the build information into the binary. Stamped binaries are   only rebuilt when their dependencies change. Use this if there are tests that   depend on the build information.<br><br>* `stamp = 0`: Always replace build information by constant values. This gives   good build result caching.<br><br>* `stamp = -1`: Embedding of build information is controlled by the   `--[no]stamp` flag.   | Integer | optional |  `-1`  |
| <a id="swift_binary-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="swift_compiler_plugin"></a>

## swift_compiler_plugin

<pre>
swift_compiler_plugin(<a href="#swift_compiler_plugin-name">name</a>, <a href="#swift_compiler_plugin-deps">deps</a>, <a href="#swift_compiler_plugin-srcs">srcs</a>, <a href="#swift_compiler_plugin-data">data</a>, <a href="#swift_compiler_plugin-copts">copts</a>, <a href="#swift_compiler_plugin-defines">defines</a>, <a href="#swift_compiler_plugin-linkopts">linkopts</a>, <a href="#swift_compiler_plugin-malloc">malloc</a>, <a href="#swift_compiler_plugin-module_name">module_name</a>,
                      <a href="#swift_compiler_plugin-package_name">package_name</a>, <a href="#swift_compiler_plugin-plugins">plugins</a>, <a href="#swift_compiler_plugin-stamp">stamp</a>, <a href="#swift_compiler_plugin-swiftc_inputs">swiftc_inputs</a>)
</pre>

Compiles and links a Swift compiler plugin (for example, a macro).

A compiler plugin is a standalone executable that minimally implements the
`CompilerPlugin` protocol from the `SwiftCompilerPlugin` module in swift-syntax.
As of the time of this writing (Xcode 15.0), a compiler plugin can contain one
or more macros, which can be associated with other Swift targets to perform
syntax-tree-based expansions.

When a `swift_compiler_plugin` target is listed in the `plugins` attribute of a
`swift_library`, it will be loaded by that library and any targets that directly
depend on it. (The `plugins` attribute also exists on `swift_binary`,
`swift_test`, and `swift_compiler_plugin` itself, to support plugins that are
only used within those targets.)

Compiler plugins also support being built as a library so that they can be
tested. The `swift_test` rule can contain `swift_compiler_plugin` targets in its
`deps`, and the plugin's module can be imported by the test's sources so that
unit tests can be written against the plugin.

Example:

```bzl
# The actual macro code, using SwiftSyntax
swift_compiler_plugin(
    name = "Macros",
    srcs = glob(["Macros/*.swift"]),
    deps = [
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftCompilerPlugin",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)

# A target testing the macro itself
swift_test(
    name = "MacrosTests",
    srcs = glob(["MacrosTests/*.swift"]),
    deps = [
        ":Macros",
        "@SwiftSyntax//:SwiftSyntaxMacrosTestSupport",
    ],
)

# The library that defines the macro hook for use in your project
swift_library(
    name = "MacroLibrary",
    srcs = glob(["MacroLibrary/*.swift"]),
    plugins = [":Macros"],
)

# A consumer of the macro library. This doesn't have to be separate from the
# MacroLibrary depending on what makes sense for your project's organization
swift_library(
    name = "MacroConsumer",
    srcs = glob(["Sources/*.swift"]),
    deps = [":MacroLibrary"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_compiler_plugin-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_compiler_plugin-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_compiler_plugin-srcs"></a>srcs |  A list of `.swift` source files that will be compiled into the library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_compiler_plugin-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_compiler_plugin-copts"></a>copts |  Additional compiler options that should be passed to `swiftc`. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_compiler_plugin-defines"></a>defines |  A list of defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, **not** `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   | List of strings | optional |  `[]`  |
| <a id="swift_compiler_plugin-linkopts"></a>linkopts |  Additional linker options that should be passed to `clang`. These strings are subject to `$(location ...)` expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_compiler_plugin-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Swift binaries are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="swift_compiler_plugin-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_compiler_plugin-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same package_name can access APIs using the 'package' access control modifier in Swift 5.9+.   | String | optional |  `""`  |
| <a id="swift_compiler_plugin-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling this module and any modules that directly depend on it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_compiler_plugin-stamp"></a>stamp |  Enable or disable link stamping; that is, whether to encode build information into the binary. Possible values are:<br><br>* `stamp = 1`: Stamp the build information into the binary. Stamped binaries are   only rebuilt when their dependencies change. Use this if there are tests that   depend on the build information.<br><br>* `stamp = 0`: Always replace build information by constant values. This gives   good build result caching.<br><br>* `stamp = -1`: Embedding of build information is controlled by the   `--[no]stamp` flag.   | Integer | optional |  `0`  |
| <a id="swift_compiler_plugin-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="swift_compiler_plugin_import"></a>

## swift_compiler_plugin_import

<pre>
swift_compiler_plugin_import(<a href="#swift_compiler_plugin_import-name">name</a>, <a href="#swift_compiler_plugin_import-executable">executable</a>, <a href="#swift_compiler_plugin_import-module_names">module_names</a>)
</pre>

Allows for a Swift compiler plugin to be loaded from a prebuilt executable or
some other binary-propagating rule, instead of building the plugin from source
using `swift_compiler_plugin`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_compiler_plugin_import-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_compiler_plugin_import-executable"></a>executable |  The compiler plugin executable that will be passed to the Swift compiler when compiling any modules that depend on the plugin. This attribute may refer directly to an executable binary or to another rule that produces an executable binary.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="swift_compiler_plugin_import-module_names"></a>module_names |  The list of names of Swift modules in the plugin executable that provide implementations of plugin types, which the compiler uses to look up their implementations.   | List of strings | required |  |


<a id="swift_cross_import_overlay"></a>

## swift_cross_import_overlay

<pre>
swift_cross_import_overlay(<a href="#swift_cross_import_overlay-name">name</a>, <a href="#swift_cross_import_overlay-deps">deps</a>, <a href="#swift_cross_import_overlay-bystanding_module">bystanding_module</a>, <a href="#swift_cross_import_overlay-declaring_module">declaring_module</a>)
</pre>

Declares a cross-import overlay that will be automatically added as a dependency
by the toolchain if its declaring and bystanding modules are both imported.

Since Bazel requires the dependency graph to be explicit, cross-import overlays
do not work correctly when the Swift compiler attempts to import them
automatically when they aren't represented in the graph. Users can explicitly
depend on the cross-import overlay module, but this is unsatisfying because
there is no single `import` declaration in the source code that indicates what
needs to be depended on.

To address this, the toolchain owner can define a `swift_cross_import_overlay`
target for each cross-import overlay that they wish to support and set them as
`cross_import_overlays` on the toolchain. During Swift compilation analysis, the
direct dependencies will be scanned and if any pair of dependencies matches a
cross-import overlay defined by the toolchain, the overlay module will be
automatically injected as a dependency as well.

NOTE: This rule and its associated APIs only exists to support cross-import
overlays _already defined by Apple's SDKs_. Since cross-import overlays are not
a public feature of the compiler and its design and implementation may change in
the future, this rule is not recommended for other widespread use.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_cross_import_overlay-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_cross_import_overlay-deps"></a>deps |  A non-empty list of targets representing modules that should be passed as dependencies when a target depends on both `declaring_module` and `bystanding_module`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="swift_cross_import_overlay-bystanding_module"></a>bystanding_module |  A label for the target representing the second of the two modules (the other being `declaring_module`) that must be imported for the cross-import overlay modules to be imported. It is completely passive in the cross-import process, having no definition with or other association to either the declaring module or the cross-import modules.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="swift_cross_import_overlay-declaring_module"></a>declaring_module |  A label for the target representing the first of the two modules (the other being `bystanding_module`) that must be imported for the cross-import overlay modules to be imported. This is the module that contains the `.swiftcrossimport` overlay definition that connects it to the bystander and to the overlay modules.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="swift_feature_allowlist"></a>

## swift_feature_allowlist

<pre>
swift_feature_allowlist(<a href="#swift_feature_allowlist-name">name</a>, <a href="#swift_feature_allowlist-aspect_ids">aspect_ids</a>, <a href="#swift_feature_allowlist-managed_features">managed_features</a>, <a href="#swift_feature_allowlist-packages">packages</a>)
</pre>

Limits the ability to request or disable certain features to a set of packages
(and possibly subpackages) in the workspace.

A Swift toolchain target can reference any number (zero or more) of
`swift_feature_allowlist` targets. The features managed by these allowlists may
overlap. For some package _P_, a feature is allowed to be used by targets in
that package if _P_ matches the `packages` patterns in *all* of the allowlists
that manage that feature.

A feature that is not managed by any allowlist is allowed to be used by any
package.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_feature_allowlist-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_feature_allowlist-aspect_ids"></a>aspect_ids |  A list of strings representing the identifiers of aspects that are allowed to enable/disable the features in `managed_features`, even when the aspect is applied to packages not covered by the `packages` attribute.<br><br>Aspect identifiers are each expected to be of the form `<.bzl file label>%<aspect top-level name>` (i.e., the form one would use if invoking it from the command line, as described at https://bazel.build/extending/aspects#invoking_the_aspect_using_the_command_line).   | List of strings | optional |  `[]`  |
| <a id="swift_feature_allowlist-managed_features"></a>managed_features |  A list of feature strings that are permitted to be specified by the targets in the packages matched by the `packages` attribute *or* an aspect whose name matches the `aspect_ids` attribute (in any package). This list may include both feature names and/or negations (a name with a leading `-`); a regular feature name means that the matching targets/aspects may explicitly request that the feature be enabled, and a negated feature means that the target may explicitly request that the feature be disabled.<br><br>For example, `managed_features = ["foo", "-bar"]` means that targets in the allowlist's packages/aspects may request that feature `"foo"` be enabled and that feature `"bar"` be disabled.   | List of strings | optional |  `[]`  |
| <a id="swift_feature_allowlist-packages"></a>packages |  A list of strings representing packages (possibly recursive) whose targets are allowed to enable/disable the features in `managed_features`. Each package pattern is written in the syntax used by the `package_group` function:<br><br>*   `//foo/bar`: Targets in the package `//foo/bar` but not in subpackages.<br><br>*   `//foo/bar/...`: Targets in the package `//foo/bar` and any of its     subpackages.<br><br>*   A leading `-` excludes packages that would otherwise have been included by     the patterns in the list.<br><br>Exclusions always take priority over inclusions; order in the list is irrelevant.   | List of strings | required |  |


<a id="swift_import"></a>

## swift_import

<pre>
swift_import(<a href="#swift_import-name">name</a>, <a href="#swift_import-deps">deps</a>, <a href="#swift_import-data">data</a>, <a href="#swift_import-archives">archives</a>, <a href="#swift_import-module_name">module_name</a>, <a href="#swift_import-plugins">plugins</a>, <a href="#swift_import-swiftdoc">swiftdoc</a>, <a href="#swift_import-swiftinterface">swiftinterface</a>,
             <a href="#swift_import-swiftmodule">swiftmodule</a>)
</pre>

Allows for the use of Swift textual module interfaces and/or precompiled Swift
modules as dependencies in other `swift_library` and `swift_binary` targets.

To use `swift_import` targets across Xcode versions and/or OS versions, it is
required to use `.swiftinterface` files. These can be produced by the pre-built
target if built with:

  - `--features=swift.enable_library_evolution`
  - `--features=swift.emit_swiftinterface`

If the pre-built target supports `.private.swiftinterface` files, these can be
used instead of `.swiftinterface` files in the `swiftinterface` attribute.

To import pre-built Swift modules that use `@_spi` when using `swiftinterface`,
the `.private.swiftinterface` files are required in order to build any code that
uses the API marked with `@_spi`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_import-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_import-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_import-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_import-archives"></a>archives |  The list of `.a` or `.lo` files provided to Swift targets that depend on this target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_import-module_name"></a>module_name |  The name of the module represented by this target.   | String | required |  |
| <a id="swift_import-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling any modules that directly depend on this target.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_import-swiftdoc"></a>swiftdoc |  The `.swiftdoc` file provided to Swift targets that depend on this target.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="swift_import-swiftinterface"></a>swiftinterface |  The `.swiftinterface` file that defines the module interface for this target. May not be specified if `swiftmodule` is specified.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="swift_import-swiftmodule"></a>swiftmodule |  The `.swiftmodule` file provided to Swift targets that depend on this target. May not be specified if `swiftinterface` is specified.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |


<a id="swift_interop_hint"></a>

## swift_interop_hint

<pre>
swift_interop_hint(<a href="#swift_interop_hint-name">name</a>, <a href="#swift_interop_hint-exclude_hdrs">exclude_hdrs</a>, <a href="#swift_interop_hint-module_map">module_map</a>, <a href="#swift_interop_hint-module_name">module_name</a>, <a href="#swift_interop_hint-suppressed">suppressed</a>)
</pre>

Defines an aspect hint that associates non-Swift BUILD targets with additional
information required for them to be imported by Swift.

> [!NOTE]
> Bazel 6 users must set the `--experimental_enable_aspect_hints` flag to utilize
> this rule. In addition, downstream consumers of rules that utilize this rule
> must also set the flag. The flag is enabled by default in Bazel 7.

Some build rules, such as `objc_library`, support interoperability with Swift
simply by depending on them; a module map is generated automatically. This is
for convenience, because the common case is that most `objc_library` targets
contain code that is compatible (i.e., capable of being imported) by Swift.

For other rules, like `cc_library`, additional information must be provided to
indicate that a particular target is compatible with Swift. This is done using
the `aspect_hints` attribute and the `swift_interop_hint` rule.

#### Using the automatically derived module name (recommended)

If you want to import a non-Swift, non-Objective-C target into Swift using the
module name that is automatically derived from the BUILD label, there is no need
to declare an instance of `swift_interop_hint`. A canonical one that requests
module name derivation has been provided in
`@build_bazel_rules_swift//swift:auto_module`. Simply add it to the `aspect_hints` of
the target you wish to import:

```build
# //my/project/BUILD
cc_library(
    name = "somelib",
    srcs = ["somelib.c"],
    hdrs = ["somelib.h"],
    aspect_hints = ["@build_bazel_rules_swift//swift:auto_module"],
)
```

When this `cc_library` is a dependency of a Swift target, a module map will be
generated for it. In this case, the module's name would be `my_project_somelib`.

#### Using an explicit module name

If you need to provide an explicit name for the module (for example, if it is
part of a third-party library that expects to be imported with a specific name),
then you can declare your own `swift_interop_hint` target to define the name:

```build
# //my/project/BUILD
cc_library(
    name = "somelib",
    srcs = ["somelib.c"],
    hdrs = ["somelib.h"],
    aspect_hints = [":somelib_swift_interop"],
)

swift_interop_hint(
    name = "somelib_swift_interop",
    module_name = "CSomeLib",
)
```

When this `cc_library` is a dependency of a Swift target, a module map will be
generated for it with the module name `CSomeLib`.

#### Using a custom module map

In rare cases, the automatically generated module map may not be suitable. For
example, a Swift module may depend on a C module that defines specific
submodules, and this is not handled by the Swift build rules. In this case, you
can provide the module map file using the `module_map` attribute.

When setting the `module_map` attribute, `module_name` must also be set to the
name of the desired top-level module; it cannot be omitted.

```build
# //my/project/BUILD
cc_library(
    name = "somelib",
    srcs = ["somelib.c"],
    hdrs = ["somelib.h"],
    aspect_hints = [":somelib_swift_interop"],
)

swift_interop_hint(
    name = "somelib_swift_interop",
    module_map = "module.modulemap",
    module_name = "CSomeLib",
)
```

#### Suppressing a module

As mentioned above, `objc_library` and other Objective-C targets generate
modules by default, without an explicit hint, for convenience. In some
situations, this behavior may not be desirable. For example, an `objc_library`
might contain only Objective-C++ code in its headers that would not be possible
to import into Swift at all.

When building with implicit modules, this is not typically an issue because the
module map would only be used if Swift code tried to import it (although it does
create useless actions and compiler inputs during the build). When building with
explicit modules, however, Bazel needs to know which targets represent modules
that it can compile and which do not.

In these cases, there is no need to declare an instance of `swift_interop_hint`.
A canonical one that suppresses module generation has been provided in
`@build_bazel_rules_swift//swift:no_module`. Simply add it to the `aspect_hints` of
the target whose module you wish to suppress:

```build
# //my/project/BUILD
objc_library(
    name = "somelib",
    srcs = ["somelib.mm"],
    hdrs = ["somelib.h"],
    aspect_hints = ["@build_bazel_rules_swift//swift:no_module"],
)
```

When this `objc_library` is a dependency of a Swift target, no module map or
explicit module will be generated for it, nor will any Swift information from
its transitive dependencies be propagated.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_interop_hint-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_interop_hint-exclude_hdrs"></a>exclude_hdrs |  A list of header files that should be excluded from the Clang module generated for the target to which this hint is applied. This allows a target to exclude a subset of a library's headers specifically from the Swift module map without removing them from the library completely, which can be useful if some headers are not Swift-compatible but are still needed by other sources in the library or by non-Swift dependents.<br><br>This attribute may only be specified if a custom `module_map` is not provided. Setting both attributes is an error.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_interop_hint-module_map"></a>module_map |  An optional custom `.modulemap` file that defines the Clang module for the headers in the target to which this hint is applied.<br><br>If this attribute is omitted, a module map will be automatically generated based on the headers in the hinted target.<br><br>If this attribute is provided, then `module_name` must also be provided and match the name of the desired top-level module in the `.modulemap` file. (A single `.modulemap` file may define multiple top-level modules.)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="swift_interop_hint-module_name"></a>module_name |  The name that will be used to import the hinted module into Swift.<br><br>If left unspecified, the module name will be computed based on the hinted target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_interop_hint-suppressed"></a>suppressed |  If `True`, the hinted target should suppress any module that it would otherwise generate.   | Boolean | optional |  `False`  |


<a id="swift_library"></a>

## swift_library

<pre>
swift_library(<a href="#swift_library-name">name</a>, <a href="#swift_library-deps">deps</a>, <a href="#swift_library-srcs">srcs</a>, <a href="#swift_library-data">data</a>, <a href="#swift_library-always_include_developer_search_paths">always_include_developer_search_paths</a>, <a href="#swift_library-alwayslink">alwayslink</a>, <a href="#swift_library-copts">copts</a>,
              <a href="#swift_library-defines">defines</a>, <a href="#swift_library-generated_header_name">generated_header_name</a>, <a href="#swift_library-generates_header">generates_header</a>, <a href="#swift_library-library_evolution">library_evolution</a>, <a href="#swift_library-linkopts">linkopts</a>,
              <a href="#swift_library-linkstatic">linkstatic</a>, <a href="#swift_library-module_name">module_name</a>, <a href="#swift_library-package_name">package_name</a>, <a href="#swift_library-plugins">plugins</a>, <a href="#swift_library-private_deps">private_deps</a>, <a href="#swift_library-swiftc_inputs">swiftc_inputs</a>)
</pre>

Compiles and links Swift code into a static library and Swift module.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_library-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_library-srcs"></a>srcs |  A list of `.swift` source files that will be compiled into the library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="swift_library-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_library-always_include_developer_search_paths"></a>always_include_developer_search_paths |  If `True`, the developer framework search paths will be added to the compilation command. This enables a Swift module to access `XCTest` without having to mark the target as `testonly = True`.   | Boolean | optional |  `False`  |
| <a id="swift_library-alwayslink"></a>alwayslink |  If true, any binary that depends (directly or indirectly) on this Swift module will link in all the object files for the files listed in `srcs`, even if some contain no symbols referenced by the binary. This is useful if your code isn't explicitly called by code in the binary; for example, if you rely on runtime checks for protocol conformances added in extensions in the library but do not directly reference any other symbols in the object file that adds that conformance.   | Boolean | optional |  `False`  |
| <a id="swift_library-copts"></a>copts |  Additional compiler options that should be passed to `swiftc`. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_library-defines"></a>defines |  A list of defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, **not** `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   | List of strings | optional |  `[]`  |
| <a id="swift_library-generated_header_name"></a>generated_header_name |  The name of the generated Objective-C interface header. This name must end with a `.h` extension and cannot contain any path separators.<br><br>If this attribute is not specified, then the default behavior is to name the header `${target_name}-Swift.h`.<br><br>This attribute is ignored if the toolchain does not support generating headers.   | String | optional |  `""`  |
| <a id="swift_library-generates_header"></a>generates_header |  If True, an Objective-C header will be generated for this target, in the same build package where the target is defined. By default, the name of the header is `${target_name}-Swift.h`; this can be changed using the `generated_header_name` attribute.<br><br>Targets should only set this attribute to True if they export Objective-C APIs. A header generated for a target that does not export Objective-C APIs will be effectively empty (except for a large amount of prologue and epilogue code) and this is generally wasteful because the extra file needs to be propagated in the build graph and, when explicit modules are enabled, extra actions must be executed to compile the Objective-C module for the generated header.   | Boolean | optional |  `False`  |
| <a id="swift_library-library_evolution"></a>library_evolution |  Indicates whether the Swift code should be compiled with library evolution mode enabled.<br><br>This attribute should be used to compile a module that will be distributed as part of a client-facing (non-implementation-only) module in a library or framework that will be distributed for use outside of the Bazel build graph. Setting this to true will compile the module with the `-library-evolution` flag and emit a `.swiftinterface` file as one of the compilation outputs.   | Boolean | optional |  `False`  |
| <a id="swift_library-linkopts"></a>linkopts |  Additional linker options that should be passed to the linker for the binary that depends on this target. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_library-linkstatic"></a>linkstatic |  If True, the Swift module will be built for static linking.  This will make all interfaces internal to the module that is being linked against and will inform the consuming module that the objects will be locally available (which may potentially avoid a PLT relocation).  Set to `False` to build a `.so` or `.dll`.   | Boolean | optional |  `True`  |
| <a id="swift_library-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_library-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same package_name can access APIs using the 'package' access control modifier in Swift 5.9+.   | String | optional |  `""`  |
| <a id="swift_library-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling this module and any modules that directly depend on it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_library-private_deps"></a>private_deps |  A list of targets that are implementation-only dependencies of the target being built. Libraries/linker flags from these dependencies will be propagated to dependent for linking, but artifacts/flags required for compilation (such as .swiftmodule files, C headers, and search paths) will not be propagated.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_library-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="swift_library_group"></a>

## swift_library_group

<pre>
swift_library_group(<a href="#swift_library_group-name">name</a>, <a href="#swift_library_group-deps">deps</a>)
</pre>

Groups Swift compatible libraries (e.g. `swift_library` and `objc_library`).
The target can be used anywhere a `swift_library` can be used. It behaves
similar to source-less `{cc,obj}_library` targets.

Unlike `swift_module_alias`, a new module isn't created for this target, you
need to import the grouped libraries directly.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_library_group-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_library_group-deps"></a>deps |  A list of targets that should be included in the group. Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="swift_module_alias"></a>

## swift_module_alias

<pre>
swift_module_alias(<a href="#swift_module_alias-name">name</a>, <a href="#swift_module_alias-deps">deps</a>, <a href="#swift_module_alias-module_name">module_name</a>)
</pre>

Creates a Swift module that re-exports other modules.

This rule effectively creates an "alias" for one or more modules such that a
client can import the alias module and it will implicitly import those
dependencies. It should be used primarily as a way to migrate users when a
module name is being changed. An alias that depends on more than one module can
be used to split a large module into smaller, more targeted modules.

Symbols in the original modules can be accessed through either the original
module name or the alias module name, so callers can be migrated separately
after moving the physical build target as needed. (An exception to this is
runtime type metadata, which only encodes the module name of the type where the
symbol is defined; it is not repeated by the alias module.)

> Caution: This rule uses the undocumented `@_exported` feature to re-export the
> `deps` in the new module. You depend on undocumented features at your own
> risk, as they may change in a future version of Swift.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_module_alias-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_module_alias-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target. Allowed kinds are `swift_import` and `swift_library` (or anything else propagating `SwiftInfo`).   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_module_alias-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |


<a id="swift_module_mapping"></a>

## swift_module_mapping

<pre>
swift_module_mapping(<a href="#swift_module_mapping-name">name</a>, <a href="#swift_module_mapping-aliases">aliases</a>)
</pre>

Defines a set of
[module aliases](https://github.com/apple/swift-evolution/blob/main/proposals/0339-module-aliasing-for-disambiguation.md)
that will be passed to the Swift compiler.

This rule defines a mapping from original module names to aliased names. This is
useful if you are building a library or framework for external use and want to
ensure that dependencies do not conflict with other versions of the same library
that another framework or the client may use.

To use this feature, first define a `swift_module_mapping` target that lists the
aliases you need:

```build
# //some/package/BUILD

swift_library(
    name = "Utils",
    srcs = [...],
    module_name = "Utils",
)

swift_library(
    name = "Framework",
    srcs = [...],
    module_name = "Framework",
    deps = [":Utils"],
)

swift_module_mapping(
    name = "mapping",
    aliases = {
        "Utils": "GameUtils",
    },
)
```

Then, pass the label of that target to Bazel using the
`--@build_bazel_rules_swift//swift:module_mapping` build flag:

```shell
bazel build //some/package:Framework \
    --@build_bazel_rules_swift//swift:module_mapping=//some/package:mapping
```

When `Utils` is compiled, it will be given the module name `GameUtils` instead.
Then, when `Framework` is compiled, it will import `GameUtils` anywhere that the
source asked to `import Utils`.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_module_mapping-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_module_mapping-aliases"></a>aliases |  A dictionary that remaps the names of Swift modules.<br><br>Each key in the dictionary is the name of a module as it is written in source code. The corresponding value is the replacement module name to use when compiling it and/or any modules that depend on it.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | required |  |


<a id="swift_module_mapping_test"></a>

## swift_module_mapping_test

<pre>
swift_module_mapping_test(<a href="#swift_module_mapping_test-name">name</a>, <a href="#swift_module_mapping_test-deps">deps</a>, <a href="#swift_module_mapping_test-exclude">exclude</a>, <a href="#swift_module_mapping_test-mapping">mapping</a>)
</pre>

Validates that a `swift_module_mapping` target covers all the modules in the
transitive closure of a list of dependencies.

If you are building a static library or framework for external distribution and
you are using `swift_module_mapping` to rename some of the modules used by your
implementation, this rule will detect if any of your dependencies have taken on
a new dependency that you need to add to the mapping (otherwise, its symbols
would leak into your library with their original names).

When executed, this test will collect the names of all Swift modules in the
transitive closure of `deps`. System modules and modules whose names are listed
in the `exclude` attribute are omitted. Then, the test will fail if any of the
remaining modules collected are not present in the `aliases` of the
`swift_module_mapping` target specified by the `mapping` attribute.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_module_mapping_test-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_module_mapping_test-deps"></a>deps |  A list of Swift targets whose transitive closure will be validated against the `swift_module_mapping` target specified by `mapping`.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="swift_module_mapping_test-exclude"></a>exclude |  A list of module names that may be in the transitive closure of `deps` but are not required to be covered by `mapping`.   | List of strings | optional |  `[]`  |
| <a id="swift_module_mapping_test-mapping"></a>mapping |  The label of a `swift_module_mapping` target against which the transitive closure of `deps` will be validated.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="swift_package_configuration"></a>

## swift_package_configuration

<pre>
swift_package_configuration(<a href="#swift_package_configuration-name">name</a>, <a href="#swift_package_configuration-configured_features">configured_features</a>, <a href="#swift_package_configuration-packages">packages</a>)
</pre>

A compilation configuration to apply to the Swift targets in a set of packages.

A Swift toolchain target can reference any number (zero or more) of
`swift_package_configuration` targets. When the compilation action for a target
is being configured, those package configurations will be applied if the
target's label is included by the package specifications in the configuration.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_package_configuration-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_package_configuration-configured_features"></a>configured_features |  A list of feature strings that will be applied by default to targets in the packages matched by the `packages` attribute, as if they had been specified by the `package(features = ...)` rule in the BUILD file.<br><br>This list may include both feature names and/or negations (a name with a leading `-`); a regular feature name means that the targets in the matching packages will have the feature enabled, and a negated feature means that the target will have the feature disabled.<br><br>For example, `configured_features = ["foo", "-bar"]` means that targets in the configuration's packages will have the feature `"foo"` enabled by default and the feature `"bar"` disabled by default.   | List of strings | optional |  `[]`  |
| <a id="swift_package_configuration-packages"></a>packages |  A list of strings representing packages (possibly recursive) whose targets will have this package configuration applied. Each package pattern is written in the syntax used by the `package_group` function:<br><br>*   `//foo/bar`: Targets in the package `//foo/bar` but not in subpackages.<br><br>*   `//foo/bar/...`: Targets in the package `//foo/bar` and any of its     subpackages.<br><br>*   A leading `-` excludes packages that would otherwise have been included by     the patterns in the list.<br><br>Exclusions always take priority over inclusions; order in the list is irrelevant.   | List of strings | required |  |


<a id="swift_proto_compiler"></a>

## swift_proto_compiler

<pre>
swift_proto_compiler(<a href="#swift_proto_compiler-name">name</a>, <a href="#swift_proto_compiler-deps">deps</a>, <a href="#swift_proto_compiler-bundled_proto_paths">bundled_proto_paths</a>, <a href="#swift_proto_compiler-plugin">plugin</a>, <a href="#swift_proto_compiler-plugin_name">plugin_name</a>, <a href="#swift_proto_compiler-plugin_option_allowlist">plugin_option_allowlist</a>,
                     <a href="#swift_proto_compiler-plugin_options">plugin_options</a>, <a href="#swift_proto_compiler-protoc">protoc</a>, <a href="#swift_proto_compiler-suffixes">suffixes</a>)
</pre>



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_proto_compiler-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_proto_compiler-deps"></a>deps |  List of targets providing SwiftInfo and CcInfo. Added as implicit dependencies for any swift_proto_library using this compiler. Typically, these are Well Known Types and proto runtime libraries.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_compiler-bundled_proto_paths"></a>bundled_proto_paths |  List of proto paths for which to skip generation because they're built into the modules imported by the generated Swift proto code, e.g., SwiftProtobuf.   | List of strings | optional |  `["google/protobuf/any.proto", "google/protobuf/api.proto", "google/protobuf/descriptor.proto", "google/protobuf/duration.proto", "google/protobuf/empty.proto", "google/protobuf/field_mask.proto", "google/protobuf/source_context.proto", "google/protobuf/struct.proto", "google/protobuf/timestamp.proto", "google/protobuf/type.proto", "google/protobuf/wrappers.proto"]`  |
| <a id="swift_proto_compiler-plugin"></a>plugin |  A proto compiler plugin executable binary.<br><br>For example: "//tools/protoc_wrapper:protoc-gen-grpc-swift" "//tools/protoc_wrapper:ProtoCompilerPlugin"   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="swift_proto_compiler-plugin_name"></a>plugin_name |  Name of the proto compiler plugin passed to protoc.<br><br>For example:<br><br><pre><code>protoc     --plugin=protoc-gen-NAME=path/to/plugin/binary</code></pre><br><br>This name will be used to prefix the option and output directory arguments. E.g.:<br><br><pre><code>protoc     --plugin=protoc-gen-NAME=path/to/mybinary     --NAME_out=OUT_DIR     --NAME_opt=Visibility=Public</code></pre><br><br>See the [protobuf API reference](https://protobuf.dev/reference/cpp/api-docs/google.protobuf.compiler.plugin) for more information.   | String | required |  |
| <a id="swift_proto_compiler-plugin_option_allowlist"></a>plugin_option_allowlist |  Allowlist of options allowed by the plugin. This is used to filter out any irrelevant plugin options passed down to the compiler from the library, which is especially useful when using multiple plugins in combination like GRPC and SwiftProtobuf.   | List of strings | required |  |
| <a id="swift_proto_compiler-plugin_options"></a>plugin_options |  Dictionary of plugin options passed to the plugin.<br><br>These are prefixed with the plugin_name + "_opt". E.g.:<br><br><pre><code>plugin_name = "swift"&#10;plugin_options = {&#10;    "Visibility": "Public",&#10;    "FileNaming": "FullPath",&#10;}</code></pre><br><br>Would be passed to protoc as:<br><br><pre><code>protoc     --plugin=protoc-gen-NAME=path/to/plugin/binary     --NAME_opt=Visibility=Public     --NAME_opt=FileNaming=FullPath</code></pre>   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | required |  |
| <a id="swift_proto_compiler-protoc"></a>protoc |  A proto compiler executable binary.<br><br>E.g. "//tools/protoc_wrapper:protoc"   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="swift_proto_compiler-suffixes"></a>suffixes |  Suffix used for Swift files generated by the plugin from protos.<br><br>E.g.<br><br><pre><code>foo.proto =&gt; foo.pb.swift&#10;foo_service.proto =&gt; foo.grpc.swift</code></pre><br><br>Each compiler target should configure this based on the suffix applied to the generated files.   | List of strings | required |  |


<a id="swift_proto_library"></a>

## swift_proto_library

<pre>
swift_proto_library(<a href="#swift_proto_library-name">name</a>, <a href="#swift_proto_library-deps">deps</a>, <a href="#swift_proto_library-srcs">srcs</a>, <a href="#swift_proto_library-data">data</a>, <a href="#swift_proto_library-additional_compiler_deps">additional_compiler_deps</a>, <a href="#swift_proto_library-additional_compiler_info">additional_compiler_info</a>,
                    <a href="#swift_proto_library-always_include_developer_search_paths">always_include_developer_search_paths</a>, <a href="#swift_proto_library-alwayslink">alwayslink</a>, <a href="#swift_proto_library-compilers">compilers</a>, <a href="#swift_proto_library-copts">copts</a>, <a href="#swift_proto_library-defines">defines</a>,
                    <a href="#swift_proto_library-generated_header_name">generated_header_name</a>, <a href="#swift_proto_library-generates_header">generates_header</a>, <a href="#swift_proto_library-library_evolution">library_evolution</a>, <a href="#swift_proto_library-linkopts">linkopts</a>, <a href="#swift_proto_library-linkstatic">linkstatic</a>,
                    <a href="#swift_proto_library-module_name">module_name</a>, <a href="#swift_proto_library-package_name">package_name</a>, <a href="#swift_proto_library-plugins">plugins</a>, <a href="#swift_proto_library-protos">protos</a>, <a href="#swift_proto_library-swiftc_inputs">swiftc_inputs</a>)
</pre>

Generates a Swift static library from one or more targets producing `ProtoInfo`.

```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "foo",
    srcs = ["foo.proto"],
)

swift_proto_library(
    name = "foo_swift",
    protos = [":foo"],
)
```

If your protos depend on protos from other targets, add dependencies between the
swift_proto_library targets which mirror the dependencies between the proto targets.

```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "bar",
    srcs = ["bar.proto"],
    deps = [":foo"],
)

swift_proto_library(
    name = "bar_swift",
    protos = [":bar"],
    deps = [":foo_swift"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_proto_library-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_proto_library-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-srcs"></a>srcs |  A list of `.swift` source files that will be compiled into the library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-additional_compiler_deps"></a>additional_compiler_deps |  List of additional dependencies required by the generated Swift code at compile time, whose SwiftProtoInfo will be ignored.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-additional_compiler_info"></a>additional_compiler_info |  Dictionary of additional information passed to the compiler targets. See the documentation of the respective compiler rules for more information on which fields are accepted and how they are used.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="swift_proto_library-always_include_developer_search_paths"></a>always_include_developer_search_paths |  If `True`, the developer framework search paths will be added to the compilation command. This enables a Swift module to access `XCTest` without having to mark the target as `testonly = True`.   | Boolean | optional |  `False`  |
| <a id="swift_proto_library-alwayslink"></a>alwayslink |  If true, any binary that depends (directly or indirectly) on this Swift module will link in all the object files for the files listed in `srcs`, even if some contain no symbols referenced by the binary. This is useful if your code isn't explicitly called by code in the binary; for example, if you rely on runtime checks for protocol conformances added in extensions in the library but do not directly reference any other symbols in the object file that adds that conformance.   | Boolean | optional |  `False`  |
| <a id="swift_proto_library-compilers"></a>compilers |  One or more `swift_proto_compiler` targets (or targets producing `SwiftProtoCompilerInfo`), from which the Swift protos will be generated.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `["@rules_swift//proto/compilers:swift_proto"]`  |
| <a id="swift_proto_library-copts"></a>copts |  Additional compiler options that should be passed to `swiftc`. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_proto_library-defines"></a>defines |  A list of defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, **not** `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   | List of strings | optional |  `[]`  |
| <a id="swift_proto_library-generated_header_name"></a>generated_header_name |  The name of the generated Objective-C interface header. This name must end with a `.h` extension and cannot contain any path separators.<br><br>If this attribute is not specified, then the default behavior is to name the header `${target_name}-Swift.h`.<br><br>This attribute is ignored if the toolchain does not support generating headers.   | String | optional |  `""`  |
| <a id="swift_proto_library-generates_header"></a>generates_header |  If True, an Objective-C header will be generated for this target, in the same build package where the target is defined. By default, the name of the header is `${target_name}-Swift.h`; this can be changed using the `generated_header_name` attribute.<br><br>Targets should only set this attribute to True if they export Objective-C APIs. A header generated for a target that does not export Objective-C APIs will be effectively empty (except for a large amount of prologue and epilogue code) and this is generally wasteful because the extra file needs to be propagated in the build graph and, when explicit modules are enabled, extra actions must be executed to compile the Objective-C module for the generated header.   | Boolean | optional |  `False`  |
| <a id="swift_proto_library-library_evolution"></a>library_evolution |  Indicates whether the Swift code should be compiled with library evolution mode enabled.<br><br>This attribute should be used to compile a module that will be distributed as part of a client-facing (non-implementation-only) module in a library or framework that will be distributed for use outside of the Bazel build graph. Setting this to true will compile the module with the `-library-evolution` flag and emit a `.swiftinterface` file as one of the compilation outputs.   | Boolean | optional |  `False`  |
| <a id="swift_proto_library-linkopts"></a>linkopts |  Additional linker options that should be passed to the linker for the binary that depends on this target. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_proto_library-linkstatic"></a>linkstatic |  If True, the Swift module will be built for static linking.  This will make all interfaces internal to the module that is being linked against and will inform the consuming module that the objects will be locally available (which may potentially avoid a PLT relocation).  Set to `False` to build a `.so` or `.dll`.   | Boolean | optional |  `True`  |
| <a id="swift_proto_library-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_proto_library-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same package_name can access APIs using the 'package' access control modifier in Swift 5.9+.   | String | optional |  `""`  |
| <a id="swift_proto_library-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling this module and any modules that directly depend on it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-protos"></a>protos |  A list of `proto_library` targets (or targets producing `ProtoInfo`), from which the Swift source files should be generated.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_proto_library-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="swift_proto_library_group"></a>

## swift_proto_library_group

<pre>
swift_proto_library_group(<a href="#swift_proto_library_group-name">name</a>, <a href="#swift_proto_library_group-compiler">compiler</a>, <a href="#swift_proto_library_group-proto">proto</a>)
</pre>

Generates a collection of Swift static library from a target producing `ProtoInfo` and its dependencies.

This rule is intended to facilitate migration from the deprecated swift proto library rules to the new ones.
Unlike `swift_proto_library` which is a drop-in-replacement for `swift_library`,
this rule uses an aspect over the direct proto library dependency and its transitive dependencies,
compiling each into a swift static library.

For example, in the following targets, the `proto_library_group_swift_proto` target only depends on
`package_2_proto` target, and this transitively depends on `package_1_proto`.

When used as a dependency from a `swift_library` or `swift_binary` target,
two modules generated from these proto library targets are visible.

Because these are derived from the proto library targets via an aspect, though,
you cannot customize many of the attributes including the swift proto compiler targets or
the module names. The module names are derived from the proto library names.

In this case, the module names are:
```
import examples_xplatform_proto_library_group_package_1_package_1_proto
import examples_xplatform_proto_library_group_package_2_package_2_proto
```

For this reason, we would encourage new consumers of the proto rules to use
`swift_proto_library` when possible.

```python
proto_library(
    name = "package_1_proto",
    srcs = glob(["*.proto"]),
    visibility = ["//visibility:public"],
)

...

proto_library(
    name = "package_2_proto",
    srcs = glob(["*.proto"]),
    visibility = ["//visibility:public"],
    deps = ["//examples/xplatform/proto_library_group/package_1:package_1_proto"],
)

...

swift_proto_library_group(
    name = "proto_library_group_swift_proto",
    proto = "//examples/xplatform/proto_library_group/package_2:package_2_proto",
)

...

swift_binary(
    name = "proto_library_group_example",
    srcs = ["main.swift"],
    deps = [
        ":proto_library_group_swift_proto",
    ],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_proto_library_group-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_proto_library_group-compiler"></a>compiler |  A `swift_proto_compiler` target (or target producing `SwiftProtoCompilerInfo`), from which the Swift protos will be generated.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_swift//proto/compilers:swift_proto"`  |
| <a id="swift_proto_library_group-proto"></a>proto |  Exactly one `proto_library` target (or target producing `ProtoInfo`), from which the Swift source files should be generated.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="swift_test"></a>

## swift_test

<pre>
swift_test(<a href="#swift_test-name">name</a>, <a href="#swift_test-deps">deps</a>, <a href="#swift_test-srcs">srcs</a>, <a href="#swift_test-data">data</a>, <a href="#swift_test-copts">copts</a>, <a href="#swift_test-defines">defines</a>, <a href="#swift_test-discover_tests">discover_tests</a>, <a href="#swift_test-env">env</a>, <a href="#swift_test-linkopts">linkopts</a>, <a href="#swift_test-malloc">malloc</a>,
           <a href="#swift_test-module_name">module_name</a>, <a href="#swift_test-package_name">package_name</a>, <a href="#swift_test-plugins">plugins</a>, <a href="#swift_test-stamp">stamp</a>, <a href="#swift_test-swiftc_inputs">swiftc_inputs</a>)
</pre>

Compiles and links Swift code into an executable test target.

### XCTest Test Discovery

By default, this rule performs _test discovery_ that finds tests written with
the `XCTest` framework and executes them automatically, without the user
providing their own `main` entry point.

On Apple platforms, `XCTest`-style tests are automatically discovered and
executed using the Objective-C runtime. To provide the same behavior on Linux,
the `swift_test` rule performs its own scan for `XCTest`-style tests. In other
words, you can write a single `swift_test` target that executes the same tests
on either Linux or Apple platforms.

There are two approaches that one can take to write a `swift_test` that supports
test discovery:

1.  **Preferred approach:** Write a `swift_test` target whose `srcs` contain
    your tests. In this mode, only these sources will be scanned for tests;
    direct dependencies will _not_ be scanned.

2.  Write a `swift_test` target with _no_ `srcs`. In this mode, all _direct_
    dependencies of the target will be scanned for tests; indirect dependencies
    will _not_ be scanned. This approach is useful if you want to share tests
    with an Apple-specific test target like `ios_unit_test`.

See the documentation of the `discover_tests` attribute for more information
about how this behavior affects the rule's outputs.
```

### Xcode Integration

If integrating with Xcode, the relative paths in test binaries can prevent the
Issue navigator from working for test failures. To work around this, you can
have the paths made absolute via swizzling by enabling the
`"apple.swizzle_absolute_xcttestsourcelocation"` feature. You'll also need to
set the `BUILD_WORKSPACE_DIRECTORY` environment variable in your scheme to the
root of your workspace (i.e. `$(SRCROOT)`).

### Test Filtering

`swift_test` supports Bazel's `--test_filter` flag on all platforms (i.e., Apple
and Linux), which can be used to run only a subset of tests. The expected filter
format is the same as Xcode's `xctest` tool:

*   `ModuleName`: Run only the test classes/methods in module `ModuleName`.
*   `ModuleName.ClassName`: Run only the test methods in class
    `ModuleName.ClassName`.
*   `ModuleName.ClassName/testMethodName`: Run only the method `testMethodName`
    in class `ModuleName.ClassName`.

Multiple such filters can be separated by commas. For example:

```shell
bazel test --test_filter=AModule,BModule.SomeTests,BModule.OtherTests/testX //my/package/...
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_test-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="swift_test-deps"></a>deps |  A list of targets that are dependencies of the target being built, which will be linked into that target.<br><br>If the Swift toolchain supports implementation-only imports (`private_deps` on `swift_library`), then targets in `deps` are treated as regular (non-implementation-only) imports that are propagated both to their direct and indirect (transitive) dependents.<br><br>Allowed kinds of dependencies are:<br><br>*   `swift_library` (or anything propagating `SwiftInfo`)<br><br>*   `cc_library` (or anything propagating `CcInfo`)<br><br>Additionally, on platforms that support Objective-C interop, `objc_library` targets (or anything propagating the `apple_common.Objc` provider) are allowed as dependencies. On platforms that do not support Objective-C interop (such as Linux), those dependencies will be **ignored.**   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_test-srcs"></a>srcs |  A list of `.swift` source files that will be compiled into the library.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_test-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_test-copts"></a>copts |  Additional compiler options that should be passed to `swiftc`. These strings are subject to `$(location ...)` and ["Make" variable](https://docs.bazel.build/versions/master/be/make-variables.html) expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_test-defines"></a>defines |  A list of defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, **not** `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   | List of strings | optional |  `[]`  |
| <a id="swift_test-discover_tests"></a>discover_tests |  Determines whether or not tests are automatically discovered in the binary. The default value is `True`.<br><br>If tests are discovered, then you should not provide your own `main` entry point in the `swift_test` binary; the test runtime provides the entry point for you. If you set this attribute to `False`, then you are responsible for providing your own `main`. This allows you to write tests that use a framework other than Apple's `XCTest`. The only requirement of such a test is that it terminate with a zero exit code for success or a non-zero exit code for failure.<br><br>Additionally, on Apple platforms, test discovery is handled by the Objective-C runtime and the output of a `swift_test` rule is an `.xctest` bundle that is invoked using the `xctest` tool in Xcode. If this attribute is used to disable test discovery, then the output of the `swift_test` rule will instead be a standard executable binary that is invoked directly.   | Boolean | optional |  `True`  |
| <a id="swift_test-env"></a>env |  Dictionary of environment variables that should be set during the test execution.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="swift_test-linkopts"></a>linkopts |  Additional linker options that should be passed to `clang`. These strings are subject to `$(location ...)` expansion.   | List of strings | optional |  `[]`  |
| <a id="swift_test-malloc"></a>malloc |  Override the default dependency on `malloc`.<br><br>By default, Swift binaries are linked against `@bazel_tools//tools/cpp:malloc"`, which is an empty library and the resulting binary will use libc's `malloc`. This label must refer to a `cc_library` rule.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@bazel_tools//tools/cpp:malloc"`  |
| <a id="swift_test-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   | String | optional |  `""`  |
| <a id="swift_test-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same package_name can access APIs using the 'package' access control modifier in Swift 5.9+.   | String | optional |  `""`  |
| <a id="swift_test-plugins"></a>plugins |  A list of `swift_compiler_plugin` targets that should be loaded by the compiler when compiling this module and any modules that directly depend on it.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="swift_test-stamp"></a>stamp |  Enable or disable link stamping; that is, whether to encode build information into the binary. Possible values are:<br><br>* `stamp = 1`: Stamp the build information into the binary. Stamped binaries are   only rebuilt when their dependencies change. Use this if there are tests that   depend on the build information.<br><br>* `stamp = 0`: Always replace build information by constant values. This gives   good build result caching.<br><br>* `stamp = -1`: Embedding of build information is controlled by the   `--[no]stamp` flag.   | Integer | optional |  `0`  |
| <a id="swift_test-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |


<a id="universal_swift_compiler_plugin"></a>

## universal_swift_compiler_plugin

<pre>
universal_swift_compiler_plugin(<a href="#universal_swift_compiler_plugin-name">name</a>, <a href="#universal_swift_compiler_plugin-plugin">plugin</a>)
</pre>

Wraps an existing `swift_compiler_plugin` target to produce a universal binary.

This is useful to allow sharing of caches between Intel and Apple Silicon Macs
at the cost of building everything twice.

Example:

```bzl
# The actual macro code, using SwiftSyntax, as usual.
swift_compiler_plugin(
    name = "Macros",
    srcs = glob(["Macros/*.swift"]),
    deps = [
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftCompilerPlugin",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)

# Wrap your compiler plugin in this universal shim.
universal_swift_compiler_plugin(
    name = "Macros.universal",
    plugin = ":Macros",
)

# The library that defines the macro hook for use in your project, this
# references the universal_swift_compiler_plugin.
swift_library(
    name = "MacroLibrary",
    srcs = glob(["MacroLibrary/*.swift"]),
    plugins = [":Macros.universal"],
)
```

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="universal_swift_compiler_plugin-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="universal_swift_compiler_plugin-plugin"></a>plugin |  Target to generate a 'fat' binary from.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="mixed_language_library"></a>

## mixed_language_library

<pre>
mixed_language_library(<a href="#mixed_language_library-name">name</a>, <a href="#mixed_language_library-alwayslink">alwayslink</a>, <a href="#mixed_language_library-clang_copts">clang_copts</a>, <a href="#mixed_language_library-clang_defines">clang_defines</a>, <a href="#mixed_language_library-clang_srcs">clang_srcs</a>, <a href="#mixed_language_library-data">data</a>,
                       <a href="#mixed_language_library-enable_modules">enable_modules</a>, <a href="#mixed_language_library-hdrs">hdrs</a>, <a href="#mixed_language_library-includes">includes</a>, <a href="#mixed_language_library-linkopts">linkopts</a>, <a href="#mixed_language_library-module_map">module_map</a>, <a href="#mixed_language_library-module_name">module_name</a>,
                       <a href="#mixed_language_library-non_arc_srcs">non_arc_srcs</a>, <a href="#mixed_language_library-package_name">package_name</a>, <a href="#mixed_language_library-private_deps">private_deps</a>, <a href="#mixed_language_library-sdk_dylibs">sdk_dylibs</a>, <a href="#mixed_language_library-sdk_frameworks">sdk_frameworks</a>,
                       <a href="#mixed_language_library-swift_copts">swift_copts</a>, <a href="#mixed_language_library-swift_defines">swift_defines</a>, <a href="#mixed_language_library-swift_srcs">swift_srcs</a>, <a href="#mixed_language_library-swiftc_inputs">swiftc_inputs</a>, <a href="#mixed_language_library-textual_hdrs">textual_hdrs</a>,
                       <a href="#mixed_language_library-umbrella_header">umbrella_header</a>, <a href="#mixed_language_library-weak_sdk_frameworks">weak_sdk_frameworks</a>, <a href="#mixed_language_library-deps">deps</a>, <a href="#mixed_language_library-kwargs">kwargs</a>)
</pre>

Creates a mixed language library from a Clang and Swift library target     pair.

Note: In the future `swift_library` will support mixed-langauge libraries.
Once that is the case, this macro will be deprecated.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="mixed_language_library-name"></a>name |  The name of the target.   |  none |
| <a id="mixed_language_library-alwayslink"></a>alwayslink |  If true, any binary that depends (directly or indirectly) on this library will link in all the object files for the files listed in `clang_srcs` and `swift_srcs`, even if some contain no symbols referenced by the binary. This is useful if your code isn't explicitly called by code in the binary; for example, if you rely on runtime checks for protocol conformances added in extensions in the library but do not directly reference any other symbols in the object file that adds that conformance.   |  `False` |
| <a id="mixed_language_library-clang_copts"></a>clang_copts |  The compiler flags for the clang library. These will only be used for the clang library. If you want them to affect the swift library as well, you need to pass them with `-Xcc` in `swift_copts`.   |  `[]` |
| <a id="mixed_language_library-clang_defines"></a>clang_defines |  Extra clang `-D` flags to pass to the compiler. They should be in the form `KEY=VALUE` or simply `KEY` and are passed not only to the compiler for this target (as `clang_copts` are) but also to all dependers of this target. Subject to "Make variable" substitution and Bourne shell tokenization.   |  `[]` |
| <a id="mixed_language_library-clang_srcs"></a>clang_srcs |  The list of C, C++, Objective-C, or Objective-C++ sources for the clang library.   |  none |
| <a id="mixed_language_library-data"></a>data |  The list of files needed by this target at runtime.<br><br>Files and targets named in the `data` attribute will appear in the `*.runfiles` area of this target, if it has one. This may include data files needed by a binary or library, or other programs needed by it.   |  `[]` |
| <a id="mixed_language_library-enable_modules"></a>enable_modules |  Enables clang module support (via `-fmodules`). Setting this to `True`  will allow you to `@import` system headers and other targets: `@import UIKit;` `@import path_to_package_target;`.   |  `False` |
| <a id="mixed_language_library-hdrs"></a>hdrs |  The list of C, C++, Objective-C, or Objective-C++ header files published by this library to be included by sources in dependent rules. This can't include `umbrella_header`.   |  `[]` |
| <a id="mixed_language_library-includes"></a>includes |  List of `#include`/`#import` search paths to add to this target and all depending targets.   |  `[]` |
| <a id="mixed_language_library-linkopts"></a>linkopts |  Extra flags to pass to the linker.   |  `[]` |
| <a id="mixed_language_library-module_map"></a>module_map |  A `File` representing an existing module map that should be used to represent the module, or `None` (the default) if the module map should be generated based on `hdrs`. If this argument is provided, then `module_name` must also be provided.<br><br>Warning: If a module map (whether provided here or not) is able to be found via an include path, it will result in duplicate module definition errors for downstream targets unless sandboxing or remote execution is used.   |  `None` |
| <a id="mixed_language_library-module_name"></a>module_name |  The name of the Swift module being built.<br><br>If left unspecified, the module name will be computed based on the target's build label, by stripping the leading `//` and replacing `/`, `:`, and other non-identifier characters with underscores.   |  `None` |
| <a id="mixed_language_library-non_arc_srcs"></a>non_arc_srcs |  The list of Objective-C files that are processed to create the library target that DO NOT use ARC. The files in this attribute are treated very similar to those in the `clang_srcs` attribute, but are compiled without ARC enabled.   |  `[]` |
| <a id="mixed_language_library-package_name"></a>package_name |  The semantic package of the Swift target being built. Targets with the same `package_name` can access APIs using the 'package' access control modifier in Swift 5.9+.   |  `None` |
| <a id="mixed_language_library-private_deps"></a>private_deps |  A list of targets that are implementation-only dependencies of the target being built. Libraries/linker flags from these dependencies will be propagated to dependent for linking, but artifacts/flags required for compilation (such as .swiftmodule files, C headers, and search paths) will not be propagated.   |  `[]` |
| <a id="mixed_language_library-sdk_dylibs"></a>sdk_dylibs |  A list of of SDK `.dylib` libraries to link with. For instance, "libz" or "libarchive". "libc++" is included automatically if the binary has any C++ or Objective-C++ sources in its dependency tree. When linking a binary, all libraries named in that binary's transitive dependency graph are used.   |  `[]` |
| <a id="mixed_language_library-sdk_frameworks"></a>sdk_frameworks |  A list of SDK frameworks to link with (e.g. "AddressBook", "QuartzCore").<br><br>When linking a top level Apple binary, all SDK frameworks listed in that binary's transitive dependency graph are linked.   |  `[]` |
| <a id="mixed_language_library-swift_copts"></a>swift_copts |  The compiler flags for the swift library.   |  `[]` |
| <a id="mixed_language_library-swift_defines"></a>swift_defines |  A list of Swift defines to add to the compilation command line.<br><br>Note that unlike C-family languages, Swift defines do not have values; they are simply identifiers that are either defined or undefined. So strings in this list should be simple identifiers, not `name=value` pairs.<br><br>Each string is prepended with `-D` and added to the command line. Unlike `swift_copts`, these flags are added for the target and every target that depends on it, so use this attribute with caution. It is preferred that you add defines directly to `swift_copts`, only using this feature in the rare case that a library needs to propagate a symbol up to those that depend on it.   |  `[]` |
| <a id="mixed_language_library-swift_srcs"></a>swift_srcs |  The sources for the swift library.   |  none |
| <a id="mixed_language_library-swiftc_inputs"></a>swiftc_inputs |  Additional files that are referenced using `$(location ...)` in attributes that support location expansion.   |  `[]` |
| <a id="mixed_language_library-textual_hdrs"></a>textual_hdrs |  The list of C, C++, Objective-C, or Objective-C++ files that are included as headers by source files in this rule or by users of this library. Unlike `hdrs`, these will not be compiled separately from the sources.   |  `[]` |
| <a id="mixed_language_library-umbrella_header"></a>umbrella_header |  A `File` representing an existing umbrella header that should be used in the generated module map or is used in the custom module map, or `None` (the default) if the umbrella header should be generated based on `hdrs`. A symlink to this header is added to an include path such that `#import <ModuleName/ModuleName.h>` works for this and downstream targets.   |  `None` |
| <a id="mixed_language_library-weak_sdk_frameworks"></a>weak_sdk_frameworks |  A list of SDK frameworks to weakly link with. For instance, "MediaAccessibility". In difference to regularly linked SDK frameworks, symbols from weakly linked frameworks do not cause an error if they are not present at runtime.   |  `[]` |
| <a id="mixed_language_library-deps"></a>deps |  A list of targets that are dependencies of the target being built.   |  `[]` |
| <a id="mixed_language_library-kwargs"></a>kwargs |  Additional arguments to pass to the underlying clang and swift library targets.   |  none |


