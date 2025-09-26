<!-- Generated with Stardoc, Do Not Edit! -->
# Build API

The `swift_common` module provides API access to the behavior implemented
by the Swift build rules, so that other custom rules can invoke Swift
compilation and/or linking as part of their implementation.
<a id="swift_common.create_swift_info"></a>

## swift_common.create_swift_info

<pre>
swift_common.create_swift_info(<a href="#swift_common.create_swift_info-direct_modules">direct_modules</a>, <a href="#swift_common.create_swift_info-transitive_modules">transitive_modules</a>)
</pre>

Contains information about the compiled artifacts of a Swift module.

This provider has a custom initializer that will merge the transitive modules of
a list of `SwiftInfo` providers, rather than a separate "merge" function. The
correct signature when _creating_ a new `SwiftInfo` provider is the following:

```build
SwiftInfo(
    direct_swift_infos,
    modules,
    swift_infos,
)
```

where the arguments are:

*   `direct_swift_infos`: A list of `SwiftInfo` providers from dependencies
    whose direct modules should be treated as direct modules in the resulting
    provider, in addition to their transitive modules being merged.
*   `modules`: A list of values (as returned by `create_swift_module_context`)
    that represent Clang and/or Swift module artifacts that are direct outputs
    of the target being built.
*   `swift_infos`: A list of `SwiftInfo` providers from dependencies whose
    transitive modules should be merged into the resulting provider.

When reading an existing `SwiftInfo` provider, it has the two fields described
below.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="swift_common.create_swift_info-direct_modules"></a>direct_modules |  `List` of values returned from `create_swift_module_context`. The modules (both Swift and C/Objective-C) emitted by the library that propagated this provider.    |
| <a id="swift_common.create_swift_info-transitive_modules"></a>transitive_modules |  `Depset` of values returned from `create_swift_module_context`. The transitive modules (both Swift and C/Objective-C) emitted by the library that propagated this provider and all of its dependencies.    |


<a id="swift_common.cc_feature_configuration"></a>

## swift_common.cc_feature_configuration

<pre>
swift_common.cc_feature_configuration(<a href="#swift_common.cc_feature_configuration-feature_configuration">feature_configuration</a>)
</pre>

Returns the C++ feature configuration in a Swift feature configuration.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.cc_feature_configuration-feature_configuration"></a>feature_configuration |  The Swift feature configuration, as returned from `configure_features`.   |  none |

**RETURNS**

A C++ `FeatureConfiguration` value (see
  [`cc_common.configure_features`](https://docs.bazel.build/versions/master/skylark/lib/cc_common.html#configure_features)
  for more information).


<a id="swift_common.compilation_attrs"></a>

## swift_common.compilation_attrs

<pre>
swift_common.compilation_attrs(<a href="#swift_common.compilation_attrs-additional_deps_aspects">additional_deps_aspects</a>, <a href="#swift_common.compilation_attrs-additional_deps_providers">additional_deps_providers</a>,
                               <a href="#swift_common.compilation_attrs-include_dev_srch_paths_attrib">include_dev_srch_paths_attrib</a>, <a href="#swift_common.compilation_attrs-requires_srcs">requires_srcs</a>)
</pre>

Returns an attribute dictionary for rules that compile Swift code.

The returned dictionary contains the subset of attributes that are shared by
the `swift_binary`, `swift_library`, and `swift_test` rules that deal with
inputs and options for compilation. Users who are authoring custom rules
that compile Swift code but not as a library can add this dictionary to
their own rule's attributes to give it a familiar API.

Do note, however, that it is the responsibility of the rule implementation
to retrieve the values of those attributes and pass them correctly to the
other `swift_common` APIs.

There is a hierarchy to the attribute sets offered by the `swift_common`
API:

1.  If you only need access to the toolchain for its tools and libraries but
    are not doing any compilation, use `toolchain_attrs`.
2.  If you need to invoke compilation actions but are not making the
    resulting object files into a static or shared library, use
    `compilation_attrs`.
3.  If you want to provide a rule interface that is suitable as a drop-in
    replacement for `swift_library`, use `library_rule_attrs`.

Each of the attribute functions in the list above also contains the
attributes from the earlier items in the list.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.compilation_attrs-additional_deps_aspects"></a>additional_deps_aspects |  A list of additional aspects that should be applied to `deps`. Defaults to the empty list. These must be passed by the individual rules to avoid potential circular dependencies between the API and the aspects; the API loaded the aspects directly, then those aspects would not be able to load the API.   |  `[]` |
| <a id="swift_common.compilation_attrs-additional_deps_providers"></a>additional_deps_providers |  A list of lists representing additional providers that should be allowed by the `deps` attribute of the rule.   |  `[]` |
| <a id="swift_common.compilation_attrs-include_dev_srch_paths_attrib"></a>include_dev_srch_paths_attrib |  A `bool` that indicates whether to include the `always_include_developer_search_paths` attribute.   |  `False` |
| <a id="swift_common.compilation_attrs-requires_srcs"></a>requires_srcs |  Indicates whether the `srcs` attribute should be marked as mandatory and non-empty. Defaults to `True`.   |  `True` |

**RETURNS**

A new attribute dictionary that can be added to the attributes of a
  custom build rule to provide a similar interface to `swift_binary`,
  `swift_library`, and `swift_test`.


<a id="swift_common.compile"></a>

## swift_common.compile

<pre>
swift_common.compile(<a href="#swift_common.compile-actions">actions</a>, <a href="#swift_common.compile-additional_inputs">additional_inputs</a>, <a href="#swift_common.compile-cc_infos">cc_infos</a>, <a href="#swift_common.compile-copts">copts</a>, <a href="#swift_common.compile-defines">defines</a>, <a href="#swift_common.compile-exec_group">exec_group</a>,
                     <a href="#swift_common.compile-extra_swift_infos">extra_swift_infos</a>, <a href="#swift_common.compile-feature_configuration">feature_configuration</a>, <a href="#swift_common.compile-generated_header_name">generated_header_name</a>, <a href="#swift_common.compile-is_test">is_test</a>,
                     <a href="#swift_common.compile-include_dev_srch_paths">include_dev_srch_paths</a>, <a href="#swift_common.compile-module_name">module_name</a>, <a href="#swift_common.compile-objc_infos">objc_infos</a>, <a href="#swift_common.compile-package_name">package_name</a>, <a href="#swift_common.compile-plugins">plugins</a>,
                     <a href="#swift_common.compile-private_swift_infos">private_swift_infos</a>, <a href="#swift_common.compile-srcs">srcs</a>, <a href="#swift_common.compile-swift_infos">swift_infos</a>, <a href="#swift_common.compile-swift_toolchain">swift_toolchain</a>, <a href="#swift_common.compile-target_name">target_name</a>,
                     <a href="#swift_common.compile-workspace_name">workspace_name</a>)
</pre>

Compiles a Swift module.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.compile-actions"></a>actions |  The context's `actions` object.   |  none |
| <a id="swift_common.compile-additional_inputs"></a>additional_inputs |  A list of `File`s representing additional input files that need to be passed to the Swift compile action because they are referenced by compiler flags.   |  `[]` |
| <a id="swift_common.compile-cc_infos"></a>cc_infos |  A list of `CcInfo` providers that represent C/Objective-C requirements of the target being compiled, such as Swift-compatible preprocessor defines, header search paths, and so forth. These are typically retrieved from a target's dependencies.   |  none |
| <a id="swift_common.compile-copts"></a>copts |  A list of compiler flags that apply to the target being built. These flags, along with those from Bazel's Swift configuration fragment (i.e., `--swiftcopt` command line flags) are scanned to determine whether whole module optimization is being requested, which affects the nature of the output files.   |  `[]` |
| <a id="swift_common.compile-defines"></a>defines |  Symbols that should be defined by passing `-D` to the compiler.   |  `[]` |
| <a id="swift_common.compile-exec_group"></a>exec_group |  Runs the Swift compilation action under the given execution group's context. If `None`, the default execution group is used.   |  `None` |
| <a id="swift_common.compile-extra_swift_infos"></a>extra_swift_infos |  Extra `SwiftInfo` providers that aren't contained by the `deps` of the target being compiled but are required for compilation.   |  `[]` |
| <a id="swift_common.compile-feature_configuration"></a>feature_configuration |  A feature configuration obtained from `configure_features`.   |  none |
| <a id="swift_common.compile-generated_header_name"></a>generated_header_name |  The name of the Objective-C generated header that should be generated for this module. If omitted, no header will be generated.   |  `None` |
| <a id="swift_common.compile-is_test"></a>is_test |  Deprecated. This argument will be removed in the next major release. Use the `include_dev_srch_paths` attribute instead. Represents if the `testonly` value of the context.   |  `None` |
| <a id="swift_common.compile-include_dev_srch_paths"></a>include_dev_srch_paths |  A `bool` that indicates whether the developer framework search paths will be added to the compilation command.   |  `None` |
| <a id="swift_common.compile-module_name"></a>module_name |  The name of the Swift module being compiled. This must be present and valid; use `derive_swift_module_name` to generate a default from the target's label if needed.   |  none |
| <a id="swift_common.compile-objc_infos"></a>objc_infos |  A list of `apple_common.ObjC` providers that represent C/Objective-C requirements of the target being compiled, such as Swift-compatible preprocessor defines, header search paths, and so forth. These are typically retrieved from a target's dependencies.   |  none |
| <a id="swift_common.compile-package_name"></a>package_name |  The semantic package of the name of the Swift module being compiled.   |  none |
| <a id="swift_common.compile-plugins"></a>plugins |  A list of `SwiftCompilerPluginInfo` providers that represent plugins that should be loaded by the compiler.   |  `[]` |
| <a id="swift_common.compile-private_swift_infos"></a>private_swift_infos |  A list of `SwiftInfo` providers from private (implementation-only) dependencies of the target being compiled. The modules defined by these providers are used as dependencies of the Swift module being compiled but not of the Clang module for the generated header.   |  `[]` |
| <a id="swift_common.compile-srcs"></a>srcs |  The Swift source files to compile.   |  none |
| <a id="swift_common.compile-swift_infos"></a>swift_infos |  A list of `SwiftInfo` providers from non-private dependencies of the target being compiled. The modules defined by these providers are used as dependencies of both the Swift module being compiled and the Clang module for the generated header.   |  none |
| <a id="swift_common.compile-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain.   |  none |
| <a id="swift_common.compile-target_name"></a>target_name |  The name of the target for which the code is being compiled, which is used to determine unique file paths for the outputs.   |  none |
| <a id="swift_common.compile-workspace_name"></a>workspace_name |  The name of the workspace for which the code is being compiled, which is used to determine unique file paths for some outputs.   |  none |

**RETURNS**

A `struct` with the following fields:

  *   `swift_info`: A `SwiftInfo` provider whose list of direct modules
      contains the single Swift module context produced by this function
      (identical to the `module_context` field below) and whose transitive
      modules represent the transitive non-private dependencies. Rule
      implementations that call this function can typically return this
      provider directly, except in rare cases like making multiple calls
      to `swift_common.compile` that need to be merged.

  *   `module_context`: A Swift module context (as returned by
      `create_swift_module_context`) that contains the Swift (and
      potentially C/Objective-C) compilation prerequisites of the compiled
      module. This should typically be propagated by a `SwiftInfo`
      provider of the calling rule, and the `CcCompilationContext` inside
      the Clang module substructure should be propagated by the `CcInfo`
      provider of the calling rule.

  *   `compilation_outputs`: A `CcCompilationOutputs` object (as returned
      by `cc_common.create_compilation_outputs`) that contains the
      compiled object files.

  *   `supplemental_outputs`: A `struct` representing supplemental,
      optional outputs. Its fields are:

      *   `ast_files`: A list of `File`s output from the `DUMP_AST`
          action.

      *   `const_values_files`: A list of `File`s that contains JSON
          representations of constant values extracted from the source
          files, if requested via a direct dependency.

      *   `indexstore_directory`: A directory-type `File` that represents
          the indexstore output files created when the feature
          `swift.index_while_building` is enabled.

      *   `macro_expansion_directory`: A directory-type `File` that
          represents the location where macro expansion files were written
          (only in debug/fastbuild and only when the toolchain supports
          macros).


<a id="swift_common.compile_module_interface"></a>

## swift_common.compile_module_interface

<pre>
swift_common.compile_module_interface(<a href="#swift_common.compile_module_interface-actions">actions</a>, <a href="#swift_common.compile_module_interface-clang_module">clang_module</a>, <a href="#swift_common.compile_module_interface-compilation_contexts">compilation_contexts</a>, <a href="#swift_common.compile_module_interface-copts">copts</a>,
                                      <a href="#swift_common.compile_module_interface-exec_group">exec_group</a>, <a href="#swift_common.compile_module_interface-feature_configuration">feature_configuration</a>, <a href="#swift_common.compile_module_interface-is_framework">is_framework</a>, <a href="#swift_common.compile_module_interface-module_name">module_name</a>,
                                      <a href="#swift_common.compile_module_interface-swiftinterface_file">swiftinterface_file</a>, <a href="#swift_common.compile_module_interface-swift_infos">swift_infos</a>, <a href="#swift_common.compile_module_interface-swift_toolchain">swift_toolchain</a>, <a href="#swift_common.compile_module_interface-target_name">target_name</a>)
</pre>

Compiles a Swift module interface.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.compile_module_interface-actions"></a>actions |  The context's `actions` object.   |  none |
| <a id="swift_common.compile_module_interface-clang_module"></a>clang_module |  An optional underlying Clang module (as returned by `create_clang_module_inputs`), if present for this Swift module.   |  `None` |
| <a id="swift_common.compile_module_interface-compilation_contexts"></a>compilation_contexts |  A list of `CcCompilationContext`s that represent C/Objective-C requirements of the target being compiled, such as Swift-compatible preprocessor defines, header search paths, and so forth. These are typically retrieved from the `CcInfo` providers of a target's dependencies.   |  none |
| <a id="swift_common.compile_module_interface-copts"></a>copts |  A list of compiler flags that apply to the target being built.   |  `[]` |
| <a id="swift_common.compile_module_interface-exec_group"></a>exec_group |  Runs the Swift compilation action under the given execution group's context. If `None`, the default execution group is used.   |  `None` |
| <a id="swift_common.compile_module_interface-feature_configuration"></a>feature_configuration |  A feature configuration obtained from `configure_features`.   |  none |
| <a id="swift_common.compile_module_interface-is_framework"></a>is_framework |  True if this module is a Framework module, false othwerise.   |  `False` |
| <a id="swift_common.compile_module_interface-module_name"></a>module_name |  The name of the Swift module being compiled. This must be present and valid; use `derive_swift_module_name` to generate a default from the target's label if needed.   |  none |
| <a id="swift_common.compile_module_interface-swiftinterface_file"></a>swiftinterface_file |  The Swift module interface file to compile.   |  none |
| <a id="swift_common.compile_module_interface-swift_infos"></a>swift_infos |  A list of `SwiftInfo` providers from dependencies of the target being compiled.   |  none |
| <a id="swift_common.compile_module_interface-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain.   |  none |
| <a id="swift_common.compile_module_interface-target_name"></a>target_name |  The name of the target for which the code is being compiled, which is used to determine unique file paths for the outputs.   |  none |

**RETURNS**

A Swift module context (as returned by `create_swift_module_context`)
  that contains the Swift (and potentially C/Objective-C) compilation
  prerequisites of the compiled module. This should typically be
  propagated by a `SwiftInfo` provider of the calling rule, and the
  `CcCompilationContext` inside the Clang module substructure should be
  propagated by the `CcInfo` provider of the calling rule.


<a id="swift_common.configure_features"></a>

## swift_common.configure_features

<pre>
swift_common.configure_features(<a href="#swift_common.configure_features-ctx">ctx</a>, <a href="#swift_common.configure_features-swift_toolchain">swift_toolchain</a>, <a href="#swift_common.configure_features-requested_features">requested_features</a>, <a href="#swift_common.configure_features-unsupported_features">unsupported_features</a>)
</pre>

Creates a feature configuration to be passed to Swift build APIs.

This function calls through to `cc_common.configure_features` to configure
underlying C++ features as well, and nests the C++ feature configuration
inside the Swift one. Users who need to call C++ APIs that require a feature
configuration can extract it by calling
`get_cc_feature_configuration(feature_configuration)`.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.configure_features-ctx"></a>ctx |  The rule context.   |  none |
| <a id="swift_common.configure_features-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain being used to build. This is used to determine features that are enabled by default or unsupported by the toolchain, and the C++ toolchain associated with the Swift toolchain is used to create the underlying C++ feature configuration.   |  none |
| <a id="swift_common.configure_features-requested_features"></a>requested_features |  The list of features to be enabled. This is typically obtained using the `ctx.features` field in a rule implementation function.   |  `[]` |
| <a id="swift_common.configure_features-unsupported_features"></a>unsupported_features |  The list of features that are unsupported by the current rule. This is typically obtained using the `ctx.disabled_features` field in a rule implementation function.   |  `[]` |

**RETURNS**

An opaque value representing the feature configuration that can be
  passed to other `swift_common` functions. Note that the structure of
  this value should otherwise not be relied on or inspected directly.


<a id="swift_common.create_clang_module"></a>

## swift_common.create_clang_module

<pre>
swift_common.create_clang_module(<a href="#swift_common.create_clang_module-compilation_context">compilation_context</a>, <a href="#swift_common.create_clang_module-module_map">module_map</a>, <a href="#swift_common.create_clang_module-precompiled_module">precompiled_module</a>,
                                 <a href="#swift_common.create_clang_module-strict_includes">strict_includes</a>)
</pre>

Creates a value representing a Clang module used as a Swift dependency.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_clang_module-compilation_context"></a>compilation_context |  A `CcCompilationContext` that contains the header files and other context (such as include paths, preprocessor defines, and so forth) needed to compile this module as an explicit module.   |  none |
| <a id="swift_common.create_clang_module-module_map"></a>module_map |  The text module map file that defines this module. This argument may be specified as a `File` or as a `string`; in the latter case, it is assumed to be the path to a file that cannot be provided as an action input because it is outside the workspace (for example, the module map for a module from an Xcode SDK).   |  none |
| <a id="swift_common.create_clang_module-precompiled_module"></a>precompiled_module |  A `File` representing the precompiled module (`.pcm` file) if one was emitted for the module. This may be `None` if no explicit module was built for the module; in that case, targets that depend on the module will fall back to the text module map and headers.   |  `None` |
| <a id="swift_common.create_clang_module-strict_includes"></a>strict_includes |  A `depset` of strings representing additional Clang include paths that should be passed to the compiler when this module is a _direct_ dependency of the module being compiled. May be `None`. **This field only exists to support a specific legacy use case and should otherwise not be used, as it is fundamentally incompatible with Swift's import model.**   |  `None` |

**RETURNS**

A `struct` containing the values provided as arguments.


<a id="swift_common.create_compilation_context"></a>

## swift_common.create_compilation_context

<pre>
swift_common.create_compilation_context(<a href="#swift_common.create_compilation_context-defines">defines</a>, <a href="#swift_common.create_compilation_context-srcs">srcs</a>, <a href="#swift_common.create_compilation_context-transitive_modules">transitive_modules</a>)
</pre>

Cretes a compilation context for a Swift target.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_compilation_context-defines"></a>defines |  A list of defines   |  none |
| <a id="swift_common.create_compilation_context-srcs"></a>srcs |  A list of Swift source files used to compile the target.   |  none |
| <a id="swift_common.create_compilation_context-transitive_modules"></a>transitive_modules |  A list of modules (as returned by `create_swift_module_context`) from the transitive dependencies of the target.   |  none |

**RETURNS**

A `struct` containing four fields:

  *   `defines`: A sequence of defines used when compiling the target.
      Includes the defines for the target and its transitive dependencies.
  *   `direct_sources`: A sequence of Swift source files used to compile
      the target.
  *   `module_maps`: A sequence of module maps used to compile the clang
      module for this target.
  *   `swiftmodules`: A sequence of swiftmodules depended on by the
      target.


<a id="swift_common.create_linking_context_from_compilation_outputs"></a>

## swift_common.create_linking_context_from_compilation_outputs

<pre>
swift_common.create_linking_context_from_compilation_outputs(<a href="#swift_common.create_linking_context_from_compilation_outputs-actions">actions</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-additional_inputs">additional_inputs</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-alwayslink">alwayslink</a>,
                                                             <a href="#swift_common.create_linking_context_from_compilation_outputs-compilation_outputs">compilation_outputs</a>,
                                                             <a href="#swift_common.create_linking_context_from_compilation_outputs-feature_configuration">feature_configuration</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-is_test">is_test</a>,
                                                             <a href="#swift_common.create_linking_context_from_compilation_outputs-include_dev_srch_paths">include_dev_srch_paths</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-label">label</a>,
                                                             <a href="#swift_common.create_linking_context_from_compilation_outputs-linking_contexts">linking_contexts</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-module_context">module_context</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-name">name</a>,
                                                             <a href="#swift_common.create_linking_context_from_compilation_outputs-swift_toolchain">swift_toolchain</a>, <a href="#swift_common.create_linking_context_from_compilation_outputs-user_link_flags">user_link_flags</a>)
</pre>

Creates a linking context from the outputs of a Swift compilation.

On some platforms, this function will spawn additional post-compile actions
for the module in order to add their outputs to the linking context. For
example, if the toolchain that requires a "module-wrap" invocation to embed
the `.swiftmodule` into an object file for debugging purposes, or if it
extracts auto-linking information from the object files to generate a linker
command line parameters file, those actions will be created here.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_linking_context_from_compilation_outputs-actions"></a>actions |  The context's `actions` object.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-additional_inputs"></a>additional_inputs |  A `list` of `File`s containing any additional files that are referenced by `user_link_flags` and therefore need to be propagated up to the linker.   |  `[]` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-alwayslink"></a>alwayslink |  If True, any binary that depends on the providers returned by this function will link in all of the library's object files, even if some contain no symbols referenced by the binary.   |  `False` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-compilation_outputs"></a>compilation_outputs |  A `CcCompilationOutputs` value containing the object files to link. Typically, this is the second tuple element in the value returned by `compile`.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-feature_configuration"></a>feature_configuration |  A feature configuration obtained from `configure_features`.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-is_test"></a>is_test |  Deprecated. This argument will be removed in the next major release. Use the `include_dev_srch_paths` attribute instead. Represents if the `testonly` value of the context.   |  `None` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-include_dev_srch_paths"></a>include_dev_srch_paths |  A `bool` that indicates whether the developer framework search paths will be added to the compilation command.   |  `None` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-label"></a>label |  The `Label` of the target being built. This is used as the owner of the linker inputs created for post-compile actions (if any), and the label's name component also determines the name of the artifact unless it is overridden by the `name` argument.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-linking_contexts"></a>linking_contexts |  A `list` of `CcLinkingContext`s containing libraries from dependencies.   |  `[]` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-module_context"></a>module_context |  The module context returned by `compile` containing information about the Swift module that was compiled. Typically, this is the first tuple element in the value returned by `compile`.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-name"></a>name |  A string that is used to derive the name of the library or libraries linked by this function. If this is not provided or is a falsy value, the name component of the `label` argument is used.   |  `None` |
| <a id="swift_common.create_linking_context_from_compilation_outputs-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain.   |  none |
| <a id="swift_common.create_linking_context_from_compilation_outputs-user_link_flags"></a>user_link_flags |  A `list` of strings containing additional flags that will be passed to the linker for any binary that links with the returned linking context.   |  `[]` |

**RETURNS**

A tuple of `(CcLinkingContext, CcLinkingOutputs)` containing the linking
  context to be propagated by the caller's `CcInfo` provider and the
  artifact representing the library that was linked, respectively.


<a id="swift_common.create_module"></a>

## swift_common.create_module

<pre>
swift_common.create_module(<a href="#swift_common.create_module-name">name</a>, <a href="#swift_common.create_module-clang">clang</a>, <a href="#swift_common.create_module-const_gather_protocols">const_gather_protocols</a>, <a href="#swift_common.create_module-compilation_context">compilation_context</a>, <a href="#swift_common.create_module-is_framework">is_framework</a>,
                           <a href="#swift_common.create_module-is_system">is_system</a>, <a href="#swift_common.create_module-swift">swift</a>)
</pre>

Creates a value containing Clang/Swift module artifacts of a dependency.

It is possible for both `clang` and `swift` to be present; this is the case
for Swift modules that generate an Objective-C header, where the Swift
module artifacts are propagated in the `swift` context and the generated
header and module map are propagated in the `clang` context.

Though rare, it is also permitted for both the `clang` and `swift` arguments
to be `None`. One example of how this can be used is to model system
dependencies (like Apple SDK frameworks) that are implicitly available as
part of a non-hermetic SDK (Xcode) but do not propagate any artifacts of
their own. This would only apply in a build using implicit modules, however;
when using explicit modules, one would propagate the module artifacts
explicitly. But allowing for the empty case keeps the build graph consistent
if switching between the two modes is necessary, since it will not change
the set of transitive module names that are propagated by dependencies
(which other build rules may want to depend on for their own analysis).


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_module-name"></a>name |  The name of the module.   |  none |
| <a id="swift_common.create_module-clang"></a>clang |  A value returned by `create_clang_module_inputs` that contains artifacts related to Clang modules, such as a module map or precompiled module. This may be `None` if the module is a pure Swift module with no generated Objective-C interface.   |  `None` |
| <a id="swift_common.create_module-const_gather_protocols"></a>const_gather_protocols |  A list of protocol names from which constant values should be extracted from source code that takes this module as a *direct* dependency.   |  `[]` |
| <a id="swift_common.create_module-compilation_context"></a>compilation_context |  A value returned from `swift_common.create_compilation_context` that contains the context needed to compile the module being built. This may be `None` if the module wasn't compiled from sources.   |  `None` |
| <a id="swift_common.create_module-is_framework"></a>is_framework |  Indictates whether the module is a framework module. The default value is `False`.   |  `False` |
| <a id="swift_common.create_module-is_system"></a>is_system |  Indicates whether the module is a system module. The default value is `False`. System modules differ slightly from non-system modules in the way that they are passed to the compiler. For example, non-system modules have their Clang module maps passed to the compiler in both implicit and explicit module builds. System modules, on the other hand, do not have their module maps passed to the compiler in implicit module builds because there is currently no way to indicate that modules declared in a file passed via `-fmodule-map-file` should be treated as system modules even if they aren't declared with the `[system]` attribute, and some system modules may not build cleanly with respect to warnings otherwise. Therefore, it is assumed that any module with `is_system == True` must be able to be found using import search paths in order for implicit module builds to succeed.   |  `False` |
| <a id="swift_common.create_module-swift"></a>swift |  A value returned by `create_swift_module_inputs` that contains artifacts related to Swift modules, such as the `.swiftmodule`, `.swiftdoc`, and/or `.swiftinterface` files emitted by the compiler. This may be `None` if the module is a pure C/Objective-C module.   |  `None` |

**RETURNS**

A `struct` containing the given values provided as arguments.


<a id="swift_common.create_swift_interop_info"></a>

## swift_common.create_swift_interop_info

<pre>
swift_common.create_swift_interop_info(<a href="#swift_common.create_swift_interop_info-exclude_headers">exclude_headers</a>, <a href="#swift_common.create_swift_interop_info-module_map">module_map</a>, <a href="#swift_common.create_swift_interop_info-module_name">module_name</a>, <a href="#swift_common.create_swift_interop_info-requested_features">requested_features</a>,
                                       <a href="#swift_common.create_swift_interop_info-suppressed">suppressed</a>, <a href="#swift_common.create_swift_interop_info-swift_infos">swift_infos</a>, <a href="#swift_common.create_swift_interop_info-unsupported_features">unsupported_features</a>)
</pre>

Returns a provider that lets a target expose C/Objective-C APIs to Swift.

The provider returned by this function allows custom build rules written in
Starlark to be uninvolved with much of the low-level machinery involved in
making a Swift-compatible module. Such a target should propagate a `CcInfo`
provider whose compilation context contains the headers that it wants to
make into a module, and then also propagate the provider returned from this
function.

The simplest usage is for a custom rule to do the following:

*   Add `swift_clang_module_aspect` to any attribute that provides
    dependencies of the code that needs to interop with Swift (typically
    `deps`, but could be other attributes as well, such as attributes
    providing additional support libraries).
*   Have the rule implementation call `create_swift_interop_info`, passing
    it only the list of `SwiftInfo` providers from its dependencies. This
    tells `swift_clang_module_aspect` when it runs on *this* rule's target
    to derive the module name from the target label and create a module map
    using the headers from the compilation context of the `CcInfo` you
    propagate.

If the custom rule has reason to provide its own module name or module map,
then it can do so using the `module_name` and `module_map` arguments.

When a rule returns this provider, it must provide the full set of
`SwiftInfo` providers from dependencies that will be merged with the one
that `swift_clang_module_aspect` creates for the target itself. The aspect
will **not** collect dependency providers automatically. This allows the
rule to not only add extra dependencies (such as support libraries from
implicit attributes) but also to exclude dependencies if necessary.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_swift_interop_info-exclude_headers"></a>exclude_headers |  A `list` of `File`s representing headers that should be excluded from the module if the module map is generated.   |  `[]` |
| <a id="swift_common.create_swift_interop_info-module_map"></a>module_map |  A `File` representing an existing module map that should be used to represent the module, or `None` (the default) if the module map should be generated based on the headers in the target's compilation context. If this argument is provided, then `module_name` must also be provided.   |  `None` |
| <a id="swift_common.create_swift_interop_info-module_name"></a>module_name |  A string denoting the name of the module, or `None` (the default) if the name should be derived automatically from the target label.   |  `None` |
| <a id="swift_common.create_swift_interop_info-requested_features"></a>requested_features |  A list of features (empty by default) that should be requested for the target, which are added to those supplied in the `features` attribute of the target. These features will be enabled unless they are otherwise marked as unsupported (either on the target or by the toolchain). This allows the rule implementation to have additional control over features that should be supported by default for all instances of that rule as if it were creating the feature configuration itself; for example, a rule can request that `swift.emit_c_module` always be enabled for its targets even if it is not explicitly enabled in the toolchain or on the target directly.   |  `[]` |
| <a id="swift_common.create_swift_interop_info-suppressed"></a>suppressed |  A `bool` indicating whether the module that the aspect would create for the target should instead be suppressed.   |  `False` |
| <a id="swift_common.create_swift_interop_info-swift_infos"></a>swift_infos |  A list of `SwiftInfo` providers from dependencies, which will be merged with the new `SwiftInfo` created by the aspect.   |  `[]` |
| <a id="swift_common.create_swift_interop_info-unsupported_features"></a>unsupported_features |  A list of features (empty by default) that should be considered unsupported for the target, which are added to those supplied as negations in the `features` attribute. This allows the rule implementation to have additional control over features that should be disabled by default for all instances of that rule as if it were creating the feature configuration itself; for example, a rule that processes frameworks with headers that do not follow strict layering can request that `swift.strict_module` always be disabled for its targets even if it is enabled by default in the toolchain.   |  `[]` |

**RETURNS**

A provider whose type/layout is an implementation detail and should not
  be relied upon.


<a id="swift_common.create_swift_module"></a>

## swift_common.create_swift_module

<pre>
swift_common.create_swift_module(<a href="#swift_common.create_swift_module-ast_files">ast_files</a>, <a href="#swift_common.create_swift_module-const_protocols_to_gather">const_protocols_to_gather</a>, <a href="#swift_common.create_swift_module-defines">defines</a>, <a href="#swift_common.create_swift_module-generated_header">generated_header</a>,
                                 <a href="#swift_common.create_swift_module-indexstore">indexstore</a>, <a href="#swift_common.create_swift_module-original_module_name">original_module_name</a>, <a href="#swift_common.create_swift_module-plugins">plugins</a>, <a href="#swift_common.create_swift_module-private_swiftinterface">private_swiftinterface</a>,
                                 <a href="#swift_common.create_swift_module-swiftdoc">swiftdoc</a>, <a href="#swift_common.create_swift_module-swiftinterface">swiftinterface</a>, <a href="#swift_common.create_swift_module-swiftmodule">swiftmodule</a>, <a href="#swift_common.create_swift_module-swiftsourceinfo">swiftsourceinfo</a>)
</pre>

Creates a value representing a Swift module use as a Swift dependency.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.create_swift_module-ast_files"></a>ast_files |  A list of `File`s output from the `DUMP_AST` action.   |  `[]` |
| <a id="swift_common.create_swift_module-const_protocols_to_gather"></a>const_protocols_to_gather |  A list of protocol names from which constant values should be extracted from source code that takes this module as a *direct* dependency.   |  `[]` |
| <a id="swift_common.create_swift_module-defines"></a>defines |  A list of defines that will be provided as `copts` to targets that depend on this module. If omitted, the empty list will be used.   |  `[]` |
| <a id="swift_common.create_swift_module-generated_header"></a>generated_header |  A `File` representing the Swift generated header.   |  `None` |
| <a id="swift_common.create_swift_module-indexstore"></a>indexstore |  A `File` representing the directory that contains the index store data generated by the compiler if the `"swift.index_while_building"` feature is enabled, otherwise this will be `None`.   |  `None` |
| <a id="swift_common.create_swift_module-original_module_name"></a>original_module_name |  The original name of the module if it was changed by a module mapping; otherwise, `None`.   |  `None` |
| <a id="swift_common.create_swift_module-plugins"></a>plugins |  A list of `SwiftCompilerPluginInfo` providers representing compiler plugins that are required by this module and should be loaded by the compiler when this module is directly depended on.   |  `[]` |
| <a id="swift_common.create_swift_module-private_swiftinterface"></a>private_swiftinterface |  The `.private.swiftinterface` file emitted by the compiler for this module. May be `None` if no private module interface file was emitted.   |  `None` |
| <a id="swift_common.create_swift_module-swiftdoc"></a>swiftdoc |  The `.swiftdoc` file emitted by the compiler for this module.   |  none |
| <a id="swift_common.create_swift_module-swiftinterface"></a>swiftinterface |  The `.swiftinterface` file emitted by the compiler for this module. May be `None` if no module interface file was emitted.   |  `None` |
| <a id="swift_common.create_swift_module-swiftmodule"></a>swiftmodule |  The `.swiftmodule` file emitted by the compiler for this module.   |  none |
| <a id="swift_common.create_swift_module-swiftsourceinfo"></a>swiftsourceinfo |  The `.swiftsourceinfo` file emitted by the compiler for this module. May be `None` if no source info file was emitted.   |  `None` |

**RETURNS**

A `struct` containing the values provided as arguments.


<a id="swift_common.derive_module_name"></a>

## swift_common.derive_module_name

<pre>
swift_common.derive_module_name(<a href="#swift_common.derive_module_name-args">args</a>)
</pre>

Returns a derived module name from the given build label.

For targets whose module name is not explicitly specified, the module name
is computed using the following algorithm:

*   The package and name components of the label are considered separately.
    All _interior_ sequences of non-identifier characters (anything other
    than `a-z`, `A-Z`, `0-9`, and `_`) are replaced by a single underscore
    (`_`). Any leading or trailing non-identifier characters are dropped.
*   If the package component is non-empty after the above transformation,
    it is joined with the transformed name component using an underscore.
    Otherwise, the transformed name is used by itself.
*   If this would result in a string that begins with a digit (`0-9`), an
    underscore is prepended to make it identifier-safe.

This mapping is intended to be fairly predictable, but not reversible.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.derive_module_name-args"></a>args |  Either a single argument of type `Label`, or two arguments of type `str` where the first argument is the package name and the second argument is the target name.   |  none |

**RETURNS**

The module name derived from the label.


<a id="swift_common.extract_symbol_graph"></a>

## swift_common.extract_symbol_graph

<pre>
swift_common.extract_symbol_graph(<a href="#swift_common.extract_symbol_graph-actions">actions</a>, <a href="#swift_common.extract_symbol_graph-compilation_contexts">compilation_contexts</a>, <a href="#swift_common.extract_symbol_graph-emit_extension_block_symbols">emit_extension_block_symbols</a>,
                                  <a href="#swift_common.extract_symbol_graph-feature_configuration">feature_configuration</a>, <a href="#swift_common.extract_symbol_graph-include_dev_srch_paths">include_dev_srch_paths</a>, <a href="#swift_common.extract_symbol_graph-minimum_access_level">minimum_access_level</a>,
                                  <a href="#swift_common.extract_symbol_graph-module_name">module_name</a>, <a href="#swift_common.extract_symbol_graph-output_dir">output_dir</a>, <a href="#swift_common.extract_symbol_graph-swift_infos">swift_infos</a>, <a href="#swift_common.extract_symbol_graph-swift_toolchain">swift_toolchain</a>)
</pre>

Extracts the symbol graph from a Swift module.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.extract_symbol_graph-actions"></a>actions |  The object used to register actions.   |  none |
| <a id="swift_common.extract_symbol_graph-compilation_contexts"></a>compilation_contexts |  A list of `CcCompilationContext`s that represent C/Objective-C requirements of the target being compiled, such as Swift-compatible preprocessor defines, header search paths, and so forth. These are typically retrieved from the `CcInfo` providers of a target's dependencies.   |  none |
| <a id="swift_common.extract_symbol_graph-emit_extension_block_symbols"></a>emit_extension_block_symbols |  A `bool` that indicates whether `extension` block information should be included in the symbol graph.   |  `None` |
| <a id="swift_common.extract_symbol_graph-feature_configuration"></a>feature_configuration |  The Swift feature configuration.   |  none |
| <a id="swift_common.extract_symbol_graph-include_dev_srch_paths"></a>include_dev_srch_paths |  A `bool` that indicates whether the developer framework search paths will be added to the compilation command.   |  none |
| <a id="swift_common.extract_symbol_graph-minimum_access_level"></a>minimum_access_level |  The minimum access level of the declarations that should be extracted into the symbol graphs. The default value is `None`, which means the Swift compiler's default behavior should be used (at the time of this writing, the default behavior is "public").   |  `None` |
| <a id="swift_common.extract_symbol_graph-module_name"></a>module_name |  The name of the module whose symbol graph should be extracted.   |  none |
| <a id="swift_common.extract_symbol_graph-output_dir"></a>output_dir |  A directory-type `File` into which `.symbols.json` files representing the module's symbol graph will be extracted. If extraction is successful, this directory will contain a file named `${MODULE_NAME}.symbols.json`. Optionally, if the module contains extensions to types in other modules, then there will also be files named `${MODULE_NAME}@${EXTENDED_MODULE}.symbols.json`.   |  none |
| <a id="swift_common.extract_symbol_graph-swift_infos"></a>swift_infos |  A list of `SwiftInfo` providers from dependencies of the target being compiled. This should include both propagated and non-propagated (implementation-only) dependencies.   |  none |
| <a id="swift_common.extract_symbol_graph-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain.   |  none |


<a id="swift_common.get_toolchain"></a>

## swift_common.get_toolchain

<pre>
swift_common.get_toolchain(<a href="#swift_common.get_toolchain-ctx">ctx</a>, <a href="#swift_common.get_toolchain-exec_group">exec_group</a>, <a href="#swift_common.get_toolchain-mandatory">mandatory</a>, <a href="#swift_common.get_toolchain-attr">attr</a>)
</pre>

Gets the Swift toolchain associated with the rule or aspect.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.get_toolchain-ctx"></a>ctx |  The rule or aspect context.   |  none |
| <a id="swift_common.get_toolchain-exec_group"></a>exec_group |  The name of the execution group that should contain the toolchain. If this is provided and the toolchain is not declared in that execution group, it will be looked up from `ctx` as a fallback instead. If this argument is `None` (the default), then the toolchain will only be looked up from `ctx.`   |  `None` |
| <a id="swift_common.get_toolchain-mandatory"></a>mandatory |  If `False`, this function will return `None` instead of failing if no toolchain is found. Defaults to `True`.   |  `True` |
| <a id="swift_common.get_toolchain-attr"></a>attr |  The name of the attribute on the calling rule or aspect that should be used to retrieve the toolchain if it is not provided by the `toolchains` argument of the rule/aspect. Note that this is only supported for legacy/migration purposes and will be removed once migration to toolchains is complete.   |  `"_toolchain"` |

**RETURNS**

A `SwiftToolchainInfo` provider, or `None` if the toolchain was not
  found and not required.


<a id="swift_common.is_enabled"></a>

## swift_common.is_enabled

<pre>
swift_common.is_enabled(<a href="#swift_common.is_enabled-feature_configuration">feature_configuration</a>, <a href="#swift_common.is_enabled-feature_name">feature_name</a>)
</pre>

Returns `True` if the feature is enabled in the feature configuration.

This function handles both Swift-specific features and C++ features so that
users do not have to manually extract the C++ configuration in order to
check it.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.is_enabled-feature_configuration"></a>feature_configuration |  The Swift feature configuration, as returned by `configure_features`.   |  none |
| <a id="swift_common.is_enabled-feature_name"></a>feature_name |  The name of the feature to check.   |  none |

**RETURNS**

`True` if the given feature is enabled in the feature configuration.


<a id="swift_common.library_rule_attrs"></a>

## swift_common.library_rule_attrs

<pre>
swift_common.library_rule_attrs(<a href="#swift_common.library_rule_attrs-additional_deps_aspects">additional_deps_aspects</a>, <a href="#swift_common.library_rule_attrs-requires_srcs">requires_srcs</a>)
</pre>

Returns an attribute dictionary for `swift_library`-like rules.

The returned dictionary contains the same attributes that are defined by the
`swift_library` rule (including the private `_toolchain` attribute that
specifies the toolchain dependency). Users who are authoring custom rules
can use this dictionary verbatim or add other custom attributes to it in
order to make their rule a drop-in replacement for `swift_library` (for
example, if writing a custom rule that does some preprocessing or generation
of sources and then compiles them).

Do note, however, that it is the responsibility of the rule implementation
to retrieve the values of those attributes and pass them correctly to the
other `swift_common` APIs.

There is a hierarchy to the attribute sets offered by the `swift_common`
API:

1.  If you only need access to the toolchain for its tools and libraries but
    are not doing any compilation, use `toolchain_attrs`.
2.  If you need to invoke compilation actions but are not making the
    resulting object files into a static or shared library, use
    `compilation_attrs`.
3.  If you want to provide a rule interface that is suitable as a drop-in
    replacement for `swift_library`, use `library_rule_attrs`.

Each of the attribute functions in the list above also contains the
attributes from the earlier items in the list.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.library_rule_attrs-additional_deps_aspects"></a>additional_deps_aspects |  A list of additional aspects that should be applied to `deps`. Defaults to the empty list. These must be passed by the individual rules to avoid potential circular dependencies between the API and the aspects; the API loaded the aspects directly, then those aspects would not be able to load the API.   |  `[]` |
| <a id="swift_common.library_rule_attrs-requires_srcs"></a>requires_srcs |  Indicates whether the `srcs` attribute should be marked as mandatory and non-empty. Defaults to `True`.   |  `True` |

**RETURNS**

A new attribute dictionary that can be added to the attributes of a
  custom build rule to provide the same interface as `swift_library`.


<a id="swift_common.precompile_clang_module"></a>

## swift_common.precompile_clang_module

<pre>
swift_common.precompile_clang_module(<a href="#swift_common.precompile_clang_module-actions">actions</a>, <a href="#swift_common.precompile_clang_module-cc_compilation_context">cc_compilation_context</a>, <a href="#swift_common.precompile_clang_module-exec_group">exec_group</a>,
                                     <a href="#swift_common.precompile_clang_module-feature_configuration">feature_configuration</a>, <a href="#swift_common.precompile_clang_module-module_map_file">module_map_file</a>, <a href="#swift_common.precompile_clang_module-module_name">module_name</a>,
                                     <a href="#swift_common.precompile_clang_module-swift_toolchain">swift_toolchain</a>, <a href="#swift_common.precompile_clang_module-target_name">target_name</a>, <a href="#swift_common.precompile_clang_module-swift_infos">swift_infos</a>)
</pre>

Precompiles an explicit Clang module that is compatible with Swift.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.precompile_clang_module-actions"></a>actions |  The context's `actions` object.   |  none |
| <a id="swift_common.precompile_clang_module-cc_compilation_context"></a>cc_compilation_context |  A `CcCompilationContext` that contains headers and other information needed to compile this module. This compilation context should contain all headers required to compile the module, which includes the headers for the module itself *and* any others that must be present on the file system/in the sandbox for compilation to succeed. The latter typically refers to the set of headers of the direct dependencies of the module being compiled, which Clang needs to be physically present before it detects that they belong to one of the precompiled module dependencies.   |  none |
| <a id="swift_common.precompile_clang_module-exec_group"></a>exec_group |  Runs the Swift compilation action under the given execution group's context. If `None`, the default execution group is used.   |  `None` |
| <a id="swift_common.precompile_clang_module-feature_configuration"></a>feature_configuration |  A feature configuration obtained from `configure_features`.   |  none |
| <a id="swift_common.precompile_clang_module-module_map_file"></a>module_map_file |  A textual module map file that defines the Clang module to be compiled.   |  none |
| <a id="swift_common.precompile_clang_module-module_name"></a>module_name |  The name of the top-level module in the module map that will be compiled.   |  none |
| <a id="swift_common.precompile_clang_module-swift_toolchain"></a>swift_toolchain |  The `SwiftToolchainInfo` provider of the toolchain.   |  none |
| <a id="swift_common.precompile_clang_module-target_name"></a>target_name |  The name of the target for which the code is being compiled, which is used to determine unique file paths for the outputs.   |  none |
| <a id="swift_common.precompile_clang_module-swift_infos"></a>swift_infos |  A list of `SwiftInfo` providers representing dependencies required to compile this module.   |  `[]` |

**RETURNS**

A struct containing the precompiled module and optional indexstore directory,
  or `None` if the toolchain or target does not support precompiled modules.


<a id="swift_common.toolchain_attrs"></a>

## swift_common.toolchain_attrs

<pre>
swift_common.toolchain_attrs(<a href="#swift_common.toolchain_attrs-toolchain_attr_name">toolchain_attr_name</a>)
</pre>

Returns an attribute dictionary for toolchain users.

The returned dictionary contains a key with the name specified by the
argument `toolchain_attr_name` (which defaults to the value `"_toolchain"`),
the value of which is a BUILD API `attr.label` that references the default
Swift toolchain. Users who are authoring custom rules can add this
dictionary to the attributes of their own rule in order to depend on the
toolchain and access its `SwiftToolchainInfo` provider to pass it to other
`swift_common` functions.

There is a hierarchy to the attribute sets offered by the `swift_common`
API:

1.  If you only need access to the toolchain for its tools and libraries but
    are not doing any compilation, use `toolchain_attrs`.
2.  If you need to invoke compilation actions but are not making the
    resulting object files into a static or shared library, use
    `compilation_attrs`.
3.  If you want to provide a rule interface that is suitable as a drop-in
    replacement for `swift_library`, use `library_rule_attrs`.

Each of the attribute functions in the list above also contains the
attributes from the earlier items in the list.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.toolchain_attrs-toolchain_attr_name"></a>toolchain_attr_name |  The name of the attribute that should be created that points to the toolchain. This defaults to `_toolchain`, which is sufficient for most rules; it is customizable for certain aspects where having an attribute with the same name but different values applied to a particular target causes a build crash.   |  `"_toolchain"` |

**RETURNS**

A new attribute dictionary that can be added to the attributes of a
  custom build rule to provide access to the Swift toolchain.


<a id="swift_common.use_toolchain"></a>

## swift_common.use_toolchain

<pre>
swift_common.use_toolchain(<a href="#swift_common.use_toolchain-mandatory">mandatory</a>)
</pre>

Returns a list of toolchain types needed to use the Swift toolchain.

This function returns a list so that it can be easily composed with other
toolchains if necessary. For example, a rule with multiple toolchain
dependencies could write:

```
toolchains = use_swift_toolchain() + [other toolchains...]
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="swift_common.use_toolchain-mandatory"></a>mandatory |  Whether or not it should be an error if the toolchain cannot be resolved. Defaults to True.   |  `True` |

**RETURNS**

A list of
  [toolchain types](https://bazel.build/rules/lib/builtins/toolchain_type.html)
  that should be passed to `rule()`, `aspect()`, or `exec_group()`.


