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

"""Development and production dependencies of rules_java."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//toolchains:jdk_build_file.bzl", "JDK_BUILD_TEMPLATE")
load("//toolchains:local_java_repository.bzl", "local_java_repository")
load("//toolchains:remote_java_repository.bzl", "remote_java_repository")

# visible for tests
JAVA_TOOLS_CONFIG = {
    "version": "v13.9",
    "release": "false",
    "artifacts": {
        "java_tools_linux": {
            "mirror_url": "https://mirror.bazel.build/bazel_java_tools/releases/java/v13.9/java_tools_linux-v13.9.zip",
            "github_url": "https://github.com/bazelbuild/java_tools/releases/download/java_v13.9/java_tools_linux-v13.9.zip",
            "sha": "7a3d7b1cd080efdf49ab2a3818177799416734acf2bd23040aa9037141287548",
        },
        "java_tools_windows": {
            "mirror_url": "https://mirror.bazel.build/bazel_java_tools/releases/java/v13.9/java_tools_windows-v13.9.zip",
            "github_url": "https://github.com/bazelbuild/java_tools/releases/download/java_v13.9/java_tools_windows-v13.9.zip",
            "sha": "6a17ac1921d60af5dca780f4200fd0f9963441bd7afff53b9efad6e7156c699d",
        },
        "java_tools_darwin_x86_64": {
            "mirror_url": "https://mirror.bazel.build/bazel_java_tools/releases/java/v13.9/java_tools_darwin_x86_64-v13.9.zip",
            "github_url": "https://github.com/bazelbuild/java_tools/releases/download/java_v13.9/java_tools_darwin_x86_64-v13.9.zip",
            "sha": "802bfb5085cec0ac5745a637ae2e7a7152c54230ba542d093a10bd48ba29ba6f",
        },
        "java_tools_darwin_arm64": {
            "mirror_url": "https://mirror.bazel.build/bazel_java_tools/releases/java/v13.9/java_tools_darwin_arm64-v13.9.zip",
            "github_url": "https://github.com/bazelbuild/java_tools/releases/download/java_v13.9/java_tools_darwin_arm64-v13.9.zip",
            "sha": "9fa400a43153b048ae5a785e3ee533d675ed6a994ab3c763f50bd15a28544c10",
        },
        "java_tools": {
            "mirror_url": "https://mirror.bazel.build/bazel_java_tools/releases/java/v13.9/java_tools-v13.9.zip",
            "github_url": "https://github.com/bazelbuild/java_tools/releases/download/java_v13.9/java_tools-v13.9.zip",
            "sha": "3b92e0c1884ac0e9683e87c3c49e1098cff91faeacdb76cc90d92efb0df861cf",
        },
    },
}

def java_tools_repos():
    """ Declares the remote java_tools repositories """
    for name, config in JAVA_TOOLS_CONFIG["artifacts"].items():
        maybe(
            http_archive,
            name = "remote_" + name,
            sha256 = config["sha"],
            urls = [
                config["mirror_url"],
                config["github_url"],
            ],
        )

def local_jdk_repo():
    maybe(
        local_java_repository,
        name = "local_jdk",
        build_file_content = JDK_BUILD_TEMPLATE,
    )

# DO NOT MANUALLY UPDATE! Update java/bazel/repositories_util.bzl instead and
# build the java/bazel:dump_remote_jdk_configs target to generate this list
_REMOTE_JDK_CONFIGS_LIST = [
    struct(
        name = "remote_jdk8_linux_aarch64",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:aarch64"],
        sha256 = "82c46c65d57e187ef68fdd125ef760eaeb52ebfe1be1a6a251cf5b43cbebc78a",
        strip_prefix = "zulu8.78.0.19-ca-jdk8.0.412-linux_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-linux_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-linux_aarch64.tar.gz"],
        version = "8",
    ),
    struct(
        name = "remote_jdk8_linux",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
        sha256 = "9c0ac5ebffa61520fee78ead52add0f4edd3b1b54b01b6a17429b719515caf90",
        strip_prefix = "zulu8.78.0.19-ca-jdk8.0.412-linux_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-linux_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-linux_x64.tar.gz"],
        version = "8",
    ),
    struct(
        name = "remote_jdk8_macos_aarch64",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:aarch64"],
        sha256 = "35bc35808379400e4a70e1f7ee379778881799b93c2cc9fe1ae515c03c2fb057",
        strip_prefix = "zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64.tar.gz"],
        version = "8",
    ),
    struct(
        name = "remote_jdk8_macos",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:x86_64"],
        sha256 = "2bfa0506196962bddb21a604eaa2b0b39eaf3383d0bdad08bdbe7f42f25d8928",
        strip_prefix = "zulu8.78.0.19-ca-jdk8.0.412-macosx_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-macosx_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-macosx_x64.tar.gz"],
        version = "8",
    ),
    struct(
        name = "remote_jdk8_windows",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:x86_64"],
        sha256 = "ca5499c301d5b42604d8535b8c40a7f928a796247b8c66a600333dd799798ff7",
        strip_prefix = "zulu8.78.0.19-ca-jdk8.0.412-win_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-win_x64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu8.78.0.19-ca-jdk8.0.412-win_x64.zip"],
        version = "8",
    ),
    struct(
        name = "remote_jdk8_linux_s390x",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:s390x"],
        sha256 = "276a431c79b7e94bc1b1b4fd88523383ae2d635ea67114dfc8a6174267f8fb2c",
        strip_prefix = "jdk8u292-b10",
        urls = ["https://github.com/AdoptOpenJDK/openjdk8-binaries/releases/download/jdk8u292-b10/OpenJDK8U-jdk_s390x_linux_hotspot_8u292b10.tar.gz", "https://mirror.bazel.build/github.com/AdoptOpenJDK/openjdk8-binaries/releases/download/jdk8u292-b10/OpenJDK8U-jdk_s390x_linux_hotspot_8u292b10.tar.gz"],
        version = "8",
    ),
    struct(
        name = "remotejdk11_linux_aarch64",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:aarch64"],
        sha256 = "be7d7574253c893eb58f66e985c75adf48558c41885827d1f02f827e109530e0",
        strip_prefix = "zulu11.72.19-ca-jdk11.0.23-linux_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-linux_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-linux_aarch64.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_linux",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
        sha256 = "0a4d1bfc7a96a7f9f5329b72b9801b3c53366417b4753f1b658fa240204c7347",
        strip_prefix = "zulu11.72.19-ca-jdk11.0.23-linux_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-linux_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-linux_x64.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_macos_aarch64",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:aarch64"],
        sha256 = "40fb1918385e03814b67b7608c908c7f945ccbeddbbf5ed062cdfb2602e21c83",
        strip_prefix = "zulu11.72.19-ca-jdk11.0.23-macosx_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-macosx_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-macosx_aarch64.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_macos",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:x86_64"],
        sha256 = "e5b19b82045826ae09c9d17742691bc9e40312c44be7bd7598ae418a3d4edb1c",
        strip_prefix = "zulu11.72.19-ca-jdk11.0.23-macosx_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-macosx_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-macosx_x64.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_win",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:x86_64"],
        sha256 = "1295b2affe498018c45f6f15187b58c4456d51dce5eb608ee73ef7665d4566d2",
        strip_prefix = "zulu11.72.19-ca-jdk11.0.23-win_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-win_x64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu11.72.19-ca-jdk11.0.23-win_x64.zip"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_linux_ppc64le",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:ppc"],
        sha256 = "a8fba686f6eb8ae1d1a9566821dbd5a85a1108b96ad857fdbac5c1e4649fc56f",
        strip_prefix = "jdk-11.0.15+10",
        urls = ["https://github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.15+10/OpenJDK11U-jdk_ppc64le_linux_hotspot_11.0.15_10.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.15+10/OpenJDK11U-jdk_ppc64le_linux_hotspot_11.0.15_10.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_linux_s390x",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:s390x"],
        sha256 = "a58fc0361966af0a5d5a31a2d8a208e3c9bb0f54f345596fd80b99ea9a39788b",
        strip_prefix = "jdk-11.0.15+10",
        urls = ["https://github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.15+10/OpenJDK11U-jdk_s390x_linux_hotspot_11.0.15_10.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.15+10/OpenJDK11U-jdk_s390x_linux_hotspot_11.0.15_10.tar.gz"],
        version = "11",
    ),
    struct(
        name = "remotejdk11_win_arm64",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:arm64"],
        sha256 = "b8a28e6e767d90acf793ea6f5bed0bb595ba0ba5ebdf8b99f395266161e53ec2",
        strip_prefix = "jdk-11.0.13+8",
        urls = ["https://aka.ms/download-jdk/microsoft-jdk-11.0.13.8.1-windows-aarch64.zip", "https://mirror.bazel.build/aka.ms/download-jdk/microsoft-jdk-11.0.13.8.1-windows-aarch64.zip"],
        version = "11",
    ),
    struct(
        name = "remotejdk17_linux_aarch64",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:aarch64"],
        sha256 = "518cc455c0c7b49c0ae7d809c0bb87ab371bb850d46abb8efad5010c6a06faec",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-linux_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-linux_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-linux_aarch64.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_linux",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
        sha256 = "a1e8ac9ae5804b84dc07cf9d8ebe1b18247d70c92c1e0de97ea10109563f4379",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-linux_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-linux_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-linux_x64.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_macos_aarch64",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:aarch64"],
        sha256 = "dd1a82d57e80cdefb045066e5c28b5bd41e57eea9c57303ec7e012b57230bb9c",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-macosx_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-macosx_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-macosx_aarch64.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_macos",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:x86_64"],
        sha256 = "b384991e93af39abe5229c7f5efbe912a7c5a6480674a6e773f3a9128f96a764",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-macosx_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-macosx_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-macosx_x64.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_win_arm64",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:arm64"],
        sha256 = "b8833d272eb31f54f8c881139807a28a74de9deae07d2cc37688ff72043e32c9",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-win_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-win_aarch64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-win_aarch64.zip"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_win",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:x86_64"],
        sha256 = "43f0f1bdecf48ba9763d46ee7784554c95b442ffdd39ebd62dc8b297cc82e116",
        strip_prefix = "zulu17.50.19-ca-jdk17.0.11-win_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-win_x64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-win_x64.zip"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_linux_ppc64le",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:ppc"],
        sha256 = "00a4c07603d0218cd678461b5b3b7e25b3253102da4022d31fc35907f21a2efd",
        strip_prefix = "jdk-17.0.8.1+1",
        urls = ["https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.8.1+1/OpenJDK17U-jdk_ppc64le_linux_hotspot_17.0.8.1_1.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.8.1+1/OpenJDK17U-jdk_ppc64le_linux_hotspot_17.0.8.1_1.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk17_linux_s390x",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:s390x"],
        sha256 = "ffacba69c6843d7ca70d572489d6cc7ab7ae52c60f0852cedf4cf0d248b6fc37",
        strip_prefix = "jdk-17.0.8.1+1",
        urls = ["https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.8.1+1/OpenJDK17U-jdk_s390x_linux_hotspot_17.0.8.1_1.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.8.1+1/OpenJDK17U-jdk_s390x_linux_hotspot_17.0.8.1_1.tar.gz"],
        version = "17",
    ),
    struct(
        name = "remotejdk21_linux_aarch64",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:aarch64"],
        sha256 = "da3c2d7db33670bcf66532441aeb7f33dcf0d227c8dafe7ce35cee67f6829c4c",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-linux_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_linux",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
        sha256 = "318d0c2ed3c876fb7ea2c952945cdcf7decfb5264ca51aece159e635ac53d544",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-linux_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_x64.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_macos_aarch64",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:aarch64"],
        sha256 = "bc2750f81a166cc6e9c30ae8aaba54f253a8c8ec9d8cfc04a555fe20712c7bff",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-macosx_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-macosx_aarch64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-macosx_aarch64.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_macos",
        target_compatible_with = ["@platforms//os:macos", "@platforms//cpu:x86_64"],
        sha256 = "5ce75a6a247c7029b74c4ca7cf6f60fd2b2d68ce1e8956fb448d2984316b5fea",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-macosx_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-macosx_x64.tar.gz", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-macosx_x64.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_win_arm64",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:arm64"],
        sha256 = "9f873eccf030b1d3dc879ec1eb0ff5e11bf76002dc81c5c644c3462bf6c5146b",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-win_aarch64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-win_aarch64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-win_aarch64.zip"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_win",
        target_compatible_with = ["@platforms//os:windows", "@platforms//cpu:x86_64"],
        sha256 = "d771dad10d3f0b440c3686d1f3d2b68b320802ac97b212d87671af3f2eef8848",
        strip_prefix = "zulu21.36.17-ca-jdk21.0.4-win_x64",
        urls = ["https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-win_x64.zip", "https://mirror.bazel.build/cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-win_x64.zip"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_linux_ppc64le",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:ppc"],
        sha256 = "c208cd0fb90560644a90f928667d2f53bfe408c957a5e36206585ad874427761",
        strip_prefix = "jdk-21.0.4+7",
        urls = ["https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_ppc64le_linux_hotspot_21.0.4_7.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_ppc64le_linux_hotspot_21.0.4_7.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_linux_riscv64",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:riscv64"],
        sha256 = "b04fd7f52d18268a935f1a7144dae802b25db600ec97156ddd46b3100cbd13da",
        strip_prefix = "jdk-21.0.4+7",
        urls = ["https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_riscv64_linux_hotspot_21.0.4_7.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_riscv64_linux_hotspot_21.0.4_7.tar.gz"],
        version = "21",
    ),
    struct(
        name = "remotejdk21_linux_s390x",
        target_compatible_with = ["@platforms//os:linux", "@platforms//cpu:s390x"],
        sha256 = "c900c8d64fab1e53274974fa4a4c736a5a3754485a5c56f4947281480773658a",
        strip_prefix = "jdk-21.0.4+7",
        urls = ["https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_s390x_linux_hotspot_21.0.4_7.tar.gz", "https://mirror.bazel.build/github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4+7/OpenJDK21U-jdk_s390x_linux_hotspot_21.0.4_7.tar.gz"],
        version = "21",
    ),
]

def _make_version_to_remote_jdks():
    result = {}
    for cfg in _REMOTE_JDK_CONFIGS_LIST:
        result.setdefault(cfg.version, [])
        result[cfg.version].append(cfg)
    return result

# visible for testing
REMOTE_JDK_CONFIGS = _make_version_to_remote_jdks()

def _remote_jdk_repos_for_version(version):
    for item in REMOTE_JDK_CONFIGS[version]:
        maybe(
            remote_java_repository,
            name = item.name,
            exec_compatible_with = item.target_compatible_with,
            sha256 = item.sha256,
            strip_prefix = item.strip_prefix,
            urls = item.urls,
            version = item.version,
        )

def remote_jdk8_repos(name = ""):
    """Imports OpenJDK 8 repositories.

    Args:
        name: The name of this macro (not used)
    """
    _remote_jdk_repos_for_version("8")

def remote_jdk11_repos():
    """Imports OpenJDK 11 repositories."""
    _remote_jdk_repos_for_version("11")

def remote_jdk17_repos():
    """Imports OpenJDK 17 repositories."""
    _remote_jdk_repos_for_version("17")

def remote_jdk21_repos():
    """Imports OpenJDK 21 repositories."""
    _remote_jdk_repos_for_version("21")

def rules_java_dependencies():
    """An utility method to load all dependencies of rules_java.

    Loads the remote repositories used by default in Bazel.
    """

    local_jdk_repo()
    remote_jdk8_repos()
    remote_jdk11_repos()
    remote_jdk17_repos()
    remote_jdk21_repos()
    java_tools_repos()

def rules_java_toolchains(name = "toolchains"):
    """An utility method to load all Java toolchains.

    Args:
        name: The name of this macro (not used)
    """
    native.register_toolchains(
        "//toolchains:all",
        "@local_jdk//:runtime_toolchain_definition",
        "@local_jdk//:bootstrap_runtime_toolchain_definition",
    )
    for items in REMOTE_JDK_CONFIGS.values():
        for item in items:
            native.register_toolchains(
                "@" + item.name + "_toolchain_config_repo//:toolchain",
                "@" + item.name + "_toolchain_config_repo//:bootstrap_runtime_toolchain",
            )
