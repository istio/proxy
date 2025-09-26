# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Implementation of the `swift_test` rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("//swift/internal:compiling.bzl", "compile")
load("//swift/internal:env_expansion.bzl", "expanded_env")
load(
    "//swift/internal:feature_names.bzl",
    "SWIFT_FEATURE_ADD_TARGET_NAME_TO_OUTPUT",
)
load("//swift/internal:features.bzl", "is_feature_enabled")
load(
    "//swift/internal:linking.bzl",
    "binary_rule_attrs",
    "configure_features_for_binary",
    "malloc_linking_context",
    "register_link_binary_action",
)
load(
    "//swift/internal:output_groups.bzl",
    "supplemental_compilation_output_groups",
)
load("//swift/internal:providers.bzl", "SwiftCompilerPluginInfo")
load(
    "//swift/internal:swift_symbol_graph_aspect.bzl",
    "make_swift_symbol_graph_aspect",
)
load("//swift/internal:symbol_graph_extracting.bzl", "extract_symbol_graph")
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)
load(
    "//swift/internal:utils.bzl",
    "expand_locations",
    "get_providers",
    "include_developer_search_paths",
)
load(":module_name.bzl", "derive_swift_module_name")
load(
    ":providers.bzl",
    "SwiftBinaryInfo",
    "SwiftInfo",
    "SwiftSymbolGraphInfo",
    "create_swift_module_context",
)

_test_discovery_symbol_graph_aspect = make_swift_symbol_graph_aspect(
    default_emit_extension_block_symbols = "0",
    default_minimum_access_level = "internal",
    testonly_targets = True,
)

def _maybe_parse_as_library_copts(srcs):
    """Returns a list of compiler flags depending on `main.swift`'s presence.

    Now that the `@main` attribute exists and is becoming more common, in the
    case there is a single file not named `main.swift`, we assume that it has a
    `@main` annotation, in which case it needs to be parsed as a library, not
    as if it has top level code. In the case this is the wrong assumption,
    compilation or linking will fail.

    Args:
        srcs: A list of source files to check for the presence of `main.swift`.

    Returns:
        A list of compiler flags to add to `copts`
    """
    use_parse_as_library = len(srcs) == 1 and \
                           srcs[0].basename != "main.swift"
    return ["-parse-as-library"] if use_parse_as_library else []

def _create_xctest_bundle(name, actions, binary):
    """Creates an `.xctest` bundle that contains the given binary.

    Args:
        name: The name of the target being built, which will be used as the
            basename of the bundle (followed by the .xctest bundle extension).
        actions: The context's actions object.
        binary: The binary that will be copied into the test bundle.

    Returns:
        A `File` (tree artifact) representing the `.xctest` bundle.
    """
    xctest_bundle = actions.declare_directory("{}.xctest".format(name))

    args = actions.args()
    args.add(xctest_bundle.path)
    args.add(binary)

    # When XCTest loads this bundle, it will create an instance of this class
    # which will register the observer that writes the XML output.
    plist = '{ NSPrincipalClass = "BazelXMLTestObserverRegistration"; }'

    actions.run_shell(
        arguments = [args],
        command = (
            'mkdir -p "$1/Contents/MacOS" && ' +
            'cp "$2" "$1/Contents/MacOS" && ' +
            'echo \'{}\' > "$1/Contents/Info.plist"'.format(plist)
        ),
        inputs = [binary],
        mnemonic = "SwiftCreateTestBundle",
        outputs = [xctest_bundle],
        progress_message = "Creating test bundle for {}".format(name),
    )

    return xctest_bundle

def _create_xctest_runner(name, actions, bundle, xctest_runner_template):
    """Creates a script that will launch `xctest` with the given test bundle.

    Args:
        name: The name of the target being built, which will be used as the
            basename of the test runner script.
        actions: The context's actions object.
        bundle: The `File` representing the `.xctest` bundle that should be
            executed.
        xctest_runner_template: The `File` that will be used as a template to
            generate the test runner shell script.

    Returns:
        A `File` representing the shell script that will launch the test bundle
        with the `xctest` tool.
    """
    xctest_runner = actions.declare_file("{}.test-runner.sh".format(name))

    actions.expand_template(
        is_executable = True,
        output = xctest_runner,
        template = xctest_runner_template,
        substitutions = {
            "%bundle%": bundle.short_path,
        },
    )

    return xctest_runner

def _generate_test_discovery_srcs(
        *,
        actions,
        deps,
        name,
        owner_module_name,
        owner_symbol_graph_dir = None,
        test_discoverer):
    """Generate Swift sources to run discovered XCTest-style tests.

    The `owner_module_name` and `owner_symbol_graph_dir` arguments are used to
    support discovery of tests from the sources of the `swift_test` target
    itself. If they are provided, then that symbol graph is used *instead of*
    the symbol graphs of the direct dependencies.

    Args:
        actions: The context's actions object.
        deps: The list of direct dependencies of the test target.
        name: The name of the target being built, which will be used to derive
            the basename of the directory containing the generated files.
        owner_module_name: The name of the owner module (the target being
            built).
        owner_symbol_graph_dir: A directory-type `File` containing the extracted
            symbol graph for the owner target.
        test_discoverer: The executable `File` representing the test discoverer
            tool that will be spawned to generate the test runner sources.

    Returns:
        A list of `File`s representing generated `.swift` source files that
        should be compiled as part of the test target.
    """
    inputs = []
    outputs = []
    modules_to_scan = []
    args = actions.args()

    if owner_symbol_graph_dir:
        inputs.append(owner_symbol_graph_dir)
        modules_to_scan.append(owner_module_name)

    for dep in deps:
        if SwiftSymbolGraphInfo not in dep:
            continue

        symbol_graph_info = dep[SwiftSymbolGraphInfo]

        # Only include the direct symbol graphs if the owner didn't have any
        # sources.
        if not owner_symbol_graph_dir:
            modules_to_scan.extend([
                symbol_graph.module_name
                for symbol_graph in symbol_graph_info.direct_symbol_graphs
            ])

        # Always include the transitive symbol graphs; if a library depends on a
        # support class that inherits from `XCTestCase`, we need to be able to
        # detect that.
        for symbol_graph in (
            symbol_graph_info.transitive_symbol_graphs.to_list()
        ):
            inputs.append(symbol_graph.symbol_graph_dir)

    if not modules_to_scan:
        fail("Failed to find any modules to inspect for tests.")

    # For each direct dependency/module that we have a symbol graph for (i.e.,
    # every testonly dependency), declare a `.swift` source file where the
    # discovery tool will generate an extension that lists the test entries for
    # the classes/methods found in that module.
    for module_name in modules_to_scan:
        output_file = actions.declare_file(
            "{target}_test_discovery_srcs/{module}.entries.swift".format(
                module = module_name,
                target = name,
            ),
        )
        outputs.append(output_file)
        args.add(
            "--module-output",
            "{module}={path}".format(
                module = module_name,
                path = output_file.path,
            ),
        )

    # Also declare a single `main.swift` file where the discovery tool will
    # generate the main runner.
    main_file = actions.declare_file(
        "{target}_test_discovery_srcs/main.swift".format(target = name),
    )
    outputs.append(main_file)
    args.add("--main-output", main_file)

    # The discovery tool expects symbol graph directories as its inputs (it
    # iterates over their contents), so we must not expand directories here.
    args.add_all(inputs, expand_directories = False, uniquify = True)

    actions.run(
        arguments = [args],
        executable = test_discoverer,
        inputs = inputs,
        mnemonic = "SwiftTestDiscovery",
        outputs = outputs,
        progress_message = "Discovering tests for %{label}",
    )

    return outputs

def _do_compile(
        *,
        ctx,
        additional_copts = [],
        cc_infos,
        feature_configuration,
        include_dev_srch_paths,
        module_name,
        objc_infos,
        name,
        package_name,
        plugins = [],
        srcs,
        swift_infos,
        swift_toolchain,
        workspace_name):
    """Compiles Swift source code for a `swift_test` target.

    Args:
        ctx: The rule context.
        additional_copts: Additional Swift compiler options that should be used
            for this compilation action.
        cc_infos: A list of `CcInfo` provider's that should be
            provided as inputs to the compilation action.
        feature_configuration: The feature configuration to use for compiling.
        include_dev_srch_paths: A `bool` that indicates whether the developer
            framework search paths will be added to the compilation command.
        module_name: The name of the module being compiled.
        name: The target name or a value derived from the target name that is
            used to name output files generated by the action.
        objc_infos: A list of `apple_common.ObjC` providers that should be
            provided as inputs to the compilation action.
        package_name: The semantic package of the name of the Swift module
            being compiled.
        plugins: A list of `SwiftCompilerPluginInfo` providers that need to be
            loaded when compiling this module.
        srcs: The sources to compile.
        swift_infos: A list of `SwiftInfo` providers that should be used to
            determine the module inputs for the action.
        swift_toolchain: The Swift toolchain to use to configure the build.
        workspace_name: The name of the workspace for which the code is being
             compiled, which is used to determine unique file paths for some
             outputs.

    Returns:
        The same value as would be returned by `compile`.
    """
    return compile(
        actions = ctx.actions,
        additional_inputs = ctx.files.swiftc_inputs,
        cc_infos = cc_infos,
        copts = expand_locations(
            ctx,
            ctx.attr.copts,
            ctx.attr.swiftc_inputs,
        ) + additional_copts,
        defines = ctx.attr.defines,
        feature_configuration = feature_configuration,
        include_dev_srch_paths = include_dev_srch_paths,
        module_name = module_name,
        package_name = package_name,
        plugins = plugins,
        objc_infos = objc_infos,
        srcs = srcs,
        swift_infos = swift_infos,
        swift_toolchain = swift_toolchain,
        target_name = name,
        workspace_name = workspace_name,
    )

def _swift_test_impl(ctx):
    swift_toolchain = get_swift_toolchain(ctx)

    feature_configuration = configure_features_for_binary(
        ctx = ctx,
        requested_features = ctx.features,
        swift_toolchain = swift_toolchain,
        unsupported_features = ctx.disabled_features,
    )

    discover_tests = ctx.attr.discover_tests
    uses_xctest_bundles = swift_toolchain.test_configuration.uses_xctest_bundles
    is_bundled = discover_tests and uses_xctest_bundles

    # If we need to run the test in an .xctest bundle, the binary must have
    # Mach-O type `MH_BUNDLE` instead of `MH_EXECUTE`.
    extra_linkopts = ["-Wl,-bundle"] if is_bundled else []

    deps = list(ctx.attr.deps)
    test_runner_deps = list(ctx.attr._test_runner_deps)

    # In test discovery mode (whether manual or by the Obj-C runtime), inject
    # the test observer that prints the xUnit-style output for Bazel. Otherwise
    # don't link this, because we don't want it to pull in link time
    # dependencies on XCTest, which the test binary may not be using.
    if discover_tests:
        additional_link_deps = test_runner_deps
    else:
        additional_link_deps = []

    # `is_feature_enabled` isn't used, as it requires the prefix of the feature
    # to start with `swift.`
    swizzle_absolute_xcttestsourcelocation = (
        "apple.swizzle_absolute_xcttestsourcelocation" in
        feature_configuration._enabled_features
    )
    if swizzle_absolute_xcttestsourcelocation:
        additional_link_deps.append(
            ctx.attr._swizzle_absolute_xcttestsourcelocation,
        )

    # We also need to collect nested providers from `SwiftBinaryInfo` since we
    # support testing those.
    deps_cc_infos = []
    deps_compilation_contexts = []
    deps_objc_infos = []
    deps_swift_infos = []
    additional_linking_contexts = []
    for dep in deps:
        if CcInfo in dep:
            deps_cc_infos.append(dep[CcInfo])
            deps_compilation_contexts.append(dep[CcInfo].compilation_context)
        if apple_common.Objc in dep:
            deps_objc_infos.append(dep[apple_common.Objc])
        if SwiftInfo in dep:
            deps_swift_infos.append(dep[SwiftInfo])
        if SwiftBinaryInfo in dep:
            plugin_info = dep[SwiftBinaryInfo]
            deps_swift_infos.append(plugin_info.swift_info)
            additional_linking_contexts.append(
                plugin_info.cc_info.linking_context,
            )
    additional_linking_contexts.append(malloc_linking_context(ctx))

    test_runner_deps_cc_infos = get_providers(test_runner_deps, CcInfo)
    test_runner_deps_swift_infos = get_providers(test_runner_deps, SwiftInfo)

    srcs = ctx.files.srcs
    owner_symbol_graph_dir = None

    module_name = ctx.attr.module_name
    if not module_name:
        module_name = derive_swift_module_name(ctx.label)

    include_dev_srch_paths = include_developer_search_paths(ctx.attr)

    module_contexts = []
    all_supplemental_outputs = []

    if srcs:
        # If the `swift_test` target had sources, compile those first and then
        # extract a symbol graph from it.
        compile_result = _do_compile(
            ctx = ctx,
            # In test discovery mode (whether manual or by the Obj-C runtime),
            # compile the code with `-parse-as-library` to avoid the case where
            # a single file with no top-level code still produces an empty
            # `main`. Also compile with `-enable-testing`, because the generated
            # sources will `@testable import` this module, and this allows that
            # to work even when building in `-c opt` mode.
            additional_copts = [
                "-parse-as-library",
                "-enable-testing",
            ] if discover_tests else _maybe_parse_as_library_copts(srcs),
            cc_infos = deps_cc_infos,
            feature_configuration = feature_configuration,
            include_dev_srch_paths = include_dev_srch_paths,
            module_name = module_name,
            objc_infos = deps_objc_infos,
            package_name = ctx.attr.package_name,
            plugins = get_providers(ctx.attr.plugins, SwiftCompilerPluginInfo),
            name = ctx.label.name,
            srcs = srcs,
            swift_infos = deps_swift_infos,
            swift_toolchain = swift_toolchain,
            workspace_name = ctx.workspace_name,
        )

        module_contexts.append(compile_result.module_context)
        compilation_outputs = compile_result.compilation_outputs
        all_supplemental_outputs.append(compile_result.supplemental_outputs)

        swift_infos_including_owner = [compile_result.swift_info]

        # If we're going to do test discovery below, extract the symbol graph of
        # the module that we just compiled so that we can discover any tests in
        # the `srcs` of this target (instead of just in the direct `deps`).
        if not is_bundled:
            owner_symbol_graph_dir = ctx.actions.declare_directory(
                "{}.symbolgraphs".format(ctx.label.name),
            )
            extract_symbol_graph(
                actions = ctx.actions,
                compilation_contexts = deps_compilation_contexts,
                feature_configuration = feature_configuration,
                include_dev_srch_paths = include_dev_srch_paths,
                minimum_access_level = "internal",
                module_name = module_name,
                output_dir = owner_symbol_graph_dir,
                swift_infos = swift_infos_including_owner,
                swift_toolchain = swift_toolchain,
            )
    else:
        compilation_outputs = cc_common.create_compilation_outputs()
        swift_infos_including_owner = deps_swift_infos

    # If requested, discover tests using symbol graphs and generate a runner for
    # them.
    if discover_tests and not uses_xctest_bundles:
        discovery_srcs = _generate_test_discovery_srcs(
            actions = ctx.actions,
            deps = ctx.attr.deps,
            name = ctx.label.name,
            owner_module_name = module_name,
            owner_symbol_graph_dir = owner_symbol_graph_dir,
            test_discoverer = ctx.executable._test_discoverer,
        )
        discovery_compile_result = _do_compile(
            ctx = ctx,
            # The generated test runner uses `@main`.
            additional_copts = ["-parse-as-library"],
            cc_infos = test_runner_deps_cc_infos,
            feature_configuration = feature_configuration,
            include_dev_srch_paths = include_dev_srch_paths,
            module_name = module_name + "__GeneratedTestDiscoveryRunner",
            name = ctx.label.name + "__GeneratedTestDiscoveryRunner",
            objc_infos = deps_objc_infos,
            package_name = ctx.attr.package_name,
            srcs = discovery_srcs,
            swift_infos = (
                swift_infos_including_owner + test_runner_deps_swift_infos
            ),
            swift_toolchain = swift_toolchain,
            workspace_name = ctx.workspace_name,
        )
        module_contexts.append(discovery_compile_result.module_context)
        compilation_outputs = cc_common.merge_compilation_outputs(
            compilation_outputs = [
                compilation_outputs,
                discovery_compile_result.compilation_outputs,
            ],
        )
        all_supplemental_outputs.append(
            discovery_compile_result.supplemental_outputs,
        )

    # If we need to run the test in an .xctest bundle, the binary must have
    # Mach-O type `MH_BUNDLE` instead of `MH_EXECUTE`.
    extra_linkopts = ["-Wl,-bundle"] if is_bundled else []

    # Apply the optional debugging outputs extension if the toolchain defines
    # one.
    debug_outputs_provider = swift_toolchain.debug_outputs_provider
    if debug_outputs_provider:
        debug_extension = debug_outputs_provider(ctx = ctx)
        additional_debug_outputs = debug_extension.additional_outputs
        variables_extension = debug_extension.variables_extension
    else:
        additional_debug_outputs = []
        variables_extension = {}

    if is_feature_enabled(
        feature_configuration = feature_configuration,
        feature_name = SWIFT_FEATURE_ADD_TARGET_NAME_TO_OUTPUT,
    ):
        name = paths.join(ctx.label.name, ctx.label.name)
    else:
        name = ctx.label.name

    linking_outputs = register_link_binary_action(
        actions = ctx.actions,
        additional_inputs = ctx.files.swiftc_inputs,
        additional_linking_contexts = additional_linking_contexts,
        additional_outputs = additional_debug_outputs,
        compilation_outputs = compilation_outputs,
        deps = deps + additional_link_deps,
        feature_configuration = feature_configuration,
        module_contexts = module_contexts,
        name = name,
        output_type = "executable",
        owner = ctx.label,
        stamp = ctx.attr.stamp,
        swift_toolchain = swift_toolchain,
        user_link_flags = expand_locations(
            ctx,
            ctx.attr.linkopts,
            ctx.attr.swiftc_inputs,
        ) + extra_linkopts + ctx.fragments.cpp.linkopts,
        variables_extension = variables_extension,
    )

    # If the tests are to be bundled, create the bundle and the test runner
    # script that launches it via `xctest`. Otherwise, just use the binary
    # itself as the executable to launch.
    if is_bundled:
        xctest_bundle = _create_xctest_bundle(
            name = ctx.label.name,
            actions = ctx.actions,
            binary = linking_outputs.executable,
        )
        xctest_runner = _create_xctest_runner(
            name = ctx.label.name,
            actions = ctx.actions,
            bundle = xctest_bundle,
            xctest_runner_template = ctx.file._xctest_runner_template,
        )
        additional_test_outputs = [xctest_bundle]
        executable = xctest_runner
    else:
        additional_test_outputs = []
        executable = linking_outputs.executable

    test_environment = dicts.add(
        swift_toolchain.test_configuration.env,
        {"TEST_BINARIES_FOR_LLVM_COV": linking_outputs.executable.short_path},
        expanded_env.get_expanded_env(ctx, {}),
    )

    return [
        DefaultInfo(
            executable = executable,
            files = depset(
                [executable] + additional_test_outputs +
                additional_debug_outputs,
            ),
            runfiles = ctx.runfiles(
                collect_data = True,
                collect_default = True,
                files = ctx.files.data + additional_test_outputs,
                transitive_files = ctx.attr._apple_coverage_support.files,
            ),
        ),
        OutputGroupInfo(
            **supplemental_compilation_output_groups(*all_supplemental_outputs)
        ),
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["deps"],
            extensions = ["swift"],
            source_attributes = ["srcs"],
        ),
        SwiftInfo(
            modules = [
                create_swift_module_context(
                    name = module_context.name,
                    compilation_context = module_context.compilation_context,
                    # The rest of the fields are intentionally ommited, as we
                    # only want to expose the compilation_context
                )
                for module_context in module_contexts
            ],
        ),
        testing.ExecutionInfo(
            swift_toolchain.test_configuration.execution_requirements,
        ),
        testing.TestEnvironment(test_environment),
    ]

swift_test = rule(
    attrs = dicts.add(
        binary_rule_attrs(
            additional_deps_aspects = [_test_discovery_symbol_graph_aspect],
            additional_deps_providers = [[SwiftCompilerPluginInfo]],
            stamp_default = 0,
        ),
        {
            "discover_tests": attr.bool(
                default = True,
                doc = """\
Determines whether or not tests are automatically discovered in the binary. The
default value is `True`.

If tests are discovered, then you should not provide your own `main` entry point
in the `swift_test` binary; the test runtime provides the entry point for you.
If you set this attribute to `False`, then you are responsible for providing
your own `main`. This allows you to write tests that use a framework other than
Apple's `XCTest`. The only requirement of such a test is that it terminate with
a zero exit code for success or a non-zero exit code for failure.

Additionally, on Apple platforms, test discovery is handled by the Objective-C
runtime and the output of a `swift_test` rule is an `.xctest` bundle that is
invoked using the `xctest` tool in Xcode. If this attribute is used to disable
test discovery, then the output of the `swift_test` rule will instead be a
standard executable binary that is invoked directly.
""",
                mandatory = False,
            ),
            "env": attr.string_dict(
                doc = """
                Dictionary of environment variables that should be set during the test execution.
                """,
            ),
            "_apple_coverage_support": attr.label(
                cfg = "exec",
                default = Label(
                    "@build_bazel_apple_support//tools:coverage_support",
                ),
            ),
            "_swizzle_absolute_xcttestsourcelocation": attr.label(
                default = Label(
                    "//swift/internal:swizzle_absolute_xcttestsourcelocation",
                ),
            ),
            "_test_discoverer": attr.label(
                cfg = "exec",
                default = Label("//tools/test_discoverer"),
                executable = True,
            ),
            "_test_runner_deps": attr.label_list(
                default = [
                    Label("//tools/test_observer"),
                ],
            ),
            "_xctest_runner_template": attr.label(
                allow_single_file = True,
                default = Label("//tools/xctest_runner:xctest_runner_template"),
            ),
            # TODO(b/301253335): Enable AEGs and switch from `swift` exec_group to swift `toolchain` param.
            "_use_auto_exec_groups": attr.bool(default = False),
        },
    ),
    doc = """\
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
""",
    executable = True,
    fragments = ["cpp"],
    test = True,
    implementation = _swift_test_impl,
    toolchains = use_swift_toolchain(),
)
