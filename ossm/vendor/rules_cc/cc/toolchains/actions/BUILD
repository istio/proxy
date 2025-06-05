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
load("//cc:action_names.bzl", "ACTION_NAMES")
load("//cc/toolchains:actions.bzl", "cc_action_type", "cc_action_type_set")

package(default_visibility = ["//visibility:public"])

# Keep in sync with //cc:action_names.bzl.

cc_action_type(
    name = "c_compile",
    action_name = ACTION_NAMES.c_compile,
)

cc_action_type(
    name = "cpp_compile",
    action_name = ACTION_NAMES.cpp_compile,
)

cc_action_type(
    name = "linkstamp_compile",
    action_name = ACTION_NAMES.linkstamp_compile,
)

cc_action_type(
    name = "cc_flags_make_variable",
    action_name = ACTION_NAMES.cc_flags_make_variable,
)

cc_action_type(
    name = "cpp_module_codegen",
    action_name = ACTION_NAMES.cpp_module_codegen,
)

cc_action_type(
    name = "cpp_header_analysis",
    action_name = "c++-header-analysis",
)

cc_action_type(
    name = "cpp_header_parsing",
    action_name = ACTION_NAMES.cpp_header_parsing,
)

cc_action_type(
    name = "cpp_module_deps_scanning",
    action_name = ACTION_NAMES.cpp_module_deps_scanning,
)

cc_action_type(
    name = "cpp20_module_compile",
    action_name = ACTION_NAMES.cpp20_module_compile,
)

cc_action_type(
    name = "cpp20_module_codegen",
    action_name = ACTION_NAMES.cpp20_module_codegen,
)

cc_action_type(
    name = "cpp_module_compile",
    action_name = ACTION_NAMES.cpp_module_compile,
)

cc_action_type(
    name = "assemble",
    action_name = ACTION_NAMES.assemble,
)

cc_action_type(
    name = "preprocess_assemble",
    action_name = ACTION_NAMES.preprocess_assemble,
)

cc_action_type(
    name = "llvm_cov",
    action_name = ACTION_NAMES.llvm_cov,
)

cc_action_type(
    name = "lto_indexing",
    action_name = ACTION_NAMES.lto_indexing,
)

cc_action_type(
    name = "lto_backend",
    action_name = ACTION_NAMES.lto_backend,
)

cc_action_type(
    name = "lto_index_for_executable",
    action_name = ACTION_NAMES.lto_index_for_executable,
)

cc_action_type(
    name = "lto_index_for_dynamic_library",
    action_name = ACTION_NAMES.lto_index_for_dynamic_library,
)

cc_action_type(
    name = "lto_index_for_nodeps_dynamic_library",
    action_name = ACTION_NAMES.lto_index_for_nodeps_dynamic_library,
)

cc_action_type(
    name = "cpp_link_executable",
    action_name = ACTION_NAMES.cpp_link_executable,
)

cc_action_type(
    name = "cpp_link_dynamic_library",
    action_name = ACTION_NAMES.cpp_link_dynamic_library,
)

cc_action_type(
    name = "cpp_link_nodeps_dynamic_library",
    action_name = ACTION_NAMES.cpp_link_nodeps_dynamic_library,
)

cc_action_type(
    name = "cpp_link_static_library",
    action_name = ACTION_NAMES.cpp_link_static_library,
)

cc_action_type(
    name = "strip",
    action_name = ACTION_NAMES.strip,
)

cc_action_type(
    name = "objcopy_embed_data",
    action_name = ACTION_NAMES.objcopy_embed_data,
)

cc_action_type(
    name = "validate_static_library",
    action_name = ACTION_NAMES.validate_static_library,
)

# ld_embed_data is only available within google.
cc_action_type(
    # # copybara-comment-this-out-please
    name = "ld_embed_data_action",  # # copybara-comment-this-out-please
    action_name = "ld_embed_data",  # # copybara-comment-this-out-please
)  # # copybara-comment-this-out-please

# To make things simple, both internal and external rules will refer to
# ld_embed_data, but externally it will evaluate to the empty set.
cc_action_type_set(
    name = "ld_embed_data",
    actions = [
        ":ld_embed_data_action",  # # copybara-comment-this-out-please
    ],
    allow_empty = True,
    visibility = ["//cc/toolchains:__subpackages__"],
)

cc_action_type(
    name = "objc_compile",
    action_name = ACTION_NAMES.objc_compile,
)

cc_action_type(
    name = "objc_executable",
    action_name = ACTION_NAMES.objc_executable,
)

cc_action_type(
    name = "objc_fully_link",
    action_name = ACTION_NAMES.objc_fully_link,
)

cc_action_type(
    name = "objcpp_compile",
    action_name = ACTION_NAMES.objcpp_compile,
)

cc_action_type(
    name = "objcpp_executable",
    action_name = "objc++-executable",
)

cc_action_type(
    name = "clif_match",
    action_name = ACTION_NAMES.clif_match,
)

cc_action_type_set(
    name = "ar_actions",
    actions = [":cpp_link_static_library"],
)

cc_action_type_set(
    name = "assembly_actions",
    actions = [
        ":preprocess_assemble",
        ":assemble",
    ],
)

cc_action_type_set(
    name = "c_compile_actions",
    actions = [
        ":c_compile",
    ],
)

cc_action_type_set(
    name = "cpp_compile_actions",
    actions = [
        ":linkstamp_compile",
        ":cpp_compile",
        ":cpp_header_parsing",
        ":cpp_module_compile",
        ":cpp_module_codegen",
        ":lto_backend",
        ":clif_match",
    ],
)

cc_action_type_set(
    name = "compile_actions",
    actions = [
        ":cpp_compile_actions",
        ":c_compile_actions",
        ":assembly_actions",
        ":objc_compile",
        ":objcpp_compile",
    ],
)

cc_action_type_set(
    name = "link_actions",
    actions = [
        ":link_executable_actions",
        ":dynamic_library_link_actions",
    ],
)

cc_action_type_set(
    name = "link_executable_actions",
    actions = [
        ":cpp_link_executable",
        ":lto_index_for_executable",
    ],
)

cc_action_type_set(
    name = "dynamic_library_link_actions",
    actions = [
        ":cpp_link_dynamic_library",
        ":lto_index_for_dynamic_library",
        ":nodeps_dynamic_library_link_actions",
    ],
)

cc_action_type_set(
    name = "nodeps_dynamic_library_link_actions",
    actions = [
        ":cpp_link_nodeps_dynamic_library",
        ":lto_index_for_nodeps_dynamic_library",
    ],
)

cc_action_type_set(
    name = "transitive_link_actions",
    actions = [
        ":cpp_link_executable",
        ":cpp_link_dynamic_library",
        ":lto_index_for_executable",
        ":lto_index_for_dynamic_library",
    ],
)

cc_action_type_set(
    name = "all_actions",
    actions = [
        ":c_compile",
        ":cpp_compile",
        ":linkstamp_compile",
        ":cc_flags_make_variable",
        ":cpp_module_codegen",
        ":cpp_header_analysis",
        ":cpp_header_parsing",
        ":cpp_module_deps_scanning",
        ":cpp20_module_compile",
        ":cpp20_module_codegen",
        ":cpp_module_compile",
        ":assemble",
        ":preprocess_assemble",
        ":llvm_cov",
        ":lto_indexing",
        ":lto_backend",
        ":lto_index_for_executable",
        ":lto_index_for_dynamic_library",
        ":lto_index_for_nodeps_dynamic_library",
        ":cpp_link_executable",
        ":cpp_link_dynamic_library",
        ":cpp_link_nodeps_dynamic_library",
        ":cpp_link_static_library",
        ":strip",
        ":objcopy_embed_data",
        ":ld_embed_data",
        ":objc_compile",
        ":objc_executable",
        ":objc_fully_link",
        ":objcpp_compile",
        ":objcpp_executable",
        ":clif_match",
        ":validate_static_library",
    ],
)
