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

_TOOLCHAIN = "@rules_buf//tools/buf:toolchain_type"

def _print_buf_version_impl(ctx):
    buf = ctx.toolchains[_TOOLCHAIN].cli

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = "{} --version".format(buf.short_path),
        is_executable = True,
    )

    files = [buf]
    runfiles = ctx.runfiles(
        files = files,
    )

    return [
        DefaultInfo(
            runfiles = runfiles,
        ),
    ]

print_buf_version = rule(
    implementation = _print_buf_version_impl,
    toolchains = [_TOOLCHAIN],
    executable = True,
)
