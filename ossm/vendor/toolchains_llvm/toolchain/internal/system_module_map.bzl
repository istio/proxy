# Copyright 2024 The Bazel Authors.
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

load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:paths.bzl", "paths")

def _textual_header(file, *, include_prefixes, execroot_prefix):
    path = file.path
    for include_prefix in include_prefixes:
        if path.startswith(include_prefix):
            return "  textual header \"{}{}\"".format(execroot_prefix, path)

    # The file is not under any of the include prefixes,
    return None

def _umbrella_submodule(path):
    return """
  module "{path}" {{
    umbrella "{path}"
  }}""".format(path = path)

def _system_module_map(ctx):
    module_map = ctx.actions.declare_file(ctx.attr.name + ".modulemap")

    absolute_path_dirs = []
    relative_include_prefixes = []
    for include_dir in ctx.attr.cxx_builtin_include_directories:
        if ctx.attr.sysroot_path and include_dir.startswith("%sysroot%"):
            include_dir = ctx.attr.sysroot_path + include_dir[len("%sysroot%"):]
        if include_dir.startswith("%workspace%/"):
            include_dir = include_dir.removeprefix("%workspace%/")
        include_dir = paths.normalize(include_dir).replace("//", "/")
        if include_dir.startswith("/"):
            absolute_path_dirs.append(include_dir)
        else:
            relative_include_prefixes.append(include_dir + "/")

    # The builtin include directories are relative to the execroot, but the
    # paths in the module map must be relative to the directory that contains
    # the module map.
    execroot_prefix = (module_map.dirname.count("/") + 1) * "../"
    textual_header_closure = lambda file: _textual_header(
        file,
        include_prefixes = relative_include_prefixes,
        execroot_prefix = execroot_prefix,
    )

    umbrella_submodule_closure = lambda file: _umbrella_submodule(
        execroot_prefix + paths.normalize(file.path).replace("//", "/"),
    )

    template_dict = ctx.actions.template_dict()

    if bazel_features.rules.merkle_cache_v2:
        # If provided, cxx_builtin_files should be a filegroup with 2 source directory entries:
        #  - include/c++
        #  - lib/clang/<VERSION>/include
        cxx_builtin_include_files_closure = umbrella_submodule_closure
    else:
        cxx_builtin_include_files_closure = textual_header_closure

    template_dict.add_joined(
        "%cxx_builtin_include_files%",
        ctx.attr.cxx_builtin_include_files[DefaultInfo].files,
        join_with = "\n",
        map_each = cxx_builtin_include_files_closure,
        allow_closure = True,
    )

    # We don't have a good way to detect a source directory, so check if it's a single File...
    sysroot_files = ctx.attr.sysroot_files[DefaultInfo].files.to_list()
    if len(sysroot_files) == 1:
        template_dict.add("%sysroot%", umbrella_submodule_closure(sysroot_files[0]))
    else:
        if sysroot_files and bazel_features.rules.merkle_cache_v2:
            # buildifier: disable=print
            print("WARNING: Sysroot {} resolved to {} files. Consider using the `sysroot` repository rule in @toolchains_llvm//toolchain:sysroot.bzl which provides a single-file (directory) sysroot for more efficient builds.".format(
                ctx.attr.sysroot_files.label,
                len(sysroot_files),
            ))
        template_dict.add_joined(
            "%sysroot%",
            ctx.attr.sysroot_files[DefaultInfo].files,
            join_with = "\n",
            map_each = textual_header_closure,
            allow_closure = True,
        )

    template_dict.add_joined(
        "%umbrella_submodules%",
        depset(absolute_path_dirs),
        join_with = "\n",
        map_each = _umbrella_submodule,
    )

    ctx.actions.expand_template(
        template = ctx.file._module_map_template,
        output = module_map,
        computed_substitutions = template_dict,
    )
    return DefaultInfo(files = depset([module_map]))

system_module_map = rule(
    doc = """Generates a Clang module map for the toolchain and sysroot headers.

    Files under the configured built-in include directories that are managed by
    Bazel are included as textual headers. All directories referenced by
    absolute paths are included as umbrella submodules.""",
    implementation = _system_module_map,
    attrs = {
        "cxx_builtin_include_files": attr.label(mandatory = True),
        "cxx_builtin_include_directories": attr.string_list(mandatory = True),
        "sysroot_files": attr.label(),
        "sysroot_path": attr.string(),
        "_module_map_template": attr.label(
            default = "template.modulemap",
            allow_single_file = True,
        ),
    },
)
