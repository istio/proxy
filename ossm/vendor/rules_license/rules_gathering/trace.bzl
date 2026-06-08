# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Rules and macros for collecting package metdata providers."""

TraceInfo = provider(
    doc = """Provides a target (as a string) to assist in debugging dependency issues.""",
    fields = {
        "trace": "String: a target to trace dependency edges to.",
    },
)

def _trace_impl(ctx):
    return TraceInfo(trace = ctx.build_setting_value)

trace = rule(
    doc = """Used to allow the specification of a target to trace while collecting license dependencies.""",
    implementation = _trace_impl,
    build_setting = config.string(flag = True),
)
