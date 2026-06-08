# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""This is a modified version of
https://github.com/bazelbuild/bazel-skylib/blob/2b38b2f8bd4b8603d610cfc651fcbb299498147f/rules/common_settings.bzl"""

BuildSettingInfo = provider(fields = ["value"])

def _impl(ctx):
    return BuildSettingInfo(value = ctx.build_setting_value)

bool_flag = rule(
    implementation = _impl,
    build_setting = config.bool(flag = True),
)
