# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Helper functions to get node and npm labels in @nodejs_host and @nodejs_<platform> repositories.

Labels are different on windows and linux/OSX.
"""

load(":os_name.bzl", "is_windows_os", "os_name")

def _get_label(rctx, tool):
    ext = ".cmd" if is_windows_os(rctx) else ""
    return Label("@{}_{}//:{}{}".format(rctx.attr.node_repository, os_name(rctx), tool, ext))

def get_node_label(rctx):
    return _get_label(rctx, "bin/node")

def get_npm_label(rctx):
    return _get_label(rctx, "bin/npm")
