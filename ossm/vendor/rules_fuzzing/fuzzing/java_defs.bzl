# Copyright 2021 Google LLC
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

"""Public definitions for fuzzing rules.

Definitions outside this file are private unless otherwise noted, and may
change without notice.
"""

load(
    "//fuzzing/private:engine.bzl",
    _FuzzingEngineInfo = "FuzzingEngineInfo",
    _java_fuzzing_engine = "java_fuzzing_engine",
)
load(
    "//fuzzing/private:fuzz_test.bzl",
    _fuzzing_decoration = "fuzzing_decoration",
    _java_fuzz_test = "java_fuzz_test",
)

java_fuzz_test = _java_fuzz_test
java_fuzzing_engine = _java_fuzzing_engine

fuzzing_decoration = _fuzzing_decoration

FuzzingEngineInfo = _FuzzingEngineInfo
