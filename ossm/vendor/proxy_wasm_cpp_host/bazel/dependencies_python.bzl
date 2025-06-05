# Copyright 2024 Google LLC
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

load("@rules_fuzzing//fuzzing:init.bzl", "rules_fuzzing_init")
load("@rules_python//python:pip.bzl", "pip_parse")

def proxy_wasm_cpp_host_dependencies_python():
    # NOTE: this loads @fuzzing_py_deps via pip_parse
    rules_fuzzing_init()

    # V8 dependencies.
    pip_parse(
        name = "v8_python_deps",
        extra_pip_args = ["--require-hashes"],
        requirements_lock = "@v8//:bazel/requirements.txt",
    )
