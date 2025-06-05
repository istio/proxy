# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Implementation of the cc_toolchain rule."""

load("//cc/toolchains:cc_toolchain.bzl", _cc_toolchain = "cc_toolchain")
load(
    "//cc/toolchains/impl:toolchain_config.bzl",
    "cc_legacy_file_group",
    "cc_toolchain_config",
)

visibility("public")

# Taken from https://bazel.build/docs/cc-toolchain-config-reference#actions
# TODO: This is best-effort. Update this with the correct file groups once we
#  work out what actions correspond to what file groups.
_LEGACY_FILE_GROUPS = {
    "ar_files": [
        Label("//cc/toolchains/actions:ar_actions"),
    ],
    "as_files": [
        Label("//cc/toolchains/actions:assembly_actions"),
    ],
    "compiler_files": [
        Label("//cc/toolchains/actions:cc_flags_make_variable"),
        Label("//cc/toolchains/actions:c_compile"),
        Label("//cc/toolchains/actions:cpp_compile"),
        Label("//cc/toolchains/actions:cpp_header_parsing"),
    ],
    # There are no actions listed for coverage, dwp, and objcopy in action_names.bzl.
    "coverage_files": [],
    "dwp_files": [],
    "linker_files": [
        Label("//cc/toolchains/actions:cpp_link_dynamic_library"),
        Label("//cc/toolchains/actions:cpp_link_nodeps_dynamic_library"),
        Label("//cc/toolchains/actions:cpp_link_executable"),
    ],
    "objcopy_files": [],
    "strip_files": [
        Label("//cc/toolchains/actions:strip"),
    ],
}

def cc_toolchain(
        *,
        name,
        tool_map = None,
        args = [],
        known_features = [],
        enabled_features = [],
        libc_top = None,
        module_map = None,
        dynamic_runtime_lib = None,
        static_runtime_lib = None,
        supports_header_parsing = False,
        supports_param_files = False,
        compiler = "",
        **kwargs):
    """A C/C++ toolchain configuration.

    This rule is the core declaration of a complete C/C++ toolchain. It collects together
    tool configuration, which arguments to pass to each tool, and how
    [features](https://bazel.build/docs/cc-toolchain-config-reference#features)
    (dynamically-toggleable argument lists) interact.

    A single `cc_toolchain` may support a wide variety of platforms and configurations through
    [configurable build attributes](https://bazel.build/docs/configurable-attributes) and
    [feature relationships](https://bazel.build/docs/cc-toolchain-config-reference#feature-relationships).

    Arguments are applied to commandline invocation of tools in the following order:

    1. Arguments in the order they are listed in listed in [`args`](#cc_toolchain-args).
    2. Any legacy/built-in features that have been implicitly or explicitly enabled.
    3. User-defined features in the order they are listed in
       [`known_features`](#cc_toolchain-known_features).

    When building a `cc_toolchain` configuration, it's important to understand how `select`
    statements will be evaluated:

    * Most attributes and dependencies of a `cc_toolchain` are evaluated under the target platform.
      This means that a `@platforms//os:linux` constraint will be satisfied when
      the final compiled binaries are intended to be ran from a Linux machine. This means that
      a different operating system (e.g. Windows) may be cross-compiling to linux.
    * The `cc_tool_map` rule performs a transition to the exec platform when evaluating tools. This
      means that a if a `@platforms//os:linux` constraint is satisfied in a
      `select` statement on a `cc_tool`, that means the machine that will run the tool is a Linux
      machine. This means that a Linux machine may be cross-compiling to a different OS
      like Windows.

    Generated rules:
        {name}: A `cc_toolchain` for this toolchain.
        _{name}_config: A `cc_toolchain_config` for this toolchain.
        _{name}_*_files: Generated rules that group together files for
            "ar_files", "as_files", "compiler_files", "coverage_files",
            "dwp_files", "linker_files", "objcopy_files", and "strip_files"
            normally enumerated as part of the `cc_toolchain` rule.

    Args:
        name: (str) The name of the label for the toolchain.
        tool_map: (Label) The `cc_tool_map` that specifies the tools to use for various toolchain
            actions.
        args: (List[Label]) A list of `cc_args` and `cc_arg_list` to apply across this toolchain.
        known_features: (List[Label]) A list of `cc_feature` rules that this toolchain supports.
            Whether or not these
            [features](https://bazel.build/docs/cc-toolchain-config-reference#features)
            are enabled may change over the course of a build. See the documentation for
            `cc_feature` for more information.
        enabled_features: (List[Label]) A list of `cc_feature` rules whose initial state should
            be `enabled`. Note that it is still possible for these
            [features](https://bazel.build/docs/cc-toolchain-config-reference#features)
            to be disabled over the course of a build through other mechanisms. See the
            documentation for `cc_feature` for more information.
        libc_top: (Label) A collection of artifacts for libc passed as inputs to compile/linking
            actions. See
            [`cc_toolchain.libc_top`](https://bazel.build/reference/be/c-cpp#cc_toolchain.libc_top)
            for more information.
        module_map: (Label) Module map artifact to be used for modular builds. See
            [`cc_toolchain.module_map`](https://bazel.build/reference/be/c-cpp#cc_toolchain.module_map)
            for more information.
        dynamic_runtime_lib: (Label) Dynamic library to link when the `static_link_cpp_runtimes`
            and `dynamic_linking_mode`
            [features](https://bazel.build/docs/cc-toolchain-config-reference#features) are both
            enabled. See
            [`cc_toolchain.dynamic_runtime_lib`](https://bazel.build/reference/be/c-cpp#cc_toolchain.dynamic_runtime_lib)
            for more information.
        static_runtime_lib: (Label) Static library to link when the `static_link_cpp_runtimes`
            and `static_linking_mode`
            [features](https://bazel.build/docs/cc-toolchain-config-reference#features) are both
            enabled. See
            [`cc_toolchain.dynamic_runtime_lib`](https://bazel.build/reference/be/c-cpp#cc_toolchain.dynamic_runtime_lib)
            for more information.
        supports_header_parsing: (bool) Whether or not this toolchain supports header parsing
            actions. See
            [`cc_toolchain.supports_header_parsing`](https://bazel.build/reference/be/c-cpp#cc_toolchain.supports_header_parsing)
            for more information.
        supports_param_files: (bool) Whether or not this toolchain supports linking via param files.
            See
            [`cc_toolchain.supports_param_files`](https://bazel.build/reference/be/c-cpp#cc_toolchain.supports_param_files)
            for more information.
        compiler: (str) The type of compiler used by this toolchain (e.g. "gcc", "clang"). The current
            toolchain's compiler is exposed to `@rules_cc//cc/private/toolchain:compiler
            (compiler_flag)` as a flag value.
        **kwargs: [common attributes](https://bazel.build/reference/be/common-definitions#common-attributes)
            that should be applied to all rules created by this macro.
    """
    cc_toolchain_visibility = kwargs.pop("visibility", default = None)

    for group in _LEGACY_FILE_GROUPS:
        if group in kwargs:
            fail("Don't use legacy file groups such as %s. Instead, associate files with `cc_tool` or `cc_args` rules." % group)

    config_name = "_{}_config".format(name)
    cc_toolchain_config(
        name = config_name,
        tool_map = tool_map,
        args = args,
        known_features = known_features,
        enabled_features = enabled_features,
        compiler = compiler,
        visibility = ["//visibility:private"],
        **kwargs
    )

    # Provides ar_files, compiler_files, linker_files, ...
    legacy_file_groups = {}
    for group, actions in _LEGACY_FILE_GROUPS.items():
        group_name = "_{}_{}".format(name, group)
        cc_legacy_file_group(
            name = group_name,
            config = config_name,
            actions = actions,
            visibility = ["//visibility:private"],
            **kwargs
        )
        legacy_file_groups[group] = group_name

    _cc_toolchain(
        name = name,
        toolchain_config = config_name,
        all_files = config_name,
        dynamic_runtime_lib = dynamic_runtime_lib,
        libc_top = libc_top,
        module_map = module_map,
        static_runtime_lib = static_runtime_lib,
        supports_header_parsing = supports_header_parsing,
        supports_param_files = supports_param_files,
        # This is required for Bazel versions <= 7.x.x. It is ignored in later versions.
        exec_transition_for_inputs = False,
        visibility = cc_toolchain_visibility,
        **(kwargs | legacy_file_groups)
    )
