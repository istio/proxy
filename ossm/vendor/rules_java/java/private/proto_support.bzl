# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Support for Java compilation of protocol buffer generated code."""

load("//java/private:native.bzl", "native_java_common")

# Partial support, because internal symbols are not available in older Bazel version
# TODO: Once Java rules are moved into the rules_java, this should become a full support.

def compile(*, injecting_rule_kind, enable_jspecify, include_compilation_info, **kwargs):  # buildifier: disable=unused-variable
    return native_java_common.compile(**kwargs)

def merge(providers, *, merge_java_outputs = True, merge_source_jars = True):  # buildifier: disable=unused-variable
    return native_java_common.merge(providers)
