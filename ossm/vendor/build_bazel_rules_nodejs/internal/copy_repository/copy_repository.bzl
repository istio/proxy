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

"""Custom copy_repository rule used by npm_install and yarn_install.
"""

load("@build_bazel_rules_nodejs//nodejs/private:os_name.bzl", "is_windows_os")

def _copy_file(rctx, src):
    rctx.template(src.basename, src)

def _copy_repository_impl(rctx):
    src_path = "/".join(str(rctx.path(rctx.attr.marker_file)).split("/")[:-1])
    if is_windows_os(rctx):
        _copy_file(rctx, rctx.path(Label("@build_bazel_rules_nodejs//internal/copy_repository:_copy.bat")))
        result = rctx.execute(["cmd.exe", "/C", "_copy.bat", src_path.replace("/", "\\"), "."])
    else:
        _copy_file(rctx, rctx.path(Label("@build_bazel_rules_nodejs//internal/copy_repository:_copy.sh")))
        result = rctx.execute(["./_copy.sh", src_path, "."])
    if result.return_code:
        fail("copy_repository failed: \nSTDOUT:\n%s\nSTDERR:\n%s" % (result.stdout, result.stderr))

copy_repository = repository_rule(
    implementation = _copy_repository_impl,
    attrs = {
        "marker_file": attr.label(allow_single_file = True),
    },
)
