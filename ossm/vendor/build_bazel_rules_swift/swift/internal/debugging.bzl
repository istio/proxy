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

"""Functions relating to debugging support during compilation and linking."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load(":action_names.bzl", "SWIFT_ACTION_MODULEWRAP")
load(
    ":actions.bzl",
    "is_action_enabled",
    "run_toolchain_action",
)
load(
    ":feature_names.bzl",
    "SWIFT_FEATURE_DBG",
    "SWIFT_FEATURE_FASTBUILD",
    "SWIFT_FEATURE_NO_EMBED_DEBUG_MODULE",
)
load(":features.bzl", "is_feature_enabled")

def ensure_swiftmodule_is_embedded(
        actions,
        feature_configuration,
        label,
        swiftmodule,
        swift_toolchain):
    """Ensures that a `.swiftmodule` file is embedded in a library or binary.

    This function handles the distinctions between how different object file
    formats (i.e., Mach-O vs. ELF) have to embed the module AST for debugging
    purposes.

    Args:
        actions: The object used to register actions.
        feature_configuration: The Swift feature configuration.
        label: The `Label` of the target being built.
        swiftmodule: The `.swiftmodule` file to be wrapped.
        swift_toolchain: The `SwiftToolchainInfo` provider of the toolchain.

    Returns:
        A `LinkerInput` containing any flags and/or input files that should be
        propagated to the linker to embed the `.swiftmodule` as debugging
        information in the binary.
    """
    if is_action_enabled(
        action_name = SWIFT_ACTION_MODULEWRAP,
        swift_toolchain = swift_toolchain,
    ):
        # For ELF-format binaries, we need to invoke a Swift modulewrap action
        # to wrap the .swiftmodule file in a .o file that gets propagated to the
        # linker.
        modulewrap_obj = actions.declare_file(
            paths.replace_extension(swiftmodule.basename, ".modulewrap.o"),
        )
        prerequisites = struct(
            object_file = modulewrap_obj,
            swiftmodule_file = swiftmodule,
            target_label = feature_configuration._label,
        )
        run_toolchain_action(
            actions = actions,
            action_name = SWIFT_ACTION_MODULEWRAP,
            feature_configuration = feature_configuration,
            outputs = [modulewrap_obj],
            prerequisites = prerequisites,
            progress_message = (
                "Wrapping {} for debugging".format(swiftmodule.short_path)
            ),
            swift_toolchain = swift_toolchain,
        )

        # Passing the `.o` file directly to the linker ensures that it links to
        # the binary even if nothing else references it.
        return cc_common.create_linker_input(
            additional_inputs = depset([modulewrap_obj]),
            owner = label,
            user_link_flags = depset([modulewrap_obj.path]),
        )

    # If module-wrapping is not enabled for the toolchain, assume that we can
    # use the `-add_ast_path` linker flag.
    return cc_common.create_linker_input(
        owner = label,
        user_link_flags = depset([
            "-Wl,-add_ast_path,{}".format(swiftmodule.path),
        ]),
        additional_inputs = depset([swiftmodule]),
    )

def should_embed_swiftmodule_for_debugging(
        feature_configuration,
        module_context):
    """Returns True if the configuration wants modules embedded for debugging.

    Args:
        feature_configuration: The Swift feature configuration.
        module_context: The module context returned by `compile`.

    Returns:
        True if the `.swiftmodule` should be embedded by the linker for
        debugging.
    """
    return (
        module_context.swift and
        module_context.swift.swiftmodule and
        _is_debugging(feature_configuration = feature_configuration) and
        not is_feature_enabled(
            feature_configuration = feature_configuration,
            feature_name = SWIFT_FEATURE_NO_EMBED_DEBUG_MODULE,
        )
    )

def _is_debugging(feature_configuration):
    """Returns `True` if the current compilation mode produces debug info.

    We replicate the behavior of the C++ build rules for Swift, which are
    described here:
    https://docs.bazel.build/versions/master/user-manual.html#flag--compilation_mode

    Args:
        feature_configuration: The feature configuration.

    Returns:
        `True` if the current compilation mode produces debug info.
    """
    return (
        is_feature_enabled(
            feature_configuration = feature_configuration,
            feature_name = SWIFT_FEATURE_DBG,
        ) or is_feature_enabled(
            feature_configuration = feature_configuration,
            feature_name = SWIFT_FEATURE_FASTBUILD,
        )
    )
