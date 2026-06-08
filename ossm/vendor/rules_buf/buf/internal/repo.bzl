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

"""Defines buf_dependencies repo rule"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "update_attrs")

_DOC = """
`buf_dependencies` is a [repository rule](https://bazel.build/rules/repository_rules) that downloads one or more modules from the [BSR](https://docs.buf.build/bsr/introduction) and generates build files using Gazelle.
[Setup Gazelle](https://github.com/bazelbuild/bazel-gazelle#setup) to use this rule.

For more info please refer to the [`buf_dependencies` section](https://docs.buf.build/build-systems/bazel#buf-dependencies) of the docs.
"""

def _executable_extension(ctx):
    extension = ""
    if ctx.os.name.startswith("windows"):
        extension = ".exe"
    return extension

def _valid_pin(pin):
    module_commit = pin.split(":")
    if len(module_commit) != 2:
        return False

    module_parts = module_commit[0].split("/")
    if len(module_parts) != 3:
        return False

    return True

def _buf_dependencies_impl(ctx):
    buf = ctx.path(Label("@{}//:buf{}".format(ctx.attr.toolchain_repo, _executable_extension(ctx))))

    for pin in ctx.attr.modules:
        if not _valid_pin(pin):
            fail("failed to parse dep should be in the form of <remote>/<owner>/<repo>:<commit>")

        res = ctx.execute(
            [buf, "export", pin, "--exclude-imports", "--output", ctx.path("")],
            quiet = False,
        )
        if res.return_code != 0:
            fail("failed with code: {}, error: {}".format(res.return_code, res.stderr))

    ctx.file("WORKSPACE", "workspace(name = \"{name}\")".format(name = ctx.name), executable = False)

    # Run gazelle to generate `proto_library` rules.
    # Note that this doesn't require the `buf` extension
    # as all the files are exported to workspace root.
    #
    # Copied from `go_repository` rule
    _gazelle = "@bazel_gazelle_go_repository_tools//:bin/gazelle{}".format(_executable_extension(ctx))
    gazelle = ctx.path(Label(_gazelle))
    cmd = [
        gazelle,
        "-lang",
        "proto",
        "-mode",
        "fix",
        "-repo_root",
        ctx.path(""),
        ctx.path(""),
    ]
    res = ctx.execute(cmd, quiet = False)
    if res.return_code != 0:
        fail("failed with code: {}, error: {}".format(res.return_code, res.stderr))

    return update_attrs(ctx.attr, ["modules"], {})

buf_dependencies = repository_rule(
    implementation = _buf_dependencies_impl,
    doc = _DOC,
    attrs = {
        "modules": attr.string_list(
            allow_empty = False,
            mandatory = True,
            doc = "The module pins <remote>/<owner>/<repo>:<revision>, example: buf.build/acme/petapis:84a33a06f0954823a6f2a089fb1bb82e",
        ),
        "toolchain_repo": attr.string(
            default = "rules_buf_toolchains",
            doc = "The name of the rules_buf_toolchain repo. This is only needed the name of `rules_buf_toolchains` rule was modified.",
        ),
    },
)
