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

# The kt rule has been separated from the other rules since it requires a
# dependency on @rules_kotlin that we would not like to transfer to the
# other rules.

load("//private/rules:kt_jvm_export.bzl", _kt_jvm_export = "kt_jvm_export")

kt_jvm_export = _kt_jvm_export
