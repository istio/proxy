# Copyright 2021-2025 Buf Technologies, Inc.
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

"""Defines buf_breaking_test rule"""

load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(":plugin.bzl", "protoc_plugin_test")
load("@rules_proto//proto:proto_common.bzl", proto_toolchains = "toolchains")

_PROTO_TOOLCHAIN_TYPE = "@rules_proto//proto:toolchain_type"

_DOC = """
`buf_breaking_test` is a test rule that checks one or more `proto_library` targets for breaking changes.

For more info please refer to the [`buf_breaking_test` section](https://docs.buf.build/build-systems/bazel#buf-lint-test) of the docs.
"""

_TOOLCHAIN = str(Label("//tools/protoc-gen-buf-breaking:toolchain_type"))

def _buf_breaking_test_impl(ctx):
    proto_infos = [t[ProtoInfo] for t in ctx.attr.targets]
    config_map = {
        "against_input": ctx.file.against.short_path,
        "limit_to_input_files": ctx.attr.limit_to_input_files,
        "exclude_imports": ctx.attr.exclude_imports,
        "input_config": ctx.file.config.short_path,
        "error_format": ctx.attr.error_format,
    }
    if ctx.attr.module != "":
        config_map["module"] = ctx.attr.module
    config = json.encode(config_map)
    files_to_include = [ctx.file.against]
    if ctx.file.config != None:
        files_to_include.append(ctx.file.config)
    proto_toolchain_enabled = len(proto_toolchains.use_toolchain(_PROTO_TOOLCHAIN_TYPE)) > 0
    return protoc_plugin_test(
        ctx,
        proto_infos,
        ctx.toolchains[_PROTO_TOOLCHAIN_TYPE].proto.proto_compiler.executable if proto_toolchain_enabled else ctx.executable._protoc,
        ctx.toolchains[_TOOLCHAIN].cli,
        config,
        files_to_include,
        ctx.attr.protoc_args,
    )

buf_breaking_test = rule(
    implementation = _buf_breaking_test_impl,
    doc = _DOC,
    attrs = dict(
        {
            "_windows_constraint": attr.label(
                default = "@platforms//os:windows",
            ),
            "targets": attr.label_list(
                providers = [ProtoInfo],
                doc = """`proto_library` targets to check for breaking changes""",
            ),
            "against": attr.label(
                mandatory = True,
                allow_single_file = True,
                doc = """The image file against which breaking changes are checked.""",
            ),
            "config": attr.label(
                allow_single_file = True,
                doc = """The `buf.yaml` file""",
            ),
            "module": attr.string(
                default = "",
                doc = "The module to use in v2 config",
            ),
            "limit_to_input_files": attr.bool(
                default = False,
                doc = """Checks are limited to input files. If a file gets deleted that will not be caught. Please refer to https://docs.buf.build/breaking/protoc-plugin for more details""",
            ),
            "exclude_imports": attr.bool(
                default = True,
                doc = """Checks are limited to the source files excluding imports from breaking change detection. Please refer to https://docs.buf.build/breaking/protoc-plugin for more details""",
            ),
            "error_format": attr.string(
                default = "",
                doc = "error-format flag for buf breaking: https://buf.build/docs/reference/cli/buf/breaking#error-format",
            ),
            "protoc_args": attr.string_list(
                default = [],
                doc = "Additional arguments to pass to protoc",
            ),
        },
        **proto_toolchains.if_legacy_toolchain({
            "_protoc": attr.label(default = "@com_google_protobuf//:protoc", executable = True, cfg = "exec"),
        })
    ),
    toolchains = [_TOOLCHAIN] + proto_toolchains.use_toolchain(_PROTO_TOOLCHAIN_TYPE),
    test = True,
)
