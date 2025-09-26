"""Common dependencies for rules_proto_grpc."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("//internal:common.bzl", "check_bazel_minimum_version")

# Versions
MINIMUM_BAZEL_VERSION = "5.3.0"
ENABLE_VERSION_NAGS = False
PROTOBUF_VERSION = "21.10"  # When updating, also update protobuf-javascript, JS requirements, JS rulegen in js.go, Ruby requirements and C#/F# requirements
GRPC_VERSION = "1.54.1"  # When updating, also update grpc hash, grpc-java hash, Go repositories.bzl, Ruby requirements and C#/F# requirements
BUF_VERSION = "v1.9.0"
VERSIONS = {
    # Core
    "rules_proto": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_proto",
        "ref": "5.3.0-21.7",
        "sha256": "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
    },
    "com_google_protobuf": {
        "type": "github",
        "org": "protocolbuffers",
        "repo": "protobuf",
        "ref": "v{}".format(PROTOBUF_VERSION),
        "sha256": "f3f9ce6dc288f2f939bdc9d277ebdfbc8dbcd51741071f93da70e0e62919f57f",
    },
    "com_github_grpc_grpc": {
        "type": "github",
        "org": "grpc",
        "repo": "grpc",
        "ref": "v{}".format(GRPC_VERSION),
        "sha256": "79e3ff93f7fa3c8433e2165f2550fa14889fce147c15d9828531cbfc7ad11e01",
    },
    "zlib": {
        "type": "http",
        "urls": [
            "https://mirror.bazel.build/zlib.net/zlib-1.2.12.tar.gz",
            "https://zlib.net/zlib-1.2.12.tar.gz",
        ],
        "sha256": "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
        "strip_prefix": "zlib-1.2.12",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.zlib",
    },
    "rules_python": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_python",
        "ref": "0.15.0",
        "sha256": "fda23c37fbacf7579f94d5e8f342d3a831140e9471b770782e83846117dd6596",
    },
    "bazel_skylib": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "bazel-skylib",
        "ref": "1.3.0",
        "sha256": "3b620033ca48fcd6f5ef2ac85e0f6ec5639605fa2f627968490e52fc91a9932f",
    },
    "rules_pkg": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_pkg",
        "ref": "0.8.0",
        "sha256": "ab55ed03c8e10b5cfd0748a4f9cf5d55c23cb7a7e623c4d306b75684e57483e4",
    },

    # Android
    "build_bazel_rules_android": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_android",
        "ref": "9ab1134546364c6de84fc6c80b4202fdbebbbb35",
        "sha256": "f329928c62ade05ceda72c4e145fd300722e6e592627d43580dd0a8211c14612",
    },

    # Buf
    "protoc_gen_buf_breaking_darwin_arm64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-breaking-Darwin-arm64".format(BUF_VERSION)],
        "sha256": "785c4623ecb8b6f695c3252a5fd2c7ceb4013ba439ae07e6d38db0a8f4e0adc5",
        "executable": True,
    },
    "protoc_gen_buf_breaking_darwin_x86_64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-breaking-Darwin-x86_64".format(BUF_VERSION)],
        "sha256": "f0fc9d4f1129182b95961266087fc4775c7903dec5678723559a29932cdcb475",
        "executable": True,
    },
    "protoc_gen_buf_breaking_linux_x86_64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-breaking-Linux-x86_64".format(BUF_VERSION)],
        "sha256": "a288eea1ff7715b166c5cdd5dffd0be0e5dc5e126d564daac49a268cb32215f0",
        "executable": True,
    },
    "protoc_gen_buf_lint_darwin_arm64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-lint-Darwin-arm64".format(BUF_VERSION)],
        "sha256": "59b3af74dcc7710b88ef57775e8ca51283d89d730ba7146054f2fe71e3dbb41d",
        "executable": True,
    },
    "protoc_gen_buf_lint_darwin_x86_64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-lint-Darwin-x86_64".format(BUF_VERSION)],
        "sha256": "7bff384c4b7a2842878a98a62fba3c2760bd28e9ed1f8e8e6d9f5d13a8a0fec0",
        "executable": True,
    },
    "protoc_gen_buf_lint_linux_x86_64": {
        "type": "http_file",
        "urls": ["https://github.com/bufbuild/buf/releases/download/{}/protoc-gen-buf-lint-Linux-x86_64".format(BUF_VERSION)],
        "sha256": "7cd8366283198f781f1590d6b01cbc67ddae679d883fed80b4395c68bbf6a1b4",
        "executable": True,
    },

    # C#/F#
    "io_bazel_rules_dotnet": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_dotnet",
        "ref": "0b7ae93fa81b7327a655118da0581db5ebbe0b8d",
        "sha256": "715b072dbf52491c0505ad9059b72fdfacfe78cdca754f455f5ec568b6209c16",
    },

    # D
    "io_bazel_rules_d": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_d",
        "ref": "73a7fc7d1884b029a4723bef2a0bb1f3f93c3fb6",
        "sha256": "53bbc348ac8e8e66003dee887b2536e45739f649196733eb936991e53fdaac72",
    },
    "com_github_dcarp_protobuf_d": {
        "type": "http",
        "urls": ["https://github.com/dcarp/protobuf-d/archive/v0.6.2.tar.gz"],
        "sha256": "5509883fa042aa2e1c8c0e072e52c695fb01466f572bd828bcde06347b82d465",
        "strip_prefix": "protobuf-d-0.6.2",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_dcarp_protobuf_d",
    },

    # Doc
    "protoc_gen_doc_darwin_arm64": {
        "type": "http",
        "urls": ["https://github.com/pseudomuto/protoc-gen-doc/releases/download/v1.5.1/protoc-gen-doc_1.5.1_darwin_arm64.tar.gz"],
        "sha256": "6e8c737d9a67a6a873a3f1d37ed8bb2a0a9996f6dcf6701aa1048c7bd798aaf9",
        "build_file_content": """exports_files(glob(["protoc-gen-doc*"]))""",
    },
    "protoc_gen_doc_darwin_x86_64": {
        "type": "http",
        "urls": ["https://github.com/pseudomuto/protoc-gen-doc/releases/download/v1.5.1/protoc-gen-doc_1.5.1_darwin_amd64.tar.gz"],
        "sha256": "f429e5a5ddd886bfb68265f2f92c1c6a509780b7adcaf7a8b3be943f28e144ba",
        "build_file_content": """exports_files(glob(["protoc-gen-doc*"]))""",
    },
    "protoc_gen_doc_linux_x86_64": {
        "type": "http",
        "urls": ["https://github.com/pseudomuto/protoc-gen-doc/releases/download/v1.5.1/protoc-gen-doc_1.5.1_linux_amd64.tar.gz"],
        "sha256": "47cd72b07e6dab3408d686a65d37d3a6ab616da7d8b564b2bd2a2963a72b72fd",
        "build_file_content": """exports_files(glob(["protoc-gen-doc*"]))""",
    },
    "protoc_gen_doc_windows_x86_64": {
        "type": "http",
        "urls": ["https://github.com/pseudomuto/protoc-gen-doc/releases/download/v1.5.1/protoc-gen-doc_1.5.1_windows_amd64.tar.gz"],
        "sha256": "8acf0bf64eda29183b4c6745c3c6a12562fd9a8ab08d61788cf56e6659c66b3b",
        "build_file_content": """exports_files(glob(["protoc-gen-doc*"]))""",
    },

    # Go
    # When updating, update go version for go_register_toolchains in WORKSPACE and go.go
    "io_bazel_rules_go": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_go",
        "ref": "v0.39.1",
        "sha256": "473a064d502e89d11c497a59f9717d1846e01515a3210bd169f22323161c076e",
    },
    "bazel_gazelle": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "bazel-gazelle",
        "ref": "v0.24.0",  # 0.25.0+ has issues with "No dependencies were provided": https://github.com/bazelbuild/bazel-gazelle/issues/1217
        "sha256": "fc4c319b9e32ea44be8a5e1a46746d93e8b6a8b104baf7cb6a344a0a08386fed",
    },

    # grpc-gateway
    "com_github_grpc_ecosystem_grpc_gateway_v2": {
        "type": "github",
        "org": "grpc-ecosystem",
        "repo": "grpc-gateway",
        "ref": "v2.14.0",
        "sha256": "6bca5c2a0eddc0cd042ad8352c581c2135d0b0ab8a610f1045e9b17de9afb318",
    },

    # Java
    "io_grpc_grpc_java": {
        "type": "github",
        "org": "grpc",
        "repo": "grpc-java",
        "ref": "v{}".format(GRPC_VERSION),
        "sha256": "98c32df8a878cbca5a6799922d28e9df93a4d5607316e0e3f8269a5886d9e429",
    },
    "rules_jvm_external": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_jvm_external",
        "ref": "4.5",
        "sha256": "6e9f2b94ecb6aa7e7ec4a0fbf882b226ff5257581163e88bf70ae521555ad271",
    },

    # JavaScript
    # Use .tar.gz in release assets, not the Github generated source .tar.gz
    "build_bazel_rules_nodejs": {
        "type": "http",
        "urls": ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.7.2/rules_nodejs-5.7.2.tar.gz"],
        "sha256": "0e8a818724c0d5dcc10c31f9452ebd54b2ab94c452d4dcbb0d45a6636d2d5a44",
    },
    "com_google_protobuf_javascript": {
        "type": "github",
        "org": "protocolbuffers",
        "repo": "protobuf-javascript",
        "ref": "v3.21.2",
        "sha256": "35bca1729532b0a77280bf28ab5937438e3dcccd6b31a282d9ae84c896b6f6e3",
    },
    "grpc_web_plugin_darwin_arm64": {
        "type": "http_file",  # When updating, also update in package.json and vice-versa
        "urls": ["https://github.com/grpc/grpc-web/releases/download/1.4.2/protoc-gen-grpc-web-1.4.2-darwin-aarch64"],
        "sha256": "87263950cd36ec875c86b1e50625215727264c5495d6625ddf9e4f79aeef727e",
        "executable": True,
    },
    "grpc_web_plugin_darwin_x86_64": {
        "type": "http_file",  # When updating, also update in package.json and vice-versa
        "urls": ["https://github.com/grpc/grpc-web/releases/download/1.4.2/protoc-gen-grpc-web-1.4.2-darwin-x86_64"],
        "sha256": "6b73e8e9ef2deb114d39c9eea177ff8450d92e7154b5e47dea668a43499a2383",
        "executable": True,
    },
    "grpc_web_plugin_linux": {
        "type": "http_file",  # When updating, also update in package.json and vice-versa
        "urls": ["https://github.com/grpc/grpc-web/releases/download/1.4.2/protoc-gen-grpc-web-1.4.2-linux-x86_64"],
        "sha256": "5e82c3f1f435e176c94b94de9669911ab3bfb891608b7e80adff358f777ff857",
        "executable": True,
    },
    "grpc_web_plugin_windows": {
        "type": "http_file",  # When updating, also update in package.json and vice-versa
        "urls": ["https://github.com/grpc/grpc-web/releases/download/1.4.2/protoc-gen-grpc-web-1.4.2-windows-x86_64.exe"],
        "sha256": "3a0fc44662cb89a5c101b632e3e8841d04d9bea3103512deae82591e2acdff93",
        "executable": True,
    },

    # Ruby
    "bazelruby_rules_ruby": {
        "type": "github",
        "org": "bazelruby",
        "repo": "rules_ruby",
        "ref": "cc2f5ce961f7fa34557264dd05c7597e634f31e1",
        "sha256": "cf3c7d3c1e032c804e7f85e1c38e7f16cf50cb1353736e4ef69e3b63059d305f",
    },

    # Rust
    "rules_rust": {
        "type": "http",
        "urls": ["https://github.com/bazelbuild/rules_rust/releases/download/0.22.0/rules_rust-v0.22.0.tar.gz"],
        "sha256": "50272c39f20a3a3507cb56dcb5c3b348bda697a7d868708449e2fa6fb893444c",
    },

    # Scala
    "io_bazel_rules_scala": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_scala",
        "ref": "73f5d1a7da081c9f5160b9ed7ac745388af28e23",
        "sha256": "9529dd867c7f6951ce8b803849c3ddcddbaebbbff71bbbcac50fe94874c1cdf7",
    },

    # Swift
    "build_bazel_rules_swift": {
        "type": "github",
        "org": "bazelbuild",
        "repo": "rules_swift",
        "ref": "1.4.0",
        "sha256": "512c4a15b959b9558d4cf7cf05392d994cf5104def3bcdd2f69d3a6eab0b47b3",
    },
    "com_github_grpc_grpc_swift": {
        "type": "github",
        "org": "grpc",
        "repo": "grpc-swift",
        "ref": "1.6.0",
        "sha256": "f08729b656dd1e7c1e273f2362a907d3ce6721348a4cd347574cd1ef28a95983",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_grpc_grpc_swift",
    },
    "com_github_apple_swift_log": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-log",
        "ref": "1.4.2",
        "sha256": "de51662b35f47764b6e12e9f1d43e7de28f6dd64f05bc30a318cf978cf3bc473",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_log",
    },
    "com_github_apple_swift_nio": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-nio",
        "ref": "2.32.3",
        "sha256": "d6b41f67b907b458a4c1c86d3c8549835242cf40c49616b8d7531db002336835",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_nio",
    },
    "com_github_apple_swift_nio_extras": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-nio-extras",
        "ref": "1.10.2",
        "sha256": "2f37596dcf26532b867aee3dbd8c5354108a076174751f4e6a72a0b6506df05e",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_nio_extras",
    },
    "com_github_apple_swift_nio_http2": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-nio-http2",
        "ref": "1.18.3",
        "sha256": "497882ef4fd6980bd741a7c91783592bbee3bfac15278434cc17753c56d5dc63",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_nio_http2",
    },
    "com_github_apple_swift_nio_ssl": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-nio-ssl",
        "ref": "2.15.1",
        "sha256": "eefce9af7904b2e627219b9c78356d0bd3d659f06cdf2b45d931d832b21dcd46",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_nio_ssl",
    },
    "com_github_apple_swift_nio_transport_services": {
        # Dependency of com_github_grpc_grpc_swift
        "type": "github",
        "org": "apple",
        "repo": "swift-nio-transport-services",
        "ref": "1.11.3",
        "sha256": "1ac6867fb9251a3d4da2834b080c1cf90cf0fbdeccd66ef39b7a315e5d5612b6",
        "build_file": "@rules_proto_grpc//third_party:BUILD.bazel.com_github_apple_swift_nio_transport_services",
    },
}

def _generic_dependency(name, **kwargs):
    if name not in VERSIONS:
        fail("Name {} not in VERSIONS".format(name))
    dep = VERSIONS[name]

    existing_rules = native.existing_rules()
    if dep["type"] == "github":
        # Resolve ref and sha256
        ref = kwargs.get(name + "_ref", dep["ref"])
        sha256 = kwargs.get(name + "_sha256", dep["sha256"])

        # Fix GitHub naming normalisation in path
        stripped_ref = ref
        if stripped_ref.startswith("v"):
            stripped_ref = ref[1:]
        stripped_ref = stripped_ref.replace("@", "-")

        # Generate URLs
        urls = [
            "https://github.com/{}/{}/archive/{}.tar.gz".format(dep["org"], dep["repo"], ref),
        ]

        # Check for existing rule
        if name not in existing_rules:
            http_archive(
                name = name,
                strip_prefix = dep["repo"] + "-" + stripped_ref,
                urls = urls,
                sha256 = sha256,
                **{k: v for k, v in dep.items() if k in ["build_file", "patch_cmds"]}
            )
        elif existing_rules[name]["kind"] != "http_archive":
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different rule kind. Found {}, expected http_archive".format(
                    name,
                    existing_rules[name]["kind"],
                ))  # buildifier: disable=print
        elif existing_rules[name]["urls"] != tuple(urls):
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different version. Found urls={}, expected {}".format(
                    name,
                    existing_rules[name]["urls"],
                    tuple(urls),
                ))  # buildifier: disable=print

    elif dep["type"] == "http":
        if name not in existing_rules:
            args = {k: v for k, v in dep.items() if k in ["urls", "sha256", "strip_prefix", "build_file", "build_file_content"]}
            http_archive(name = name, **args)
        elif existing_rules[name]["kind"] != "http_archive":
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different rule kind. Found {}, expected http_archive".format(
                    name,
                    existing_rules[name]["kind"],
                ))  # buildifier: disable=print
        elif existing_rules[name]["urls"] != tuple(dep["urls"]):
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different version. Found urls={}, expected {}".format(
                    name,
                    existing_rules[name]["urls"],
                    tuple(dep["urls"]),
                ))  # buildifier: disable=print

    elif dep["type"] == "http_file":
        if name not in existing_rules:
            args = {k: v for k, v in dep.items() if k in ["urls", "sha256", "executable"]}
            http_file(name = name, **args)
        elif existing_rules[name]["kind"] != "http_file":
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different rule kind. Found {}, expected http_file".format(
                    name,
                    existing_rules[name]["kind"],
                ))  # buildifier: disable=print
        elif existing_rules[name]["urls"] != tuple(dep["urls"]):
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different version. Found urls={}, expected {}".format(
                    name,
                    existing_rules[name]["urls"],
                    tuple(dep["urls"]),
                ))  # buildifier: disable=print

    elif dep["type"] == "local":
        if name not in existing_rules:
            args = {k: v for k, v in dep.items() if k in ["path"]}
            native.local_repository(name = name, **args)
        elif existing_rules[name]["kind"] != "local_repository":
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different rule kind. Found {}, expected local_repository".format(
                    name,
                    existing_rules[name]["kind"],
                ))  # buildifier: disable=print
        elif existing_rules[name]["path"] != dep["path"]:
            if ENABLE_VERSION_NAGS:
                print("Dependency '{}' has already been declared with a different version. Found path={}, expected {}".format(
                    name,
                    existing_rules[name]["path"],
                    dep["urls"],
                ))  # buildifier: disable=print

    else:
        fail("Unknown dependency type {}".format(dep))

    if "binds" in dep:
        for bind in dep["binds"]:
            if bind["name"] not in native.existing_rules():
                native.bind(
                    name = bind["name"],
                    actual = bind["actual"],
                )

#
# Toolchains
#
def rules_proto_grpc_toolchains(name = ""):
    """Register the rules_proto_grpc toolchains."""
    check_bazel_minimum_version(MINIMUM_BAZEL_VERSION)
    native.register_toolchains(str(Label("//protobuf:protoc_toolchain")))

#
# Core
#
def rules_proto_grpc_repos(**kwargs):
    """Load the rules_proto_grpc common dependencies."""  # buildifier: disable=function-docstring-args
    check_bazel_minimum_version(MINIMUM_BAZEL_VERSION)

    rules_proto(**kwargs)
    rules_python(**kwargs)
    build_bazel_rules_swift(**kwargs)
    bazel_skylib(**kwargs)
    rules_pkg(**kwargs)

    com_google_protobuf(**kwargs)
    com_github_grpc_grpc(**kwargs)
    external_zlib(**kwargs)

def rules_proto(**kwargs):
    _generic_dependency("rules_proto", **kwargs)

def rules_python(**kwargs):
    _generic_dependency("rules_python", **kwargs)

def build_bazel_rules_swift(**kwargs):
    _generic_dependency("build_bazel_rules_swift", **kwargs)

def com_google_protobuf(**kwargs):
    _generic_dependency("com_google_protobuf", **kwargs)

def com_github_grpc_grpc(**kwargs):
    _generic_dependency("com_github_grpc_grpc", **kwargs)

def external_zlib(**kwargs):
    _generic_dependency("zlib", **kwargs)

#
# Misc
#
def bazel_skylib(**kwargs):
    _generic_dependency("bazel_skylib", **kwargs)

def rules_pkg(**kwargs):
    _generic_dependency("rules_pkg", **kwargs)

#
# Android
#
def build_bazel_rules_android(**kwargs):
    _generic_dependency("build_bazel_rules_android", **kwargs)

#
# Buf
#
def protoc_gen_buf_breaking_darwin_arm64(**kwargs):
    _generic_dependency("protoc_gen_buf_breaking_darwin_arm64", **kwargs)

def protoc_gen_buf_breaking_darwin_x86_64(**kwargs):
    _generic_dependency("protoc_gen_buf_breaking_darwin_x86_64", **kwargs)

def protoc_gen_buf_breaking_linux_x86_64(**kwargs):
    _generic_dependency("protoc_gen_buf_breaking_linux_x86_64", **kwargs)

def protoc_gen_buf_lint_darwin_arm64(**kwargs):
    _generic_dependency("protoc_gen_buf_lint_darwin_arm64", **kwargs)

def protoc_gen_buf_lint_darwin_x86_64(**kwargs):
    _generic_dependency("protoc_gen_buf_lint_darwin_x86_64", **kwargs)

def protoc_gen_buf_lint_linux_x86_64(**kwargs):
    _generic_dependency("protoc_gen_buf_lint_linux_x86_64", **kwargs)

#
# C#
#
def io_bazel_rules_dotnet(**kwargs):
    _generic_dependency("io_bazel_rules_dotnet", **kwargs)

#
# D
#
def io_bazel_rules_d(**kwargs):
    _generic_dependency("io_bazel_rules_d", **kwargs)

def com_github_dcarp_protobuf_d(**kwargs):
    _generic_dependency("com_github_dcarp_protobuf_d", **kwargs)

#
# Doc
#
def protoc_gen_doc_darwin_arm64(**kwargs):
    _generic_dependency("protoc_gen_doc_darwin_arm64", **kwargs)

def protoc_gen_doc_darwin_x86_64(**kwargs):
    _generic_dependency("protoc_gen_doc_darwin_x86_64", **kwargs)

def protoc_gen_doc_linux_x86_64(**kwargs):
    _generic_dependency("protoc_gen_doc_linux_x86_64", **kwargs)

def protoc_gen_doc_windows_x86_64(**kwargs):
    _generic_dependency("protoc_gen_doc_windows_x86_64", **kwargs)

#
# Go
#
def io_bazel_rules_go(**kwargs):
    _generic_dependency("io_bazel_rules_go", **kwargs)

def bazel_gazelle(**kwargs):
    _generic_dependency("bazel_gazelle", **kwargs)

#
# gRPC gateway
#
def com_github_grpc_ecosystem_grpc_gateway_v2(**kwargs):
    _generic_dependency("com_github_grpc_ecosystem_grpc_gateway_v2", **kwargs)

#
# Java
#
def io_grpc_grpc_java(**kwargs):
    _generic_dependency("io_grpc_grpc_java", **kwargs)

def rules_jvm_external(**kwargs):
    _generic_dependency("rules_jvm_external", **kwargs)

#
# JavaScript
#
def build_bazel_rules_nodejs(**kwargs):
    _generic_dependency("build_bazel_rules_nodejs", **kwargs)

def com_google_protobuf_javascript(**kwargs):
    _generic_dependency("com_google_protobuf_javascript", **kwargs)

def grpc_web_plugin_darwin_arm64(**kwargs):
    _generic_dependency("grpc_web_plugin_darwin_arm64", **kwargs)

def grpc_web_plugin_darwin_x86_64(**kwargs):
    _generic_dependency("grpc_web_plugin_darwin_x86_64", **kwargs)

def grpc_web_plugin_linux(**kwargs):
    _generic_dependency("grpc_web_plugin_linux", **kwargs)

def grpc_web_plugin_windows(**kwargs):
    _generic_dependency("grpc_web_plugin_windows", **kwargs)

#
# Ruby
#
def bazelruby_rules_ruby(**kwargs):
    _generic_dependency("bazelruby_rules_ruby", **kwargs)

#
# Rust
#
def rules_rust(**kwargs):
    _generic_dependency("rules_rust", **kwargs)

#
# Scala
#
def io_bazel_rules_scala(**kwargs):
    _generic_dependency("io_bazel_rules_scala", **kwargs)

#
# Swift
#
def com_github_grpc_grpc_swift(**kwargs):
    _generic_dependency("com_github_grpc_grpc_swift", **kwargs)

def com_github_apple_swift_log(**kwargs):
    _generic_dependency("com_github_apple_swift_log", **kwargs)

def com_github_apple_swift_nio(**kwargs):
    _generic_dependency("com_github_apple_swift_nio", **kwargs)

def com_github_apple_swift_nio_extras(**kwargs):
    _generic_dependency("com_github_apple_swift_nio_extras", **kwargs)

def com_github_apple_swift_nio_http2(**kwargs):
    _generic_dependency("com_github_apple_swift_nio_http2", **kwargs)

def com_github_apple_swift_nio_ssl(**kwargs):
    _generic_dependency("com_github_apple_swift_nio_ssl", **kwargs)

def com_github_apple_swift_nio_transport_services(**kwargs):
    _generic_dependency("com_github_apple_swift_nio_transport_services", **kwargs)
