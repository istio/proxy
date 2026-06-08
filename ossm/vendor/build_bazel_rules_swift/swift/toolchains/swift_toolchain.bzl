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

"""BUILD rules used to provide a Swift toolchain on Linux.

The rules defined in this file are not intended to be used outside of the Swift
toolchain package. If you are looking for rules to build Swift code using this
toolchain, see `swift.bzl`.
"""

load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(
    "@bazel_tools//tools/cpp:toolchain_utils.bzl",
    "find_cpp_toolchain",
    "use_cpp_toolchain",
)
load(
    "//swift:providers.bzl",
    "SwiftFeatureAllowlistInfo",
    "SwiftPackageConfigurationInfo",
    "SwiftToolchainInfo",
)
load(
    "//swift/internal:action_names.bzl",
    "SWIFT_ACTION_AUTOLINK_EXTRACT",
    "SWIFT_ACTION_COMPILE",
    "SWIFT_ACTION_DERIVE_FILES",
    "SWIFT_ACTION_DUMP_AST",
    "SWIFT_ACTION_MODULEWRAP",
    "SWIFT_ACTION_PRECOMPILE_C_MODULE",
    "SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT",
)
load("//swift/internal:attrs.bzl", "swift_toolchain_driver_attrs")
load("//swift/internal:autolinking.bzl", "autolink_extract_action_configs")
load(
    "//swift/internal:feature_names.bzl",
    "SWIFT_FEATURE_MODULE_MAP_HOME_IS_CWD",
    "SWIFT_FEATURE_USE_AUTOLINK_EXTRACT",
    "SWIFT_FEATURE_USE_GLOBAL_INDEX_STORE",
    "SWIFT_FEATURE_USE_MODULE_WRAP",
)
load(
    "//swift/internal:features.bzl",
    "default_features_for_toolchain",
    "features_for_build_modes",
)
load(
    "//swift/internal:providers.bzl",
    "SwiftCrossImportOverlayInfo",
    "SwiftModuleAliasesInfo",
)
load("//swift/internal:target_triples.bzl", "target_triples")
load(
    "//swift/internal:utils.bzl",
    "collect_implicit_deps_providers",
    "get_swift_executable_for_toolchain",
)
load("//swift/internal:wmo.bzl", "features_from_swiftcopts")
load(
    "//swift/toolchains/config:action_config.bzl",
    "ActionConfigInfo",
    "add_arg",
)
load(
    "//swift/toolchains/config:all_actions_config.bzl",
    "all_actions_action_configs",
)
load("//swift/toolchains/config:compile_config.bzl", "compile_action_configs")
load(
    "//swift/toolchains/config:modulewrap_config.bzl",
    "modulewrap_action_configs",
)
load(
    "//swift/toolchains/config:symbol_graph_config.bzl",
    "symbol_graph_action_configs",
)
load("//swift/toolchains/config:tool_config.bzl", "ToolConfigInfo")

def _swift_compile_resource_set(_os, inputs_size):
    # The `os` argument is unused, but the Starlark API requires both
    # positional arguments.
    return {"cpu": 1, "memory": 200. + inputs_size * 0.015}

def _all_tool_configs(
        env,
        swift_executable,
        toolchain_root,
        use_autolink_extract,
        use_module_wrap,
        additional_tools,
        tool_executable_suffix):
    """Returns the tool configurations for the Swift toolchain.

    Args:
        env: A custom environment to execute the tools in.
        swift_executable: A custom Swift driver executable to be used during the
            build, if provided.
        toolchain_root: The root directory of the toolchain.
        use_autolink_extract: If True, the link action should use
            `swift-autolink-extract` to extract the complier directed linking
            flags.
        use_module_wrap: If True, the compile action should embed the
            swiftmodule into the final image.
        additional_tools: A list of extra tools to pass to each driver config,
            in a format suitable for the `tools` argument to `ctx.actions.run`.
        tool_executable_suffix: The suffix for executable tools to use (e.g.
            `.exe` on Windows).

    Returns:
        A dictionary mapping action name to tool configurations.
    """

    def _driver_config(*, mode):
        return {
            "mode": mode,
            "swift_executable": swift_executable,
            "tool_executable_suffix": tool_executable_suffix,
            "toolchain_root": toolchain_root,
        }

    compile_tool_config = ToolConfigInfo(
        additional_tools = additional_tools,
        driver_config = _driver_config(mode = "swiftc"),
        resource_set = _swift_compile_resource_set,
        use_param_file = True,
        worker_mode = "persistent",
        env = env,
    )

    tool_configs = {
        SWIFT_ACTION_COMPILE: compile_tool_config,
        SWIFT_ACTION_DERIVE_FILES: compile_tool_config,
        SWIFT_ACTION_DUMP_AST: compile_tool_config,
        SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT: ToolConfigInfo(
            additional_tools = additional_tools,
            driver_config = _driver_config(mode = "swift-symbolgraph-extract"),
            use_param_file = True,
            worker_mode = "wrap",
            env = env,
        ),
    }

    if use_autolink_extract:
        tool_configs[SWIFT_ACTION_AUTOLINK_EXTRACT] = ToolConfigInfo(
            additional_tools = additional_tools,
            driver_config = _driver_config(mode = "swift-autolink-extract"),
            worker_mode = "wrap",
        )

    if use_module_wrap:
        tool_configs[SWIFT_ACTION_MODULEWRAP] = ToolConfigInfo(
            additional_tools = additional_tools,
            # This must come first after the driver name.
            args = ["-modulewrap"],
            driver_config = _driver_config(mode = "swift"),
            worker_mode = "wrap",
        )

    return tool_configs

def _all_action_configs(os, arch, target_triple, sdkroot, xctest_version, additional_swiftc_copts):
    """Returns the action configurations for the Swift toolchain.

    Args:
        os: The OS that we are compiling for.
        arch: The architecture we are compiling for.
        target_triple: The triple of the platform being targeted.
        sdkroot: The path to the SDK that we should use to build against.
        xctest_version: The version of XCTest to use.
        additional_swiftc_copts: Additional Swift compiler flags obtained from
            the `.../swift:copt` build setting.

    Returns:
        A list of action configurations for the toolchain.
    """

    # Basic compilation flags (target triple and toolchain search paths).
    action_configs = [
        ActionConfigInfo(
            actions = [
                SWIFT_ACTION_SYMBOL_GRAPH_EXTRACT,
            ],
            configurators = [
                add_arg("-target", target_triples.str(target_triple)),
            ],
        ),
    ]
    if sdkroot:
        action_configs = [
            ActionConfigInfo(
                actions = [
                    SWIFT_ACTION_COMPILE,
                    SWIFT_ACTION_DERIVE_FILES,
                    SWIFT_ACTION_DUMP_AST,
                    SWIFT_ACTION_PRECOMPILE_C_MODULE,
                ],
                configurators = [add_arg("-sdk", sdkroot)],
            ),
        ]

        if os and xctest_version:
            action_configs.append(
                ActionConfigInfo(
                    actions = [
                        SWIFT_ACTION_COMPILE,
                        SWIFT_ACTION_DERIVE_FILES,
                        SWIFT_ACTION_DUMP_AST,
                        SWIFT_ACTION_PRECOMPILE_C_MODULE,
                    ],
                    configurators = [
                        add_arg(
                            paths.join(
                                sdkroot,
                                "..",
                                "..",
                                "Library",
                                "XCTest-{}".format(xctest_version),
                                "usr",
                                "lib",
                                "swift",
                                os,
                            ),
                            format = "-I%s",
                        ),
                    ],
                ),
            )

            # Compatibility with older builds of the Swift SDKs.
            if arch:
                action_configs.append(
                    ActionConfigInfo(
                        actions = [
                            SWIFT_ACTION_COMPILE,
                            SWIFT_ACTION_DERIVE_FILES,
                            SWIFT_ACTION_DUMP_AST,
                            SWIFT_ACTION_PRECOMPILE_C_MODULE,
                        ],
                        configurators = [
                            add_arg(
                                paths.join(
                                    sdkroot,
                                    "..",
                                    "..",
                                    "Library",
                                    "XCTest-{}".format(xctest_version),
                                    "usr",
                                    "lib",
                                    "swift",
                                    os,
                                    arch,
                                ),
                                format = "-I%s",
                            ),
                        ],
                    ),
                )

    action_configs.extend(all_actions_action_configs())
    action_configs.extend(
        compile_action_configs(
            additional_swiftc_copts = additional_swiftc_copts,
        ),
    )
    action_configs.extend(modulewrap_action_configs())
    action_configs.extend(autolink_extract_action_configs())
    action_configs.extend(symbol_graph_action_configs())

    return action_configs

def _swift_windows_linkopts_cc_info(
        arch,
        sdkroot,
        xctest_version,
        toolchain_label):
    """Returns a `CcInfo` containing flags that should be passed to the linker.

    The provider returned by this function will be used as an implicit
    dependency of the toolchain to ensure that any binary containing Swift code
    will link to the standard libraries correctly.

    Args:
        arch: The CPU architecture, which is used as part of the library path.
        sdkroot: The path to the root of the SDK that we are building against.
        xctest_version: The version of XCTest that we are building against.
        toolchain_label: The label of the Swift toolchain that will act as the
            owner of the linker input propagating the flags.

    Returns:
        A `CcInfo` provider that will provide linker flags to binaries that
        depend on Swift targets.
    """
    platform_lib_dir = "{sdkroot}/usr/lib/swift/windows/{arch}".format(
        sdkroot = sdkroot,
        arch = arch,
    )

    runtime_object_path = "{sdkroot}/usr/lib/swift/windows/{arch}/swiftrt.obj".format(
        sdkroot = sdkroot,
        arch = arch,
    )

    linkopts = [
        "-LIBPATH:{}".format(platform_lib_dir),
        "-LIBPATH:{}".format(paths.join(sdkroot, "..", "..", "Library", "XCTest-{}".format(xctest_version), "usr", "lib", "swift", "windows", arch)),
        runtime_object_path,
    ]

    return CcInfo(
        linking_context = cc_common.create_linking_context(
            linker_inputs = depset([
                cc_common.create_linker_input(
                    owner = toolchain_label,
                    user_link_flags = depset(linkopts),
                ),
            ]),
        ),
    )

def _swift_unix_linkopts_cc_info(
        cpu,
        os,
        toolchain_label,
        toolchain_root):
    """Returns a `CcInfo` containing flags that should be passed to the linker.

    The provider returned by this function will be used as an implicit
    dependency of the toolchain to ensure that any binary containing Swift code
    will link to the standard libraries correctly.

    Args:
        cpu: The CPU architecture, which is used as part of the library path.
        os: The operating system name, which is used as part of the library
            path.
        toolchain_label: The label of the Swift toolchain that will act as the
            owner of the linker input propagating the flags.
        toolchain_root: The toolchain's root directory.

    Returns:
        A `CcInfo` provider that will provide linker flags to binaries that
        depend on Swift targets.
    """

    # TODO(#8): Support statically linking the Swift runtime.
    platform_lib_dir = "{toolchain_root}/lib/swift/{os}".format(
        os = os,
        toolchain_root = toolchain_root,
    )

    runtime_object_path = "{platform_lib_dir}/{cpu}/swiftrt.o".format(
        cpu = cpu,
        platform_lib_dir = platform_lib_dir,
    )

    linkopts = [
        "-pie",
        "-L{}".format(platform_lib_dir),
        "-Wl,-rpath,{}".format(platform_lib_dir),
        "-lm",
        "-lstdc++",
        "-lrt",
        "-ldl",
        runtime_object_path,
        "-static-libgcc",
    ]

    return CcInfo(
        linking_context = cc_common.create_linking_context(
            linker_inputs = depset([
                cc_common.create_linker_input(
                    owner = toolchain_label,
                    user_link_flags = depset(linkopts),
                ),
            ]),
        ),
    )

def _entry_point_linkopts_provider(*, entry_point_name):
    """Returns linkopts to customize the entry point of a binary."""
    return struct(
        linkopts = ["-Wl,--defsym,main={}".format(entry_point_name)],
    )

def _parse_target_system_name(*, arch, os, target_system_name):
    """Returns the target system name set by the CC toolchain or attempts to create one based on the OS and arch."""

    if target_system_name != "local":
        return target_system_name

    if os == "linux":
        return "%s-unknown-linux-gnu" % arch
    else:
        return "%s-unknown-%s" % (arch, os)

def _swift_toolchain_impl(ctx):
    toolchain_root = ctx.attr.root
    cc_toolchain = find_cpp_toolchain(ctx)
    target_system_name = _parse_target_system_name(
        arch = ctx.attr.arch,
        os = ctx.attr.os,
        target_system_name = cc_toolchain.target_gnu_system_name,
    )
    target_triple = target_triples.normalize_for_swift(
        target_triples.parse(target_system_name),
    )

    if "clang" not in cc_toolchain.compiler:
        fail("Swift requires the configured CC toolchain to be LLVM (clang). " +
             "Either use the locally installed LLVM by setting `CC=clang` in your environment " +
             "before invoking Bazel, or configure a Bazel LLVM CC toolchain.")

    if ctx.attr.os == "windows":
        swift_linkopts_cc_info = _swift_windows_linkopts_cc_info(
            ctx.attr.arch,
            ctx.attr.sdkroot,
            ctx.attr.xctest_version,
            ctx.label,
        )
    else:
        swift_linkopts_cc_info = _swift_unix_linkopts_cc_info(
            ctx.attr.arch,
            ctx.attr.os,
            ctx.label,
            toolchain_root,
        )

    # TODO: Remove once we drop bazel 7.x support
    if not bazel_features.cc.swift_fragment_removed:
        swiftcopts = list(ctx.fragments.swift.copts())
    else:
        swiftcopts = []

    if "-exec-" in ctx.bin_dir.path:
        swiftcopts.extend(ctx.attr._exec_copts[BuildSettingInfo].value)
    else:
        swiftcopts.extend(ctx.attr._copts[BuildSettingInfo].value)

    # Combine build mode features, autoconfigured features, and required
    # features.
    requested_features = (
        features_for_build_modes(ctx) +
        features_from_swiftcopts(swiftcopts = swiftcopts)
    )
    requested_features.extend(default_features_for_toolchain(
        ctx = ctx,
        target_triple = target_triple,
    ))

    requested_features.extend([
        # Allow users to start using access levels on `import`s by default. Note
        # that this does *not* change the default access level for `import`s to
        # `internal`; that is controlled by the upcoming feature flag
        # `InternalImportsByDefault`.
        "swift.experimental.AccessLevelOnImport",
    ])

    requested_features.extend(ctx.features)

    # Swift.org toolchains assume everything is just available on the PATH so we
    # we don't pass any files unless we have a custom driver executable in the
    # workspace.
    swift_executable = get_swift_executable_for_toolchain(ctx)

    all_tool_configs = _all_tool_configs(
        env = ctx.attr.env,
        swift_executable = swift_executable,
        toolchain_root = toolchain_root,
        use_autolink_extract = SWIFT_FEATURE_USE_AUTOLINK_EXTRACT in ctx.features,
        use_module_wrap = SWIFT_FEATURE_USE_MODULE_WRAP in ctx.features,
        additional_tools = [ctx.file.version_file],
        tool_executable_suffix = ctx.attr.tool_executable_suffix,
    )
    all_action_configs = _all_action_configs(
        os = ctx.attr.os,
        arch = ctx.attr.arch,
        target_triple = target_triple,
        sdkroot = ctx.attr.sdkroot,
        xctest_version = ctx.attr.xctest_version,
        additional_swiftc_copts = swiftcopts,
    )

    if ctx.attr.os == "windows":
        if ctx.attr.arch == "x86_64":
            bindir = "bin64"
        elif ctx.attr.arch == "i686":
            bindir = "bin32"
        elif ctx.attr.arch == "arm64":
            bindir = "bin64a"
        else:
            fail("unsupported arch `{}`".format(ctx.attr.arch))

        xctest = paths.normalize(paths.join(ctx.attr.sdkroot, "..", "..", "Library", "XCTest-{}".format(ctx.attr.xctest_version), "usr", bindir))
        env = dicts.add(
            ctx.attr.env,
            {"Path": xctest + ";" + ctx.attr.env["Path"]},
        )
    else:
        env = ctx.attr.env

    # TODO(allevato): Move some of the remaining hardcoded values, like object
    # format and Obj-C interop support, to attributes so that we can remove the
    # assumptions that are only valid on Linux.
    swift_toolchain_info = SwiftToolchainInfo(
        action_configs = all_action_configs,
        cc_language = None,
        cc_toolchain_info = cc_toolchain,
        clang_implicit_deps_providers = (
            collect_implicit_deps_providers([])
        ),
        cross_import_overlays = [
            target[SwiftCrossImportOverlayInfo]
            for target in ctx.attr.cross_import_overlays
        ],
        debug_outputs_provider = None,
        developer_dirs = [],
        entry_point_linkopts_provider = _entry_point_linkopts_provider,
        feature_allowlists = [
            target[SwiftFeatureAllowlistInfo]
            for target in ctx.attr.feature_allowlists
        ],
        generated_header_module_implicit_deps_providers = (
            collect_implicit_deps_providers([])
        ),
        module_aliases = (
            ctx.attr._module_mapping[SwiftModuleAliasesInfo].aliases
        ),
        implicit_deps_providers = collect_implicit_deps_providers(
            [],
            additional_cc_infos = [swift_linkopts_cc_info],
        ),
        package_configurations = [
            target[SwiftPackageConfigurationInfo]
            for target in ctx.attr.package_configurations
        ],
        requested_features = requested_features,
        root_dir = toolchain_root,
        swift_worker = ctx.attr._worker[DefaultInfo].files_to_run,
        const_protocols_to_gather = ctx.file.const_protocols_to_gather,
        test_configuration = struct(
            env = env,
            execution_requirements = {},
            uses_xctest_bundles = False,
        ),
        tool_configs = all_tool_configs,
        unsupported_features = ctx.disabled_features + [
            SWIFT_FEATURE_MODULE_MAP_HOME_IS_CWD,
            SWIFT_FEATURE_USE_GLOBAL_INDEX_STORE,
        ],
    )

    return [
        platform_common.ToolchainInfo(
            swift_toolchain = swift_toolchain_info,
        ),
        # TODO(b/205018581): Remove this legacy propagation when everything is
        # migrated over to new-style toolchains.
        swift_toolchain_info,
    ]

swift_toolchain = rule(
    attrs = dicts.add(
        swift_toolchain_driver_attrs(),
        {
            "arch": attr.string(
                doc = """\
The name of the architecture that this toolchain targets.

This name should match the name used in the toolchain's directory layout for
architecture-specific content, such as "x86_64" in "lib/swift/linux/x86_64".
""",
                mandatory = True,
            ),
            "cross_import_overlays": attr.label_list(
                allow_empty = True,
                doc = """\
A list of `swift_cross_import_overlay` targets that will be automatically
injected into the dependencies of Swift compilations if their declaring module
and bystanding module are both already declared as dependencies.
""",
                mandatory = False,
                providers = [[SwiftCrossImportOverlayInfo]],
            ),
            "feature_allowlists": attr.label_list(
                doc = """\
A list of `swift_feature_allowlist` targets that allow or prohibit packages from
requesting or disabling features.
""",
                providers = [[SwiftFeatureAllowlistInfo]],
            ),
            "os": attr.string(
                doc = """\
The name of the operating system that this toolchain targets.

This name should match the name used in the toolchain's directory layout for
platform-specific content, such as "linux" in "lib/swift/linux".
""",
                mandatory = True,
            ),
            "package_configurations": attr.label_list(
                doc = """\
A list of `swift_package_configuration` targets that specify additional compiler
configuration options that are applied to targets on a per-package basis.
""",
                providers = [[SwiftPackageConfigurationInfo]],
            ),
            "root": attr.string(
                mandatory = True,
            ),
            "version_file": attr.label(
                mandatory = True,
                allow_single_file = True,
            ),
            "_cc_toolchain": attr.label(
                default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
                doc = """\
The C++ toolchain from which other tools needed by the Swift toolchain (such as
`clang` and `ar`) will be retrieved.
""",
            ),
            "_copts": attr.label(
                default = Label("//swift:copt"),
                doc = """\
The label of the `string_list` containing additional flags that should be passed
to the compiler.
""",
            ),
            "_exec_copts": attr.label(
                default = Label("//swift:exec_copt"),
                doc = """\
The label of the `string_list` containing additional flags that should be passed
to the compiler for exec transition builds.
""",
            ),
            "_module_mapping": attr.label(
                default = Label("//swift:module_mapping"),
                providers = [[SwiftModuleAliasesInfo]],
            ),
            "_worker": attr.label(
                cfg = "exec",
                allow_files = True,
                default = Label("//tools/worker"),
                doc = """\
An executable that wraps Swift compiler invocations and also provides support
for incremental compilation using a persistent mode.
""",
                executable = True,
            ),
            "const_protocols_to_gather": attr.label(
                default = Label(
                    "//swift/toolchains/config:const_protocols_to_gather.json",
                ),
                allow_single_file = True,
                doc = """\
The label of the file specifying a list of protocols for extraction of conformances'
const values.
""",
            ),
            "env": attr.string_dict(
                doc = """\
The preserved environment variables required for the toolchain to operate
normally.
""",
                mandatory = False,
            ),
            "sdkroot": attr.string(
                doc = """\
The root of a SDK to be used for building the target.
""",
                mandatory = False,
            ),
            "tool_executable_suffix": attr.string(
                doc = """\
The suffix to apply to the tools when invoking them.  This is a platform
dependent value (e.g. `.exe` on Window).
""",
                mandatory = False,
            ),
            "xctest_version": attr.string(
                doc = """\
The version of XCTest that the toolchain packages.
""",
                mandatory = False,
            ),
        },
    ),
    doc = "Represents a Swift compiler toolchain.",
    fragments = [] if bazel_features.cc.swift_fragment_removed else ["swift"],
    toolchains = use_cpp_toolchain(),
    implementation = _swift_toolchain_impl,
)
