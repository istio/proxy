# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Instrumentation options for OSS-Fuzz."""

load("@rules_fuzzing//fuzzing/private:instrum_opts.bzl", "instrum_opts")

oss_fuzz_opts = instrum_opts.make(
    conlyopts = [%{conlyopts}],
    cxxopts = [%{cxxopts}],
)

native_library_sanitizer = "%{sanitizer}"
