# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Provides the `cel_proto_transitive_descriptor_set` build rule.
"""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")

def _cel_proto_transitive_descriptor_set(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".binarypb")
    transitive_descriptor_sets = depset(transitive = [dep[ProtoInfo].transitive_descriptor_sets for dep in ctx.attr.deps])
    args = ctx.actions.args()
    args.use_param_file(param_file_arg = "%s", use_always = True)
    args.add_all(transitive_descriptor_sets)
    ctx.actions.run_shell(
        outputs = [output],
        inputs = transitive_descriptor_sets,
        progress_message = "Joining descriptors.",
        command = ("< \"$1\" xargs cat >{output}".format(output = output.path)),
        arguments = [args],
    )
    return DefaultInfo(
        files = depset([output]),
        runfiles = ctx.runfiles(files = [output]),
    )

cel_proto_transitive_descriptor_set = rule(
    attrs = {
        "deps": attr.label_list(providers = [[ProtoInfo]]),
    },
    outputs = {
        "out": "%{name}.binarypb",
    },
    implementation = _cel_proto_transitive_descriptor_set,
)
