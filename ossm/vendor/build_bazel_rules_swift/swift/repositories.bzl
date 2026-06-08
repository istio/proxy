# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Definitions for handling Bazel repositories used by the Swift rules."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    "//swift/internal:swift_autoconfiguration.bzl",
    "swift_autoconfiguration",
)

def _maybe(repo_rule, name, **kwargs):
    """Executes the given repository rule if it hasn't been executed already.

    Args:
      repo_rule: The repository rule to be executed (e.g., `http_archive`.)
      name: The name of the repository to be defined by the rule.
      **kwargs: Additional arguments passed directly to the repository rule.
    """
    if not native.existing_rule(name):
        repo_rule(name = name, **kwargs)

# buildifier: disable=unnamed-macro
def swift_rules_dependencies(include_bzlmod_ready_dependencies = True):
    """Fetches repositories that are dependencies of `rules_swift`.

    Users should call this macro in their `WORKSPACE` to ensure that all of the
    dependencies of the Swift rules are downloaded and that they are isolated
    from changes to those dependencies.

    Args:
        include_bzlmod_ready_dependencies: Whether or not bzlmod-ready
            dependencies should be included.
    """
    if include_bzlmod_ready_dependencies:
        _maybe(
            http_archive,
            name = "bazel_skylib",
            urls = [
                "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
                "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
            ],
            sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
        )

        _maybe(
            http_archive,
            name = "build_bazel_apple_support",
            url = "https://github.com/bazelbuild/apple_support/releases/download/1.15.1/apple_support.1.15.1.tar.gz",
            sha256 = "c4bb2b7367c484382300aee75be598b92f847896fb31bbd22f3a2346adf66a80",
        )

        _maybe(
            http_archive,
            name = "rules_proto",
            urls = [
                "https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.7.tar.gz",
            ],
            sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
            strip_prefix = "rules_proto-5.3.0-21.7",
        )

        _maybe(
            http_archive,
            name = "com_github_nlohmann_json",
            urls = [
                "https://github.com/nlohmann/json/releases/download/v3.6.1/include.zip",
            ],
            sha256 = "69cc88207ce91347ea530b227ff0776db82dcb8de6704e1a3d74f4841bc651cf",
            type = "zip",
            build_file = Label(
                "//third_party:com_github_nlohmann_json/BUILD.overlay",
            ),
        )

        _maybe(
            http_archive,
            name = "bazel_features",
            sha256 = "53182a68f172a2af4ad37051f82201e222bc19f7a40825b877da3ff4c922b9e0",
            strip_prefix = "bazel_features-1.3.0",
            url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.3.0/bazel_features-v1.3.0.tar.gz",
        )

        _maybe(
            http_archive,
            name = "com_github_apple_swift_argument_parser",
            urls = ["https://github.com/apple/swift-argument-parser/archive/refs/tags/1.3.0.tar.gz"],
            sha256 = "e5010ff37b542807346927ba68b7f06365a53cf49d36a6df13cef50d86018204",
            strip_prefix = "swift-argument-parser-1.3.0",
            build_file = Label(
                "//third_party:com_github_apple_swift_argument_parser/BUILD.overlay",
            ),
        )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_protobuf",
        urls = ["https://github.com/apple/swift-protobuf/archive/1.20.2.tar.gz"],  # pinned to grpc-swift version
        sha256 = "3fb50bd4d293337f202d917b6ada22f9548a0a0aed9d9a4d791e6fbd8a246ebb",
        strip_prefix = "swift-protobuf-1.20.2/",
        build_file = Label(
            "//third_party:com_github_apple_swift_protobuf/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_grpc_grpc_swift",
        urls = ["https://github.com/grpc/grpc-swift/archive/1.16.0.tar.gz"],  # latest at time of writing
        sha256 = "58b60431d0064969f9679411264b82e40a217ae6bd34e17096d92cc4e47556a5",
        strip_prefix = "grpc-swift-1.16.0/",
        build_file = Label(
            "//third_party:com_github_grpc_grpc_swift/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_docc_symbolkit",
        urls = ["https://github.com/apple/swift-docc-symbolkit/archive/refs/tags/swift-5.10-RELEASE.tar.gz"],
        sha256 = "de1d4b6940468ddb53b89df7aa1a81323b9712775b0e33e8254fa0f6f7469a97",
        strip_prefix = "swift-docc-symbolkit-swift-5.10-RELEASE",
        build_file = Label(
            "//third_party:com_github_apple_swift_docc_symbolkit/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_nio",
        urls = ["https://github.com/apple/swift-nio/archive/2.42.0.tar.gz"],  # pinned to grpc swift version
        sha256 = "e3304bc3fb53aea74a3e54bd005ede11f6dc357117d9b1db642d03aea87194a0",
        strip_prefix = "swift-nio-2.42.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_nio/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_nio_http2",
        urls = ["https://github.com/apple/swift-nio-http2/archive/1.26.0.tar.gz"],  # pinned to grpc-swift version
        sha256 = "f0edfc9d6a7be1d587e5b403f2d04264bdfae59aac1d74f7d974a9022c6d2b25",
        strip_prefix = "swift-nio-http2-1.26.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_nio_http2/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_nio_transport_services",
        urls = ["https://github.com/apple/swift-nio-transport-services/archive/1.15.0.tar.gz"],  # pinned to grpc-swift version
        sha256 = "f3498dafa633751a52b9b7f741f7ac30c42bcbeb3b9edca6d447e0da8e693262",
        strip_prefix = "swift-nio-transport-services-1.15.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_nio_transport_services/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_nio_extras",
        urls = ["https://github.com/apple/swift-nio-extras/archive/1.4.0.tar.gz"],  # pinned to grpc-swift version
        sha256 = "4684b52951d9d9937bb3e8ccd6b5daedd777021ef2519ea2f18c4c922843b52b",
        strip_prefix = "swift-nio-extras-1.4.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_nio_extras/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_log",
        urls = ["https://github.com/apple/swift-log/archive/1.4.4.tar.gz"],  # pinned to grpc-swift version
        sha256 = "48fe66426c784c0c20031f15dc17faf9f4c9037c192bfac2f643f65cb2321ba0",
        strip_prefix = "swift-log-1.4.4/",
        build_file = Label(
            "//third_party:com_github_apple_swift_log/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_nio_ssl",
        urls = ["https://github.com/apple/swift-nio-ssl/archive/2.23.0.tar.gz"],  # pinned to grpc swift version
        sha256 = "4787c63f61dd04d99e498adc3d1a628193387e41efddf8de19b8db04544d016d",
        strip_prefix = "swift-nio-ssl-2.23.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_nio_ssl/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_collections",
        urls = ["https://github.com/apple/swift-collections/archive/1.0.4.tar.gz"],  # pinned to swift-nio @ grpc-swift version
        sha256 = "d9e4c8a91c60fb9c92a04caccbb10ded42f4cb47b26a212bc6b39cc390a4b096",
        strip_prefix = "swift-collections-1.0.4/",
        build_file = Label(
            "//third_party:com_github_apple_swift_collections/BUILD.overlay",
        ),
    )

    _maybe(
        http_archive,
        name = "com_github_apple_swift_atomics",
        urls = ["https://github.com/apple/swift-atomics/archive/1.1.0.tar.gz"],  # pinned to swift-nio @ grpc-swift version
        sha256 = "1bee7f469f7e8dc49f11cfa4da07182fbc79eab000ec2c17bfdce468c5d276fb",
        strip_prefix = "swift-atomics-1.1.0/",
        build_file = Label(
            "//third_party:com_github_apple_swift_atomics/BUILD.overlay",
        ),
    )

    # It relies on `index-import` to import indexes into Bazel's remote
    # cache and allow using a global index internally in workers.
    # Note: this is only loaded if swift.index_while_building_v2 is enabled
    _maybe(
        http_archive,
        name = "build_bazel_rules_swift_index_import",
        build_file = Label("//third_party:build_bazel_rules_swift_index_import/BUILD.overlay"),
        canonical_id = "index-import-5.8",
        urls = ["https://github.com/MobileNativeFoundation/index-import/releases/download/5.8.0.1/index-import.tar.gz"],
        sha256 = "28c1ffa39d99e74ed70623899b207b41f79214c498c603915aef55972a851a15",
    )

    _maybe(
        swift_autoconfiguration,
        name = "build_bazel_rules_swift_local_config",
    )

    if include_bzlmod_ready_dependencies:
        native.register_toolchains(
            # Use register_toolchain's target pattern expansion to register all toolchains in the package.
            "@build_bazel_rules_swift_local_config//:all",
        )
