load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Minimise the risk of accidentally depending on something that's not already loaded
load("//private/rules:maven_install.bzl", "maven_install")

_DEFAULT_REPOSITORIES = [
    "https://repo1.maven.org/maven2",
]

_MAVEN_VERSION = "3.9.8"
_MAVEN_RESOLVER_VERSION = "1.9.20"

def rules_jvm_external_deps(
        repositories = _DEFAULT_REPOSITORIES,
        deps_lock_file = "@rules_jvm_external//:rules_jvm_external_deps_install.json"):
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

    # The `rules_java` major version is tied to the major version of Bazel that it supports,
    # so this is different from the version in the MODULE file.
    major_version = native.bazel_version.partition(".")[0]
    if major_version == "5":
        maybe(
            http_archive,
            name = "rules_java",
            urls = [
                "https://github.com/bazelbuild/rules_java/releases/download/5.5.1/rules_java-5.5.1.tar.gz",
            ],
            sha256 = "73b88f34dc251bce7bc6c472eb386a6c2b312ed5b473c81fe46855c248f792e0",
        )

    else:
        maybe(
            http_archive,
            name = "rules_java",
            urls = [
                "https://github.com/bazelbuild/rules_java/releases/download/7.12.2/rules_java-7.12.2.tar.gz",
            ],
            sha256 = "a9690bc00c538246880d5c83c233e4deb83fe885f54c21bb445eb8116a180b83",
        )

    if major_version == "6":
        # Here goes the chain of rules compatibility resolution between RJE, java, android, cc and protobuf:
        #
        # rules_jvm_external wants to support LTS-2. For Bazel 8, this means supporting Bazel 6.
        #
        # rules_android is decoupled from Bazel 8, including its providers. ProguardSpecInfo is also decoupled, but to rules_java 7.12.2.
        #
        # So rules_java 7.12.2 is necessary for a decoupled rules_android to work with Bazel 6.
        #
        # But with workspace + rules_java 7.12.2, rules_java brings in a dep on
        # rules_cc's //cc package via //java/bazel/rules:rules (for CcInfo).
        # https://github.com/bazelbuild/rules_java/blob/2a9bd746974f6c94b159821d75130ad43e6b2970/java/bazel/rules/BUILD.bazel#L34-L35
        #
        # and rules_cc, in turn, brings in a dep on protobuf.
        #
        # And that's why we need the following deps:
        maybe(
            http_archive,
            name = "rules_cc",
            urls = ["https://github.com/bazelbuild/rules_cc/archive/faeafdb82814b4f7295c555781e800f080607bdd.tar.gz"],
            sha256 = "ca772d4fa149180dd1d81fe19a61c911dcebf9768d56209fc5bf382125ade0b6",
            strip_prefix = "rules_cc-faeafdb82814b4f7295c555781e800f080607bdd",
        )

        maybe(
            http_archive,
            name = "protobuf",
            sha256 = "da288bf1daa6c04d03a9051781caa52aceb9163586bff9aa6cfb12f69b9395aa",
            strip_prefix = "protobuf-27.0",
            url = "https://github.com/protocolbuffers/protobuf/releases/download/v27.0/protobuf-27.0.tar.gz",
        )

    maybe(
        http_archive,
        name = "rules_shell",
        url = "https://github.com/bazelbuild/rules_shell/releases/download/v0.3.0/rules_shell-v0.3.0.tar.gz",
        sha256 = "d8cd4a3a91fc1dc68d4c7d6b655f09def109f7186437e3f50a9b60ab436a0c53",
        strip_prefix = "rules_shell-0.3.0",
        # 0.3.0 uses load visibility and other Bazel 7+ features. Remove this
        # patch when we stop supporting Bazel 6.
        patches = ["@rules_jvm_external//:rules_shell_patch.diff"],
        patch_args = ["-p1"],
    )

    maybe(
        http_archive,
        name = "rules_license",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
            "https://github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
        ],
        sha256 = "26d4021f6898e23b82ef953078389dd49ac2b5618ac564ade4ef87cced147b38",
    )

    maybe(
        http_archive,
        name = "bazel_features",
        sha256 = "bdc12fcbe6076180d835c9dd5b3685d509966191760a0eb10b276025fcb76158",
        strip_prefix = "bazel_features-1.17.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.17.0/bazel_features-v1.17.0.tar.gz",
    )

    maven_install(
        name = "rules_jvm_external_deps",
        artifacts = [
            "com.google.auth:google-auth-library-credentials:1.23.0",
            "com.google.auth:google-auth-library-oauth2-http:1.23.0",
            "com.google.cloud:google-cloud-core:2.40.0",
            "com.google.cloud:google-cloud-storage:2.40.1",
            "com.google.code.gson:gson:2.11.0",
            "com.google.googlejavaformat:google-java-format:1.22.0",
            "com.google.guava:guava:33.2.1-jre",
            "org.apache.maven:maven-artifact:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-core:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-model:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-model-builder:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-settings:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-settings-builder:%s" % _MAVEN_VERSION,
            "org.apache.maven:maven-resolver-provider:%s" % _MAVEN_VERSION,
            "org.apache.maven.resolver:maven-resolver-api:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-impl:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-connector-basic:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-spi:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-transport-file:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-transport-http:%s" % _MAVEN_RESOLVER_VERSION,
            "org.apache.maven.resolver:maven-resolver-util:%s" % _MAVEN_RESOLVER_VERSION,
            "org.codehaus.plexus:plexus-cipher:2.1.0",
            "org.codehaus.plexus:plexus-sec-dispatcher:2.0",
            "org.codehaus.plexus:plexus-utils:3.5.1",
            "org.fusesource.jansi:jansi:2.4.1",
            "org.slf4j:jul-to-slf4j:2.0.12",
            "org.slf4j:log4j-over-slf4j:2.0.12",
            "org.slf4j:slf4j-simple:2.0.12",
            "software.amazon.awssdk:s3:2.26.12",
            "org.bouncycastle:bcprov-jdk15on:1.68",
            "org.bouncycastle:bcpg-jdk15on:1.68",
        ],
        maven_install_json = deps_lock_file,
        fail_if_repin_required = True,
        strict_visibility = True,
        fetch_sources = True,
        repositories = repositories,
    )
