# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Dependencies that are needed for development and testing of rules_python itself."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", _http_archive = "http_archive", _http_file = "http_file")
load("@bazel_tools//tools/build_defs/repo:local.bzl", "local_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//python/private:internal_config_repo.bzl", "internal_config_repo")  # buildifier: disable=bzl-visibility

def http_archive(name, **kwargs):
    maybe(
        _http_archive,
        name = name,
        **kwargs
    )

def http_file(name, **kwargs):
    maybe(
        _http_file,
        name = name,
        **kwargs
    )

def rules_python_internal_deps():
    """Fetches all required dependencies for developing/testing rules_python itself.

    Setup of these dependencies is done by `internal_dev_setup.bzl`

    For dependencies needed by *users* of rules_python, see
    python/private/py_repositories.bzl.
    """
    internal_config_repo(
        name = "rules_python_internal",
        transition_settings = [
            str(Label("//tests/multi_pypi:external_deps_name")),
        ],
    )

    local_repository(
        name = "other",
        path = "tests/modules/other",
    )

    local_repository(
        name = "another_module",
        path = "tests/modules/another_module",
    )

    http_archive(
        name = "bazel_skylib",
        sha256 = "6e78f0e57de26801f6f564fa7c4a48dc8b36873e416257a92bbb0937eeac8446",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
        ],
    )

    # See https://github.com/bazelbuild/rules_shell/releases/tag/v0.2.0
    http_archive(
        name = "rules_shell",
        sha256 = "410e8ff32e018b9efd2743507e7595c26e2628567c42224411ff533b57d27c28",
        strip_prefix = "rules_shell-0.2.0",
        url = "https://github.com/bazelbuild/rules_shell/releases/download/v0.2.0/rules_shell-v0.2.0.tar.gz",
    )

    http_archive(
        name = "rules_pkg",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/1.0.1/rules_pkg-1.0.1.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/1.0.1/rules_pkg-1.0.1.tar.gz",
        ],
        sha256 = "d20c951960ed77cb7b341c2a59488534e494d5ad1d30c4818c736d57772a9fef",
    )

    http_archive(
        name = "rules_testing",
        sha256 = "02c62574631876a4e3b02a1820cb51167bb9cdcdea2381b2fa9d9b8b11c407c4",
        strip_prefix = "rules_testing-0.6.0",
        url = "https://github.com/bazelbuild/rules_testing/releases/download/v0.6.0/rules_testing-v0.6.0.tar.gz",
    )

    http_archive(
        name = "io_bazel_stardoc",
        sha256 = "62bd2e60216b7a6fec3ac79341aa201e0956477e7c8f6ccc286f279ad1d96432",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/stardoc/releases/download/0.6.2/stardoc-0.6.2.tar.gz",
            "https://github.com/bazelbuild/stardoc/releases/download/0.6.2/stardoc-0.6.2.tar.gz",
        ],
    )

    # The below two deps are required for the integration test with bazel
    # gazelle. Maybe the test should be moved to the `gazelle` workspace?
    http_archive(
        name = "io_bazel_rules_go",
        sha256 = "278b7ff5a826f3dc10f04feaf0b70d48b68748ccd512d7f98bf442077f043fe3",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.41.0/rules_go-v0.41.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.41.0/rules_go-v0.41.0.zip",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "727f3e4edd96ea20c29e8c2ca9e8d2af724d8c7778e7923a854b2c80952bc405",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.30.0/bazel-gazelle-v0.30.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.30.0/bazel-gazelle-v0.30.0.tar.gz",
        ],
    )

    # Test data for WHL tool testing.
    http_file(
        name = "futures_2_2_0_whl",
        downloaded_file_path = "futures-2.2.0-py2.py3-none-any.whl",
        sha256 = "9fd22b354a4c4755ad8c7d161d93f5026aca4cfe999bd2e53168f14765c02cd6",
        # From https://pypi.org/pypi/futures/2.2.0
        urls = [
            "https://mirror.bazel.build/pypi.org/packages/d7/1d/68874943aa37cf1c483fc61def813188473596043158faa6511c04a038b4/futures-2.2.0-py2.py3-none-any.whl",
            "https://pypi.org/packages/d7/1d/68874943aa37cf1c483fc61def813188473596043158faa6511c04a038b4/futures-2.2.0-py2.py3-none-any.whl",
        ],
    )

    http_file(
        name = "futures_3_1_1_whl",
        downloaded_file_path = "futures-3.1.1-py2-none-any.whl",
        sha256 = "c4884a65654a7c45435063e14ae85280eb1f111d94e542396717ba9828c4337f",
        # From https://pypi.org/pypi/futures
        urls = [
            "https://mirror.bazel.build/pypi.org/packages/a6/1c/72a18c8c7502ee1b38a604a5c5243aa8c2a64f4bba4e6631b1b8972235dd/futures-3.1.1-py2-none-any.whl",
            "https://pypi.org/packages/a6/1c/72a18c8c7502ee1b38a604a5c5243aa8c2a64f4bba4e6631b1b8972235dd/futures-3.1.1-py2-none-any.whl",
        ],
    )

    http_file(
        name = "google_cloud_language_whl",
        downloaded_file_path = "google_cloud_language-0.29.0-py2.py3-none-any.whl",
        sha256 = "a2dd34f0a0ebf5705dcbe34bd41199b1d0a55c4597d38ed045bd183361a561e9",
        # From https://pypi.org/pypi/google-cloud-language
        urls = [
            "https://mirror.bazel.build/pypi.org/packages/6e/86/cae57e4802e72d9e626ee5828ed5a646cf4016b473a4a022f1038dba3460/google_cloud_language-0.29.0-py2.py3-none-any.whl",
            "https://pypi.org/packages/6e/86/cae57e4802e72d9e626ee5828ed5a646cf4016b473a4a022f1038dba3460/google_cloud_language-0.29.0-py2.py3-none-any.whl",
        ],
    )

    http_file(
        name = "grpc_whl",
        downloaded_file_path = "grpcio-1.6.0-cp27-cp27m-manylinux1_i686.whl",
        sha256 = "c232d6d168cb582e5eba8e1c0da8d64b54b041dd5ea194895a2fe76050916561",
        # From https://pypi.org/pypi/grpcio/1.6.0
        urls = [
            "https://mirror.bazel.build/pypi.org/packages/c6/28/67651b4eabe616b27472c5518f9b2aa3f63beab8f62100b26f05ac428639/grpcio-1.6.0-cp27-cp27m-manylinux1_i686.whl",
            "https://pypi.org/packages/c6/28/67651b4eabe616b27472c5518f9b2aa3f63beab8f62100b26f05ac428639/grpcio-1.6.0-cp27-cp27m-manylinux1_i686.whl",
        ],
    )

    http_file(
        name = "mock_whl",
        downloaded_file_path = "mock-2.0.0-py2.py3-none-any.whl",
        sha256 = "5ce3c71c5545b472da17b72268978914d0252980348636840bd34a00b5cc96c1",
        # From https://pypi.org/pypi/mock
        urls = [
            "https://mirror.bazel.build/pypi.org/packages/e6/35/f187bdf23be87092bd0f1200d43d23076cee4d0dec109f195173fd3ebc79/mock-2.0.0-py2.py3-none-any.whl",
            "https://pypi.org/packages/e6/35/f187bdf23be87092bd0f1200d43d23076cee4d0dec109f195173fd3ebc79/mock-2.0.0-py2.py3-none-any.whl",
        ],
    )

    http_archive(
        name = "rules_bazel_integration_test",
        sha256 = "6e65d497c68f5794349bfa004369e144063686ce1ebd0227717cd23285be45ef",
        urls = [
            "https://github.com/bazel-contrib/rules_bazel_integration_test/releases/download/v0.20.0/rules_bazel_integration_test.v0.20.0.tar.gz",
        ],
    )

    # Dependency of rules_bazel_integration_test.
    http_archive(
        name = "cgrindel_bazel_starlib",
        sha256 = "9090280a9cff7322e7c22062506b3273a2e880ca464e520b5c77fdfbed4e8805",
        urls = [
            "https://github.com/cgrindel/bazel-starlib/releases/download/v0.18.1/bazel-starlib.v0.18.1.tar.gz",
        ],
    )

    http_archive(
        name = "com_google_protobuf",
        sha256 = "23082dca1ca73a1e9c6cbe40097b41e81f71f3b4d6201e36c134acc30a1b3660",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v29.0-rc2/protobuf-29.0-rc2.zip",
        strip_prefix = "protobuf-29.0-rc2",
    )

    # Needed for stardoc
    http_archive(
        name = "rules_java",
        urls = [
            "https://github.com/bazelbuild/rules_java/releases/download/8.6.2/rules_java-8.6.2.tar.gz",
        ],
        sha256 = "a64ab04616e76a448c2c2d8165d836f0d2fb0906200d0b7c7376f46dd62e59cc",
    )

    RULES_JVM_EXTERNAL_TAG = "5.2"
    RULES_JVM_EXTERNAL_SHA = "f86fd42a809e1871ca0aabe89db0d440451219c3ce46c58da240c7dcdc00125f"
    http_archive(
        name = "rules_jvm_external",
        patch_args = ["-p1"],
        patches = ["@io_bazel_stardoc//:rules_jvm_external.patch"],
        strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
        sha256 = RULES_JVM_EXTERNAL_SHA,
        url = "https://github.com/bazelbuild/rules_jvm_external/releases/download/%s/rules_jvm_external-%s.tar.gz" % (RULES_JVM_EXTERNAL_TAG, RULES_JVM_EXTERNAL_TAG),
    )

    http_archive(
        name = "rules_license",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_license/releases/download/0.0.7/rules_license-0.0.7.tar.gz",
            "https://github.com/bazelbuild/rules_license/releases/download/0.0.7/rules_license-0.0.7.tar.gz",
        ],
        sha256 = "4531deccb913639c30e5c7512a054d5d875698daeb75d8cf90f284375fe7c360",
    )

    http_archive(
        name = "bazel_features",
        sha256 = "d7787da289a7fb497352211ad200ec9f698822a9e0757a4976fd9f713ff372b3",
        strip_prefix = "bazel_features-1.9.1",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.9.1/bazel_features-v1.9.1.tar.gz",
    )

    http_archive(
        name = "rules_cc",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.1.5/rules_cc-0.1.5.tar.gz"],
        sha256 = "b8b918a85f9144c01f6cfe0f45e4f2838c7413961a8ff23bc0c6cdf8bb07a3b6",
        strip_prefix = "rules_cc-0.1.5",
    )

    http_archive(
        name = "rules_multirun",
        sha256 = "0e124567fa85287874eff33a791c3bbdcc5343329a56faa828ef624380d4607c",
        url = "https://github.com/keith/rules_multirun/releases/download/0.9.0/rules_multirun.0.9.0.tar.gz",
    )
