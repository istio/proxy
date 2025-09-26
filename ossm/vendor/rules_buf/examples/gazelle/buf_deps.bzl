# Copyright 2021-2025 Buf Technologies, Inc.
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

load("@rules_buf//buf:defs.bzl", "buf_dependencies")

def buf_deps():
    buf_dependencies(
        name = "buf_deps_barapis",
        modules = [
            "buf.build/acme/paymentapis:6e230f46113f498392c82d12b1a07b70",
            "buf.build/acme/petapis:84a33a06f0954823a6f2a089fb1bb82e",
            "buf.build/envoyproxy/protoc-gen-validate:dc09a417d27241f7b069feae2cd74a0e",
            "buf.build/googleapis/googleapis:84c3cad756d2435982d9e3b72680fa96",
        ],
    )
    buf_dependencies(
        name = "buf_deps_fooapis",
        modules = [
            "buf.build/acme/paymentapis:6e230f46113f498392c82d12b1a07b70",
            "buf.build/acme/petapis:84a33a06f0954823a6f2a089fb1bb82e",
            "buf.build/googleapis/googleapis:84c3cad756d2435982d9e3b72680fa96",
        ],
    )
