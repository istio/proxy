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

"""Dependencies for the Rust `bindgen` rules"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//3rdparty/crates:crates.bzl", "crate_repositories")

BINDGEN_VERSION = "0.71.1"

# buildifier: disable=unnamed-macro
def rust_bindgen_dependencies(is_bzlmod = False):
    """Declare dependencies needed for bindgen.

    Args:
        is_bzlmod (bool): Whether or not this macro is called in a bzlmod context.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """

    if not is_bzlmod:
        maybe(
            http_archive,
            name = "llvm-raw",
            urls = ["https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.3/llvm-project-17.0.3.src.tar.xz"],
            strip_prefix = "llvm-project-17.0.3.src",
            integrity = "sha256-vloeRNZPMGu0T859NuOzmTaU6OYSKyNIYIkGKDwXbbg=",
            build_file_content = "# empty",
            patch_args = ["-p1"],
            patches = [
                Label("//3rdparty/patches:llvm-raw.incompatible_disallow_empty_glob.patch"),
            ],
        )

        maybe(
            http_archive,
            name = "zlib",
            build_file = Label("//3rdparty:BUILD.zlib.bazel"),
            sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
            strip_prefix = "zlib-1.2.11",
            urls = [
                "https://zlib.net/zlib-1.2.11.tar.gz",
                "https://storage.googleapis.com/mirror.tensorflow.org/zlib.net/zlib-1.2.11.tar.gz",
            ],
        )

        maybe(
            http_archive,
            name = "zstd",
            build_file = Label("//3rdparty:BUILD.zstd.bazel"),
            sha256 = "7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0",
            strip_prefix = "zstd-1.5.2",
            urls = [
                "https://github.com/facebook/zstd/releases/download/v1.5.2/zstd-1.5.2.tar.gz",
            ],
        )

    bindgen_name = "rules_rust_bindgen__bindgen-cli-{}".format(BINDGEN_VERSION)
    maybe(
        http_archive,
        name = bindgen_name,
        integrity = "sha256-/e0QyglWr9DL5c+JzHGuGmeeZbghbGUfyhe6feisVNw=",
        type = "tar.gz",
        urls = ["https://static.crates.io/crates/bindgen-cli/bindgen-cli-{}.crate".format(BINDGEN_VERSION)],
        strip_prefix = "bindgen-cli-{}".format(BINDGEN_VERSION),
        build_file = Label("//3rdparty:BUILD.bindgen-cli.bazel"),
    )

    direct_deps = [
        struct(repo = bindgen_name, is_dev_dep = False),
    ]
    direct_deps.extend(crate_repositories())
    return direct_deps

# buildifier: disable=unnamed-macro
def rust_bindgen_register_toolchains(register_toolchains = True):
    """Registers the default toolchains for the `rules_rust` [bindgen][bg] rules.

    [bg]: https://rust-lang.github.io/rust-bindgen/

    Args:
        register_toolchains (bool, optional): Whether or not to register toolchains.
    """
    if register_toolchains:
        native.register_toolchains(str(Label("//:default_bindgen_toolchain")))
