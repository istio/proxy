load("//cc/toolchains/impl:variables.bzl", "cc_builtin_variables", "cc_variable", "types")

package(default_visibility = ["//visibility:public"])

cc_variable(
    name = "cs_fdo_instrument_path",
    actions = [
        "//cc/toolchains/actions:link_actions",
        "//cc/toolchains/actions:compile_actions",
    ],
    type = types.directory,
)

cc_variable(
    name = "def_file_path",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "dependency_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "dependent_module_map_files",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.file)),
)

cc_variable(
    name = "external_include_paths",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.directory)),
)

cc_variable(
    name = "fdo_instrument_path",
    actions = [
        "//cc/toolchains/actions:link_actions",
        "//cc/toolchains/actions:compile_actions",
    ],
    type = types.directory,
)

cc_variable(
    name = "fdo_prefetch_hints_path",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "fdo_profile_path",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "force_pic",
    actions = ["//cc/toolchains/actions:link_actions"],
    # Provided when --force-pic is passed
    type = types.option(types.void),
)

cc_variable(
    name = "framework_include_paths",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "gcov_gcno_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "generate_interface_library",
    actions = ["//cc/toolchains/actions:link_actions"],
    # "yes" or "no"
    type = types.option(types.string),
)

cc_variable(
    name = "include",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.file),
)

cc_variable(
    name = "include_paths",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "includes",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.file)),
)

cc_variable(
    name = "input_file",
    actions = ["//cc/toolchains/actions:strip"],
    type = types.file,
)

cc_variable(
    name = "interface_library_builder_path",
    actions = ["//cc/toolchains/actions:link_actions"],
    # Should be a file, but contains the string "ignored" when there's no value.
    type = types.option(types.string),
)

cc_variable(
    name = "interface_library_input_path",
    actions = ["//cc/toolchains/actions:link_actions"],
    # Should be a file, but contains the string "ignored" when there's no value.
    type = types.option(types.string),
)

cc_variable(
    name = "interface_library_output_path",
    actions = ["//cc/toolchains/actions:link_actions"],
    # Should be a file, but contains the string "ignored" when there's no value.
    type = types.option(types.string),
)

cc_variable(
    name = "is_cc_test",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.bool),
)

cc_variable(
    name = "is_using_fission",
    actions = [
        "//cc/toolchains/actions:link_actions",
        "//cc/toolchains/actions:compile_actions",
    ],
    type = types.option(types.void),
)

cc_variable(
    name = "legacy_compile_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.string)),
)

cc_variable(
    name = "legacy_link_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.list(types.string),
)

cc_variable(
    name = "libraries_to_link",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    type = types.option(types.list(types.struct(
        name = types.string,
        is_whole_archive = types.bool,
        object_files = types.list(types.file),
        path = types.string,
        shared_libraries = types.list(types.struct(
            name = types.string,
            is_whole_archive = types.bool,
            object_files = types.list(types.file),
            path = types.file,
            type = types.string,
        )),
        type = types.string,
    ))),
)

cc_variable(
    name = "libraries_to_link.type",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    # See :libraries_to_link.
    type = types.string,
)

cc_variable(
    name = "libraries_to_link.name",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    # See :libraries_to_link.
    type = types.string,
)

cc_variable(
    name = "libraries_to_link.path",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    # See :libraries_to_link.
    type = types.string,
)

cc_variable(
    name = "libraries_to_link.object_files",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    # See :libraries_to_link.
    type = types.list(types.file),
)

cc_variable(
    name = "libraries_to_link.is_whole_archive",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.bool,
)

cc_variable(
    name = "libraries_to_link.shared_libraries",
    actions = ["//cc/toolchains/actions:link_actions"],
    # See :libraries_to_link.
    type = types.unknown,
)

cc_variable(
    name = "libraries_to_link.shared_libraries.is_whole_archive",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.string,
)

cc_variable(
    name = "libraries_to_link.shared_libraries.name",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.string,
)

cc_variable(
    name = "libraries_to_link.shared_libraries.object_files",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.list(types.file),
)

cc_variable(
    name = "libraries_to_link.shared_libraries.path",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.file,
)

cc_variable(
    name = "libraries_to_link.shared_libraries.type",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.string,
)

cc_variable(
    name = "library_search_directories",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "linker_param_file",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    type = types.option(types.file),
)

cc_variable(
    name = "linkstamp_paths",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "lto_indexing_bitcode_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "module_files",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.file)),
)

cc_variable(
    name = "module_map_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "module_name",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "no_objc_arc",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "objc_arc",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "output_assembly_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "output_execpath",
    actions = [
        "//cc/toolchains/actions:cpp_link_static_library",
        "//cc/toolchains/actions:link_actions",
    ],
    type = types.option(types.directory),
)

cc_variable(
    name = "output_file",
    actions = [
        "//cc/toolchains/actions:compile_actions",
        "//cc/toolchains/actions:strip",
    ],
    type = types.option(types.file),
)

cc_variable(
    name = "output_preprocess_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "per_object_debug_info_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "pic",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.void),
)

cc_variable(
    name = "preprocessor_defines",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.string),
)

cc_variable(
    name = "propellor_optimize_ld_path",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "quote_include_paths",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "runtime_library_search_directories",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.list(types.directory)),
)

cc_variable(
    name = "runtime_solib_name",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "serialized_diagnostics_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "source_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.file,
)

cc_variable(
    name = "strip_debug_symbols",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.void),
)

cc_variable(
    name = "stripopts",
    actions = ["//cc/toolchains/actions:strip"],
    type = types.list(types.string),
)

# Instead of the "sysroot" variable, use the cc_sysroot macro in
# //cc/toolchains/args:sysroot.bzl

cc_variable(
    name = "system_include_paths",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.list(types.directory),
)

cc_variable(
    name = "thinlto_index",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "thinlto_indexing_param_file",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "thinlto_input_bitcode_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "thinlto_merged_object_file",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "thinlto_object_suffix_replace",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "thinlto_output_object_file",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "thinlto_param_file",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.file),
)

cc_variable(
    name = "thinlto_prefix_replace",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.option(types.string),
)

cc_variable(
    name = "unfiltered_compile_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.string)),
)

cc_variable(
    name = "user_compile_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    type = types.option(types.list(types.string)),
)

cc_variable(
    name = "user_link_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    type = types.list(types.string),
)

cc_builtin_variables(
    name = "variables",
    srcs = [
        ":cs_fdo_instrument_path",
        ":def_file_path",
        ":dependency_file",
        ":dependent_module_map_files",
        ":external_include_paths",
        ":fdo_instrument_path",
        ":fdo_prefetch_hints_path",
        ":fdo_profile_path",
        ":force_pic",
        ":framework_include_paths",
        ":gcov_gcno_file",
        ":generate_interface_library",
        ":include",
        ":include_paths",
        ":includes",
        ":input_file",
        ":interface_library_builder_path",
        ":interface_library_input_path",
        ":interface_library_output_path",
        ":is_cc_test",
        ":is_using_fission",
        ":legacy_compile_flags",
        ":legacy_link_flags",
        ":libraries_to_link",
        ":library_search_directories",
        ":linker_param_file",
        ":linkstamp_paths",
        ":lto_indexing_bitcode_file",
        ":module_files",
        ":module_map_file",
        ":module_name",
        ":no_objc_arc",
        ":objc_arc",
        ":output_assembly_file",
        ":output_execpath",
        ":output_file",
        ":output_preprocess_file",
        ":per_object_debug_info_file",
        ":pic",
        ":preprocessor_defines",
        ":propellor_optimize_ld_path",
        ":quote_include_paths",
        ":runtime_library_search_directories",
        ":runtime_solib_name",
        ":serialized_diagnostics_file",
        ":source_file",
        ":strip_debug_symbols",
        ":stripopts",
        ":system_include_paths",
        ":thinlto_index",
        ":thinlto_indexing_param_file",
        ":thinlto_input_bitcode_file",
        ":thinlto_merged_object_file",
        ":thinlto_object_suffix_replace",
        ":thinlto_output_object_file",
        ":thinlto_param_file",
        ":thinlto_prefix_replace",
        ":unfiltered_compile_flags",
        ":user_compile_flags",
        ":user_link_flags",
    ],
    visibility = ["//visibility:public"],
)
