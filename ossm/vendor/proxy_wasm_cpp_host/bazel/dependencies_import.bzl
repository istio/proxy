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

load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@fuzzing_py_deps//:requirements.bzl", pip_fuzzing_dependencies = "install_deps")
load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")
load("@rules_python//python:repositories.bzl", "python_register_toolchains")
load("@v8_python_deps//:requirements.bzl", pip_v8_dependencies = "install_deps")

def proxy_wasm_cpp_host_dependencies_import():
    python_register_toolchains(
        name = "python_3_11",
        python_version = "3.11",
        ignore_root_user_error = True,  # for docker run
    )
    bazel_skylib_workspace()
    bazel_features_deps()

    llvm_register_toolchains()

    rules_fuzzing_dependencies()

    pip_fuzzing_dependencies()
    pip_v8_dependencies()
