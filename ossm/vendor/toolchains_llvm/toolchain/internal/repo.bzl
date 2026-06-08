# Copyright 2021 The Bazel Authors.
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
load(
    "//toolchain/internal:common.bzl",
    _attr_dict = "attr_dict",
    _os = "os",
    _supported_os_arch_keys = "supported_os_arch_keys",
)
load(
    "//toolchain/internal:llvm_distributions.bzl",
    _download_llvm = "download_llvm",
    _required_llvm_version_rctx = "required_llvm_version_rctx",
)

_target_pairs = ", ".join(_supported_os_arch_keys())

# Atributes common to both `llvm` and `toolchain` repository rules.
common_attrs = {
    "llvm_versions": attr.string_dict(
        mandatory = False,
        doc = ("LLVM version strings, keyed by host OS release name and architecture, " +
               "e.g. darwin-x86_64, darwin-aarch64, ubuntu-20.04-x86_64, etc., or a " +
               "less specific OS and arch pair ({}). ".format(_target_pairs) +
               "An empty key is used to specify a fallback default for all hosts. " +
               "If no `toolchain_roots` is given, then the toolchain will be looked up " +
               "in the list of known llvm_distributions using the provided version. " +
               "If unset, a default value is set from the `llvm_version` attribute."),
    ),
    "extra_llvm_distributions": attr.string_dict(
        mandatory = False,
        doc = ("A dictionary that maps distributions to their SHA256 values. " +
               "It allows for simple additon of llvm distributiosn using the " +
               "'utils/lvm_checksums.sh' tool. This also allows to use the " +
               "distributions lists of future toolchain versions."),
    ),
    "exec_os": attr.string(
        mandatory = False,
        doc = "Execution platform OS, if different from host OS.",
    ),
    "exec_arch": attr.string(
        mandatory = False,
        doc = "Execution platform architecture, if different from host arch.",
    ),
}

llvm_repo_attrs = dict(common_attrs)
llvm_repo_attrs.update({
    "llvm_version": attr.string(
        doc = ("One of the supported versions of LLVM, e.g. 12.0.0; used with the " +
               "`auto` value for the `distribution` attribute, and as a default value " +
               "for the `llvm_versions` attribute. This value can be set to `first` or " +
               "`latest` in order to find the respective first or latest LLVM version " +
               "supported on the OS/arch. Further, requirements can be provided, e.g. " +
               "`latest:>=17.0.4,!=19.0.7`."),
    ),
    "urls": attr.string_list_dict(
        mandatory = False,
        doc = ("URLs to LLVM pre-built binary distribution archives, keyed by host OS " +
               "release name and architecture, e.g. darwin-x86_64, darwin-aarch64, " +
               "ubuntu-20.04-x86_64, etc., or a less specific OS and arch pair " +
               "({}). ".format(_target_pairs) +
               "May also need the `strip_prefix` attribute. " +
               "Consider also setting the `sha256` attribute. An empty key is " +
               "used to specify a fallback default for all hosts. This attribute " +
               "overrides `distribution`, `llvm_version`, `llvm_mirror` and " +
               "`alternative_llvm_sources` attributes if the host OS key is present."),
    ),
    "sha256": attr.string_dict(
        mandatory = False,
        doc = "The expected SHA-256 of the file downloaded as per the `urls` attribute.",
    ),
    "strip_prefix": attr.string_dict(
        mandatory = False,
        doc = "The prefix to strip from the extracted file from the `urls` attribute.",
    ),
    "distribution": attr.string(
        default = "auto",
        doc = ("LLVM pre-built binary distribution filename, must be one " +
               "listed on http://releases.llvm.org/download.html for the version " +
               "specified in the `llvm_version` attribute. A special value of " +
               "'auto' tries to detect the version based on host OS."),
    ),
    "llvm_mirror": attr.string(
        doc = "Base URL for an LLVM release mirror." +
              "\n\n" +
              "This mirror must follow the same structure as the official LLVM release " +
              "sources (`releases.llvm.org` for versions <= 9, `llvm/llvm-project` GitHub " +
              "releases for newer versions)." +
              "\n\n" +
              "If provided, this mirror will be given precedence over the official LLVM release " +
              "sources (see: " +
              "https://github.com/bazel-contrib/toolchains_llvm/toolchain/internal/llvm_distributions.bzl).",
    ),
    "alternative_llvm_sources": attr.string_list(
        doc = "Patterns for alternative LLVM release sources. Unlike URLs specified for `llvm_mirror` " +
              "these do not have to follow the same structure as the official LLVM release sources." +
              "\n\n" +
              "Patterns may include `{llvm_version}` (which will be substituted for the full LLVM " +
              "version, i.e. 13.0.0) and `{basename}` (which will be replaced with the filename " +
              "used by the official LLVM release sources for a particular distribution; i.e. " +
              "`llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz`)." +
              "\n\n" +
              "As with `llvm_mirror`, these sources will take precedence over the official LLVM " +
              "release sources.",
    ),
    "libclang_rt": attr.label_keyed_string_dict(
        mandatory = False,
        doc = ("Additional libclang_rt libraries to overlay into the downloaded LLVM " +
               "distribution. The key is the label of a libclang_rt library, " +
               "and the value is `\"{llvm_target_name}/{library_name}.a\"`."),
    ),
    "netrc": attr.string(
        mandatory = False,
        doc = "Path to the netrc file for authenticated LLVM URL downloads.",
    ),
    "auth_patterns": attr.string_dict(
        mandatory = False,
        doc = "An optional dict mapping host names to custom authorization patterns.",
    ),
})

_compiler_configuration_attrs = {
    "sysroot": attr.string_dict(
        mandatory = False,
        doc = ("System path or fileset, for each target OS and arch pair you want to support " +
               "({}), ".format(_target_pairs) +
               "used to indicate the set of files that form the sysroot for the compiler. " +
               "If the value begins with exactly one forward slash '/', then the value is " +
               "assumed to be a system path. Else, the value will be assumed to be a label " +
               "containing the files and the sysroot path will be taken as the path to the " +
               "package of this label."),
    ),
    "cxx_cross_lib": attr.string_dict(
        mandatory = False,
        doc = ("Cross-compile C++ standard library paths, keyed by target pair " +
               "({}). ".format(_target_pairs) +
               "Value is a label pointing to a repo containing prebuilt C++ standard " +
               "library headers in include/ and libraries in lib/. The include/ directory " +
               "will be added to cxx_builtin_include_directories and lib/ will be added to " +
               "the linker search path via extra_link_flags for the specified target."),
    ),
    "cxx_builtin_include_directories": attr.string_list_dict(
        mandatory = False,
        doc = ("Additional builtin include directories to be added to the default system " +
               "directories, for each target OS and arch pair you want to support " +
               "({}); ".format(_target_pairs) +
               "see documentation for bazel's create_cc_toolchain_config_info."),
    ),
    "stdlib": attr.string_dict(
        mandatory = False,
        doc = ("stdlib implementation, for each target OS and arch pair you want to support " +
               "({}), ".format(_target_pairs) +
               "linked to the compiled binaries. An empty key can be used to specify a " +
               "value for all target pairs. Possible values are `builtin-libc++` (default) " +
               "which uses the libc++ shipped with clang, `libc++` which uses libc++ available on " +
               "the host or sysroot, `stdc++` which uses libstdc++ available on the host or " +
               "sysroot, and `none` which uses `-nostdlib` with the compiler."),
    ),
    "cxx_standard": attr.string_dict(
        mandatory = False,
        doc = ("C++ standard, for each target OS and arch pair you want to support " +
               "({}), ".format(_target_pairs) +
               "passed as `-std` flag to the compiler. An empty key can be used to specify a " +
               "value for all target pairs. Default value is c++17."),
    ),
    # For default values of all the below flags overrides, consult
    # cc_toolchain_config.bzl in this directory.
    "compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "conly_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra flags for compiling C (not C++) files, " +
               "for each target OS and arch pair you want to support " +
               "({}), ".format(", ".join(_supported_os_arch_keys())) + "."),
    ),
    "cxx_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for cxx_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for link_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "archive_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for archive_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "link_libs": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for link_libs, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "fastbuild_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for fastbuild_compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "opt_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for opt_compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "opt_link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for opt_link_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "dbg_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for dbg_compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "coverage_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for coverage_compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "coverage_link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for coverage_link_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    "unfiltered_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Override for unfiltered_compile_flags, replacing the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to override " +
               "({}); empty key overrides all.".format(_target_pairs)),
    ),
    # Same as the above flags, but instead of overriding the defaults, it just adds extras
    "extra_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra compile_flags, added after default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_cxx_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra cxx_flags, added after default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra link_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_archive_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra archive_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_link_libs": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra for link_libs, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_opt_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra opt_compile_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_opt_link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra opt_link_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_dbg_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra dbg_compile_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_coverage_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra coverage_compile_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_coverage_link_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra coverage_link_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "extra_unfiltered_compile_flags": attr.string_list_dict(
        mandatory = False,
        doc = ("Extra unfiltered_compile_flags, added after the default values. " +
               "`{toolchain_path_prefix}` in the flags will be substituted by the path " +
               "to the root LLVM distribution directory. Provide one list for each " +
               "target OS and arch pair you want to add " +
               "({}); an empty key adds all.".format(_target_pairs)),
    ),
    "target_settings": attr.string_list_dict(
        mandatory = False,
        doc = ("Override the toolchain's `target_settings` attribute."),
    ),
    "extra_compiler_files": attr.label(
        mandatory = False,
        doc = ("Files to be made available in the sandbox for compile actions. " +
               "Mostly useful for providing files containing lists of flags, e.g. " +
               "sanitizer ignorelists."),
    ),
    "extra_enabled_features": attr.label_list(
        mandatory = False,
        doc = ("Extra `cc_feature` features to add to this toolchain in an initially " +
               "enabled state. This attribute has limited integration with `cc_feature`, " +
               "and does not run additional correctness checks or handle things like `data` " +
               "files. This is only offered as a migration bridge for projects transitioning " +
               "to rule-based toolchain configurations, or sharing of simple argument sets " +
               "with older toolchains."),
    ),
    "extra_known_features": attr.label_list(
        mandatory = False,
        doc = ("Extra `cc_feature` features to add to this toolchain in an initially " +
               "disabled state. This attribute has limited integration with `cc_feature`, " +
               "and does not run additional correctness checks or handle things like `data` " +
               "files. This is only offered as a migration bridge for projects transitioning " +
               "to rule-based toolchain configurations, or sharing of simple argument sets " +
               "with older toolchains."),
    ),
}

llvm_config_attrs = dict(common_attrs)
llvm_config_attrs.update(_compiler_configuration_attrs)
llvm_config_attrs.update({
    "toolchain_roots": attr.string_dict(
        mandatory = False,
        # TODO: Ideally, we should be taking a filegroup label here instead of a package path, but
        # we ultimately need to subset the files to be more selective in what we include in the
        # sandbox for which operations, and it is not straightforward to subset a filegroup.
        doc = ("System or package path, keyed by host OS release name and architecture, e.g. " +
               "darwin-x86_64, darwin-aarch64, ubuntu-20.04-x86_64, etc., or a less specific " +
               "OS and arch pair ({}), to be used as the LLVM toolchain ".format(_target_pairs) +
               "distributions. An empty key can be used to specify a fallback default for " +
               "all hosts, e.g. with the llvm_toolchain_repo rule. " +
               "If the value begins with exactly one forward slash '/', then the value is " +
               "assumed to be a system path and the toolchain is configured to use absolute " +
               "paths. Else, the value will be assumed to be a bazel package containing the " +
               "filegroup targets as in BUILD.llvm_repo."),
    ),
    "absolute_paths": attr.bool(
        default = False,
        doc = "Use absolute paths in the toolchain. Avoids sandbox overhead.",
    ),
    "extra_exec_compatible_with": attr.string_list_dict(
        mandatory = False,
        doc = "Extra constraints to be added to exec_compatible_with for each target",
    ),
    "extra_target_compatible_with": attr.string_list_dict(
        mandatory = False,
        doc = "Extra constraints to be added to target_compatible_with for each target",
    ),
    "_cc_toolchain_config_bzl": attr.label(
        default = "//toolchain:cc_toolchain_config.bzl",
    ),
    "_toolchains_bzl_tpl": attr.label(
        default = "//toolchain:toolchains.bzl.tpl",
    ),
    "_build_toolchain_tpl": attr.label(
        default = "//toolchain:BUILD.toolchain.tpl",
    ),
    "_darwin_cc_wrapper_sh_tpl": attr.label(
        default = "//toolchain:osx_cc_wrapper.sh.tpl",
    ),
    "_cc_wrapper_sh_tpl": attr.label(
        default = "//toolchain:cc_wrapper.sh.tpl",
    ),
})

def llvm_repo_impl(rctx):
    os = _os(rctx)
    if os == "windows":
        rctx.file("BUILD.bazel", executable = False)
        return None

    llvm_version = _required_llvm_version_rctx(rctx)
    major_llvm_version = int(llvm_version.split(".")[0])

    rctx.file(
        "BUILD.bazel",
        content = rctx.read(Label("//toolchain:BUILD.llvm_repo.tpl")).format(
            # The versioning changed.
            LLVM_VERSION = major_llvm_version if major_llvm_version >= 16 else llvm_version,
        ),
        executable = False,
    )

    updated_attrs = _download_llvm(rctx)

    # We try to avoid patches to the downloaded repo so that it is easier for
    # users to bring their own LLVM distribution through `http_archive`. If we
    # do want to make changes, then we should do it through a patch file, and
    # document it for users of toolchain_roots attribute.

    if hasattr(rctx, "repo_metadata"):
        if updated_attrs == _attr_dict(rctx.attr):
            return rctx.repo_metadata(reproducible = True)
        else:
            return rctx.repo_metadata(attrs_for_reproducibility = updated_attrs)
    else:
        return updated_attrs
