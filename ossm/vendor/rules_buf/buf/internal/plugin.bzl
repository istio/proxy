# Copyright 2021-2023 Buf Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""protoc plugin based test rule"""

def protoc_plugin_test(ctx, proto_infos, protoc, plugin, config, files_to_include = [], protoc_args = []):
    """protoc_plugin_test creates a script file for a generic protoc plugin

    Args:
        ctx: rule context
        proto_infos: The ProtoInfo providers of `proto_library`
        protoc: protoc executable
        plugin: plugin executable
        config: plugin option to be passed to protoc
        files_to_include: any additional files to be included as part of runfiles
        protoc_args: extra arguments to be passed to protoc
    Returns:
        Runfiles required to run the test
    """
    deps = depset(
        [pi.direct_descriptor_set for pi in proto_infos],
        transitive = [pi.transitive_descriptor_sets for pi in proto_infos],
    )

    sources = []
    source_files = []

    for pi in proto_infos:
        for f in pi.direct_sources:
            source_files.append(f)

            # source is the argument passed to protoc. This is the import path "foo/foo.proto"
            # We have to trim the prefix if strip_import_prefix attr is used in proto_library.
            sources.append(
                f.path[len(pi.proto_source_root) + 1:] if f.path.startswith(pi.proto_source_root) else f.path,
            )

    args = ctx.actions.args()
    args = args.set_param_file_format("multiline")
    args.add_joined(["--plugin", "protoc-gen-buf-plugin", plugin.short_path], join_with = "=")
    args.add_joined(["--buf-plugin_opt", config], join_with = "=")
    args.add_joined("--descriptor_set_in", deps, join_with = ":", map_each = _short_path)
    args.add_joined(["--buf-plugin_out", "."], join_with = "=")
    args.add_all(protoc_args)
    args.add_all(sources)

    args_file = ctx.actions.declare_file("{}-args".format(ctx.label.name))
    ctx.actions.write(
        output = args_file,
        content = args,
        is_executable = True,
    )

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = "{} @{}".format(protoc.short_path, args_file.short_path),
        is_executable = True,
    )

    files = [protoc, plugin, args_file] + source_files + files_to_include
    runfiles = ctx.runfiles(
        files = files,
        transitive_files = deps,
    )

    return [
        DefaultInfo(
            runfiles = runfiles,
        ),
    ]

def _short_path(file, dir_exp):
    return file.short_path
