workspace(name = "rules_jvm_external")

android_sdk_repository(name = "androidsdk")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load(
    "//private:versions.bzl",
    "COURSIER_CLI_GITHUB_ASSET_URL",
    "COURSIER_CLI_SHA256",
)

http_file(
    name = "coursier_cli",
    sha256 = COURSIER_CLI_SHA256,
    urls = [COURSIER_CLI_GITHUB_ASSET_URL],
)

load("//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

http_archive(
    name = "rules_kotlin",
    sha256 = "5766f1e599acf551aa56f49dab9ab9108269b03c557496c54acaf41f98e2b8d6",
    url = "https://github.com/bazelbuild/rules_kotlin/releases/download/v1.9.0/rules_kotlin-v1.9.0.tar.gz",
)

load("@rules_kotlin//kotlin:repositories.bzl", "kotlin_repositories")

kotlin_repositories()

load("@rules_kotlin//kotlin:core.bzl", "kt_register_toolchains")

kt_register_toolchains()

http_archive(
    name = "io_bazel_stardoc",
    sha256 = "3fd8fec4ddec3c670bd810904e2e33170bedfe12f90adf943508184be458c8bb",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/stardoc/releases/download/0.5.3/stardoc-0.5.3.tar.gz",
        "https://github.com/bazelbuild/stardoc/releases/download/0.5.3/stardoc-0.5.3.tar.gz",
    ],
)

load("@io_bazel_stardoc//:setup.bzl", "stardoc_repositories")

stardoc_repositories()

http_archive(
    name = "rules_testing",
    sha256 = "02c62574631876a4e3b02a1820cb51167bb9cdcdea2381b2fa9d9b8b11c407c4",
    strip_prefix = "rules_testing-0.6.0",
    url = "https://github.com/bazelbuild/rules_testing/releases/download/v0.6.0/rules_testing-v0.6.0.tar.gz",
)

http_archive(
    name = "aspect_bazel_lib",
    sha256 = "3522895fa13b97e8b27e3b642045682aa4233ae1a6b278aad6a3b483501dc9f2",
    strip_prefix = "bazel-lib-2.20.0",
    url = "https://github.com/bazel-contrib/bazel-lib/releases/download/v2.20.0/bazel-lib-v2.20.0.tar.gz",
)

load("@aspect_bazel_lib//lib:repositories.bzl", "aspect_bazel_lib_dependencies", "aspect_bazel_lib_register_toolchains")

# Required bazel-lib dependencies

aspect_bazel_lib_dependencies()

# Required rules_shell dependencies
load("@rules_shell//shell:repositories.bzl", "rules_shell_dependencies", "rules_shell_toolchains")

rules_shell_dependencies()

rules_shell_toolchains()

# Register bazel-lib toolchains

aspect_bazel_lib_register_toolchains()

# Create the host platform repository transitively required by bazel-lib

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@platforms//host:extension.bzl", "host_platform_repo")

maybe(
    host_platform_repo,
    name = "host_platform",
)

# Stardoc also depends on skydoc_repositories, rules_sass, rules_nodejs, but our
# usage of Stardoc (scripts/generate_docs) doesn't require any of these
# dependencies. So, we omit them to keep the WORKSPACE file simpler.
# https://skydoc.bazel.build/docs/getting_started_stardoc.html

# Required for buildifier (`//scripts:buildifier`)
http_file(
    name = "buildifier-linux-arm64",
    sha256 = "c22a44eee37b8927167ee6ee67573303f4e31171e7ec3a8ea021a6a660040437",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/v7.1.2/buildifier-linux-arm64"],
)

http_file(
    name = "buildifier-linux-x86_64",
    sha256 = "28285fe7e39ed23dc1a3a525dfcdccbc96c0034ff1d4277905d2672a71b38f13",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/v7.1.2/buildifier-linux-amd64"],
)

http_file(
    name = "buildifier-macos-arm64",
    sha256 = "d0909b645496608fd6dfc67f95d9d3b01d90736d7b8c8ec41e802cb0b7ceae7c",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/v7.1.2/buildifier-darwin-arm64"],
)

http_file(
    name = "buildifier-macos-x86_64",
    sha256 = "687c49c318fb655970cf716eed3c7bfc9caeea4f2931a2fd36593c458de0c537",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/v7.1.2/buildifier-darwin-amd64"],
)

# Begin test dependencies

load("//:defs.bzl", "maven_install")
load("//:specs.bzl", "maven")

maven_install(
    artifacts = [
        "com.google.guava:guava:31.1-jre",
        "org.hamcrest:hamcrest-core:2.1",
        "io.netty:netty-tcnative-boringssl-static:2.0.61.Final",
    ],
    maven_install_json = "@rules_jvm_external//:maven_coursier_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    resolver = "coursier",
)

load("@maven//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

maven_install(
    name = "exclusion_testing",
    artifacts = [
        maven.artifact(
            artifact = "guava",
            exclusions = [
                maven.exclusion(
                    artifact = "animal-sniffer-annotations",
                    group = "org.codehaus.mojo",
                ),
                "com.google.j2objc:j2objc-annotations",
            ],
            group = "com.google.guava",
            version = "27.0-jre",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "global_exclusion_testing",
    artifacts = [
        "com.google.guava:guava:27.0-jre",  # depends on animal-sniffer-annotations and j2objc-annotations
        "com.squareup.okhttp3:okhttp:3.14.1",  # depends on animal-sniffer-annotations
        "com.diffplug.durian:durian-core:1.2.0",  # depends on animal-sniffer-annotations and j2objc-annotations
    ],
    excluded_artifacts = [
        maven.exclusion(
            artifact = "animal-sniffer-annotations",
            group = "org.codehaus.mojo",
        ),
        "com.google.j2objc:j2objc-annotations",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "manifest_stamp_testing",
    artifacts = [
        "com.google.guava:guava:27.0-jre",
        "javax.inject:javax.inject:1",
        "org.apache.beam:beam-sdks-java-core:2.15.0",
        "org.bouncycastle:bcprov-jdk15on:1.64",
    ],
    maven_install_json = "//tests/custom_maven_install:manifest_stamp_testing_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@manifest_stamp_testing//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

maven_install(
    name = "multiple_lock_files",
    artifacts = [
        "org.zeromq:jeromq",
        "redis.clients:jedis",
    ],
    maven_install_json = "//tests/custom_maven_install:multiple_lock_files_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@multiple_lock_files//:defs.bzl", _multiple_lock_files_pinned_maven_install = "pinned_maven_install")

_multiple_lock_files_pinned_maven_install()

maven_install(
    name = "testing",
    artifacts = [
        "com.fasterxml.jackson:jackson-bom:2.9.10",
        "com.github.fommil.netlib:all:1.1.2",
    ],
    maven_install_json = "@//:foo.json",
    resolver = "maven",
)

# These artifacts helped discover limitations in the Coursier resolver. Each
# artifact listed here *must have* an accompanying issue. We build_test these
# targets to ensure that they remain supported by the rule.
maven_install(
    name = "regression_testing_coursier",
    artifacts = [
        # https://github.com/bazelbuild/rules_jvm_external/issues/74
        "org.pantsbuild:jarjar:1.6.6",
        # https://github.com/bazelbuild/rules_jvm_external/issues/59
        "junit:junit:4.12",
        "org.jetbrains.kotlin:kotlin-test:1.3.21",
        # https://github.com/bazelbuild/rules_jvm_external/issues/101
        # As referenced in the issue, daml is not available anymore, hence
        # replacing with another artifact with a classifier.
        "org.eclipse.jetty:jetty-http:jar:tests:9.4.20.v20190813",
        # https://github.com/bazelbuild/rules_jvm_external/issues/116
        "org.eclipse.jetty.orbit:javax.servlet:3.0.0.v201112011016",
        # https://github.com/bazelbuild/rules_jvm_external/issues/92#issuecomment-478430167
        maven.artifact(
            "com.squareup",
            "javapoet",
            "1.11.1",
            neverlink = True,
        ),
        # https://github.com/bazelbuild/rules_jvm_external/issues/98
        "com.github.fommil.netlib:all:1.1.2",
        "nz.ac.waikato.cms.weka:weka-stable:3.8.1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/111
        "com.android.support:appcompat-v7:28.0.0@aar",
        "com.google.android.gms:play-services-base:16.1.0",
        # https://github.com/bazelbuild/rules_jvm_external/issues/119#issuecomment-484278260
        "org.apache.flink:flink-test-utils_2.12:1.8.0",
        # https://github.com/bazelbuild/rules_jvm_external/issues/170
        "ch.epfl.scala:compiler-interface:1.3.0-M4+20-c8a2f9bd",
        # https://github.com/bazelbuild/rules_jvm_external/issues/172
        "org.openjfx:javafx-base:11.0.1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/178
        "io.kubernetes:client-java:4.0.0-beta1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/119#issuecomment-504704752
        "com.github.oshi:oshi-parent:3.4.0",
        "com.github.spinalhdl:spinalhdl-core_2.11:1.3.6",
        "com.github.spinalhdl:spinalhdl-lib_2.11:1.3.6",
        # https://github.com/bazelbuild/rules_jvm_external/issues/201
        "org.apache.kafka:kafka_2.11:2.1.1",
        "io.confluent:kafka-avro-serializer:5.0.1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/309
        "io.quarkus.http:quarkus-http-servlet:3.0.0.Beta1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/371
        "com.fasterxml.jackson:jackson-bom:2.9.10",
        "org.junit:junit-bom:5.3.1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/686
        "io.netty:netty-tcnative-boringssl-static:2.0.51.Final",
        # https://github.com/bazelbuild/rules_jvm_external/issues/852
        maven.artifact(
            artifact = "jaxb-ri",
            exclusions = [
                "com.sun.xml.bind:jaxb-samples",
                "com.sun.xml.bind:jaxb-release-documentation",
            ],
            group = "com.sun.xml.bind",
            version = "2.3.6",
        ),
        # https://github.com/bazelbuild/rules_jvm_external/issues/865
        maven.artifact(
            artifact = "google-api-services-compute",
            classifier = "javadoc",
            group = "com.google.apis",
            version = "v1-rev235-1.25.0",
        ),
        # https://github.com/bazelbuild/rules_jvm_external/issues/907
        # Any two platforms to ensure that it doesn't work _only_ under the host operating system
        "com.google.protobuf:protoc:exe:linux-x86_64:3.21.12",
        "com.google.protobuf:protoc:exe:osx-aarch_64:3.21.12",
        # https://github.com/bazelbuild/rules_jvm_external/issues/917
        # androidx core-testing POM has "exclusion" for "byte-buddy" but it should be downloaded as mockito-core
        # dependency when the usually omitted "jar" packaging type is specified.
        "org.mockito:mockito-core:3.3.3@jar",
        "androidx.arch.core:core-testing:2.1.0@aar",
        # https://github.com/bazelbuild/rules_jvm_external/issues/1028
        "build.buf:protovalidate:0.1.9",
        # https://github.com/bazelbuild/rules_jvm_external/issues/1250
        "com.github.spotbugs:spotbugs:4.7.0",
        # https://github.com/bazelbuild/rules_jvm_external/issues/1267
        "org.mockito:mockito-core:3.3.3@pom",
        # https://github.com/bazelbuild/rules_jvm_external/issues/1345
        maven.artifact(
            artifact = "jffi",
            classifier = "native",
            group = "com.github.jnr",
            version = "1.3.13",
        ),
    ],
    generate_compat_repositories = True,
    maven_install_json = "//tests/custom_maven_install:regression_testing_coursier_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
        "https://packages.confluent.io/maven/",
    ],
)

load("@regression_testing_coursier//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

load("@regression_testing_coursier//:compat.bzl", "compat_repositories")

compat_repositories()

# These artifacts helped discover limitations in the Maven resolver. Each
# artifact listed here *must have* an accompanying issue. We build_test these
# targets to ensure that they remain supported by the rule.
maven_install(
    name = "regression_testing_maven",
    artifacts = [
        # Depends on org.apache.yetus:audience-annotations:0.11.0 which has an invalid pom
        "org.apache.parquet:parquet-common:1.11.1",
        # https://github.com/bazelbuild/rules_jvm_external/issues/1144
        "org.codehaus.plexus:plexus:1.0.4",
        "org.hamcrest:hamcrest-core:1.3",
        # https://github.com/bazelbuild/rules_jvm_external/issues/199
        # https://github.com/bazelbuild/rules_jvm_external/issues/1162
        "io.opentelemetry:opentelemetry-sdk",
        maven.artifact(
            artifact = "opentelemetry-api",
            group = "io.opentelemetry",
            neverlink = True,
        ),
        # https://github.com/bazel-contrib/rules_jvm_external/issues/132
        "com.amazonaws:DynamoDBLocal:1.25.0",
    ],
    boms = [
        "io.opentelemetry:opentelemetry-bom:1.31.0",
    ],
    generate_compat_repositories = True,
    maven_install_json = "//tests/custom_maven_install:regression_testing_maven_install.json",
    repin_instructions = "Please run `REPIN=1 bazel run @regression_testing_maven//:pin` to refresh the lock file.",
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
    resolver = "maven",
)

load("@regression_testing_maven//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

load("@regression_testing_maven//:compat.bzl", "compat_repositories")

compat_repositories()

maven_install(
    name = "regression_testing_gradle",
    artifacts = [
        # https://github.com/bazel-contrib/rules_jvm_external/issues/909
        "androidx.compose.foundation:foundation-layout:1.5.0-beta01",
        # https://github.com/bazel-contrib/rules_jvm_external/issues/909#issuecomment-2019217013
        "androidx.annotation:annotation:1.6.0",
        # https://github.com/bazel-contrib/rules_jvm_external/issues/1409
        "com.squareup.okhttp3:okhttp:4.12.0",
        # https://github.com/bazel-contrib/rules_jvm_external/issues/1471
        "androidx.fragment:fragment-ktx:1.6.1",
    ],
    generate_compat_repositories = True,
    maven_install_json = "//tests/custom_maven_install:regression_testing_gradle_install.json",
    repin_instructions = "Please run `REPIN=1 bazel run @regression_testing_gradle//:pin` to refresh the lock file.",
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
    resolver = "gradle",
)

load("@regression_testing_gradle//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

load("@regression_testing_gradle//:compat.bzl", "compat_repositories")

compat_repositories()

# Grab com.google.ar.sceneform:rendering because we overrode it above
http_file(
    name = "com.google.ar.sceneform_rendering",
    downloaded_file_path = "rendering-1.10.0.aar",
    sha256 = "d2f6cd1d54eee0d5557518d1edcf77a3ba37494ae94f9bb862e570ee426a3431",
    urls = [
        "https://dl.google.com/android/maven2/com/google/ar/sceneform/rendering/1.10.0/rendering-1.10.0.aar",
    ],
)

maven_install(
    name = "testonly_testing",
    artifacts = [
        maven.artifact(
            artifact = "guava",
            group = "com.google.guava",
            version = "27.0-jre",
        ),
        maven.artifact(
            testonly = True,
            artifact = "auto-value-annotations",
            group = "com.google.auto.value",
            version = "1.6.3",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "policy_pinned_testing",
    artifacts = [
        # https://github.com/bazelbuild/rules_jvm_external/issues/107
        "com.google.cloud:google-cloud-storage:1.66.0",
        "com.google.guava:guava:25.0-android",
    ],
    maven_install_json = "//tests/custom_maven_install:policy_pinned_testing_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
    version_conflict_policy = "pinned",
)

load(
    "@policy_pinned_testing//:defs.bzl",
    _policy_pinned_maven_install = "pinned_maven_install",
)

_policy_pinned_maven_install()

maven_install(
    name = "strict_visibility_testing",
    artifacts = [
        # https://github.com/bazelbuild/rules_jvm_external/issues/94
        "org.apache.tomcat:tomcat-catalina:9.0.24",
        # https://github.com/bazelbuild/rules_jvm_external/issues/255
        maven.artifact(
            artifact = "jetty-http",
            classifier = "tests",
            group = "org.eclipse.jetty",
            version = "9.4.20.v20190813",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    strict_visibility = True,
)

maven_install(
    name = "strict_visibility_with_compat_testing",
    artifacts = [
        # Must not be in any other maven_install where generate_compat_repositories = True
        "com.google.http-client:google-http-client-gson:1.42.3",
    ],
    generate_compat_repositories = True,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    strict_visibility = True,
)

load("@strict_visibility_with_compat_testing//:compat.bzl", "compat_repositories")

compat_repositories()

maven_install(
    name = "maven_install_in_custom_location",
    artifacts = ["com.google.guava:guava:27.0-jre"],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:maven_install.json",
    repositories = ["https://repo1.maven.org/maven2"],
)

load("@maven_install_in_custom_location//:defs.bzl", "pinned_maven_install")

pinned_maven_install()

maven_install(
    name = "duplicate_version_warning",
    artifacts = [
        "com.fasterxml.jackson.core:jackson-annotations:2.10.1",
        "com.fasterxml.jackson.core:jackson-annotations:2.12.1",
        "com.fasterxml.jackson.core:jackson-annotations:2.10.1",
        "com.fasterxml.jackson.core:jackson-annotations:2.11.2",
        "com.github.jnr:jffi:1.3.4",
        maven.artifact(
            artifact = "jffi",
            classifier = "native",
            group = "com.github.jnr",
            version = "1.3.3",
        ),
        maven.artifact(
            artifact = "jffi",
            classifier = "native",
            group = "com.github.jnr",
            version = "1.3.2",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
)

maven_install(
    name = "duplicate_version_warning_same_version",
    artifacts = [
        "com.fasterxml.jackson.core:jackson-annotations:2.10.1",
        "com.fasterxml.jackson.core:jackson-annotations:2.10.1",
        maven.artifact(
            artifact = "jffi",
            classifier = "native",
            group = "com.github.jnr",
            version = "1.3.3",
        ),
        maven.artifact(
            artifact = "jffi",
            classifier = "native",
            group = "com.github.jnr",
            version = "1.3.3",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
)

# The test this is for is only for `bzlmod`, but we want to
# be able to run tests in workspace mode too
maven_install(
    name = "root_wins",
    artifacts = [
        "io.netty:netty-buffer:4.1.121.Final",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "service_indexing_testing",
    artifacts = [
        "com.google.auto.value:auto-value:1.10.4",
        "com.google.auto.value:auto-value-annotations:1.10.4",
        "org.projectlombok:lombok:1.18.22",
    ] + [
        maven.artifact(
            testonly = True,  # must be propagated to the generated plugin
            artifact = artifact,
            group = "org.openjdk.jmh",
            version = "1.37",
        )
        for artifact in ("jmh-core", "jmh-generator-annprocess")
    ],
    maven_install_json = "//tests/custom_maven_install:service_indexing_testing.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@service_indexing_testing//:defs.bzl", pinned_service_indexing_testing = "pinned_maven_install")

pinned_service_indexing_testing()

maven_install(
    name = "jvm_import_test",
    artifacts = [
        "com.google.code.findbugs:jsr305:3.0.2",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "starlark_aar_import_with_sources_test",
    # The default is "@rules_android//rules:rules.bzl" but use
    # "@rules_android//android:rules.bzl" with the older 0.1.1 release
    # to use the native rules.
    aar_import_bzl_label = "@rules_android//android:rules.bzl",
    artifacts = [
        "androidx.work:work-runtime:2.6.0",
    ],
    fetch_sources = True,
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
    use_starlark_android_rules = True,
)

maven_install(
    name = "starlark_aar_import_test",
    # The default is "@rules_android//rules:rules.bzl" but use
    # "@rules_android//android:rules.bzl" with the older 0.1.1 release
    # to use the native rules.
    aar_import_bzl_label = "@rules_android//android:rules.bzl",
    artifacts = [
        "com.android.support:appcompat-v7:28.0.0",
    ],
    fetch_sources = False,
    repositories = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ],
    use_starlark_android_rules = True,
)

# for the above "starlark_aar_import_test" maven_install with
# use_starlark_android_rules = True.
# Note that this version is different from the version in MODULE.bazel
# because the latest versions of rules_android do not support Bazel 6,
# which rules_jvm_external supports and uses in CI tests. So use
# rules_android 0.1.1, which are wrappers around the native Android rules,
# since the tests with Bazel 6 do no use bzlmod.
http_archive(
    name = "rules_android",
    sha256 = "cd06d15dd8bb59926e4d65f9003bfc20f9da4b2519985c27e190cddc8b7a7806",
    strip_prefix = "rules_android-0.1.1",
    url = "https://github.com/bazelbuild/rules_android/archive/v0.1.1.zip",
)

# https://github.com/bazelbuild/rules_jvm_external/issues/351
maven_install(
    name = "json_artifacts_testing",
    artifacts = [
        "org.json:json:20190722",
        "io.quarkus:quarkus-maven-plugin:1.0.1.Final",
        "io.quarkus:quarkus-bom-descriptor-json:1.0.1.Final",
    ],
    fetch_sources = True,
    maven_install_json = "//tests/custom_maven_install:json_artifacts_testing_install.json",
    repositories = [
        "https://repo.maven.apache.org/maven2/",
        "https://repo.spring.io/plugins-release/",
    ],
)

# https://github.com/bazelbuild/rules_jvm_external/issues/433
maven_install(
    name = "version_interval_testing",
    artifacts = [
        "io.grpc:grpc-netty-shaded:1.29.0",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load(
    "@json_artifacts_testing//:defs.bzl",
    _json_artifacts_testing_install = "pinned_maven_install",
)

_json_artifacts_testing_install()

maven_install(
    name = "m2local_testing",
    artifacts = [
        # this is a test jar built for integration
        # tests in this repo
        "com.example:kt:1.0.0",
    ],
    fail_on_missing_checksum = True,
    repositories = [
        "m2Local",
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "m2local_testing_repin",
    artifacts = [
        # this is a test jar built for integration
        # tests in this repo
        "com.example:no-docs:1.0.0",
    ],
    maven_install_json = "//tests/custom_maven_install:m2local_testing_with_pinned_file_install.json",
    repositories = [
        "m2Local",
        "https://repo1.maven.org/maven2",
    ],
)

load("@m2local_testing_repin//:defs.bzl", m2local_testing_repin_maven_install = "pinned_maven_install")

m2local_testing_repin_maven_install()

maven_install(
    name = "m2local_testing_without_checksum",
    artifacts = [
        # this is a test jar built for integration
        # tests in this repo
        "com.example:kt:1.0.0",
    ],
    # jar won't have checksums for this test case
    fail_on_missing_checksum = False,
    repositories = [
        "m2Local",
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "m2local_testing_ignore_empty_files",
    artifacts = [
        # this is a test jar built for integration
        # tests in this repo
        "com.example:kt:1.0.0",
    ],
    fetch_sources = True,
    ignore_empty_files = True,
    repositories = [
        "m2Local",
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "m2local_testing_ignore_empty_files_repin",
    artifacts = [
        # this is a test jar built for integration
        # tests in this repo
        "com.example:kt:1.0.0",
    ],
    fetch_sources = True,
    ignore_empty_files = True,
    maven_install_json = "//tests/custom_maven_install:m2local_testing_ignore_empty_files_with_pinned_file_install.json",
    repositories = [
        "m2Local",
        "https://repo1.maven.org/maven2",
    ],
)

load("@m2local_testing_ignore_empty_files_repin//:defs.bzl", m2local_testing_ignore_empty_files_repin_maven_install = "pinned_maven_install")

m2local_testing_ignore_empty_files_repin_maven_install()

maven_install(
    name = "artifact_with_plus",
    artifacts = [
        "ch.epfl.scala:compiler-interface:1.3.0-M4+47-d881fa2f",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "artifact_with_plus_repin",
    artifacts = [
        "ch.epfl.scala:compiler-interface:1.3.0-M4+47-d881fa2f",
    ],
    maven_install_json = "//tests/custom_maven_install:artifact_with_plus_repin_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@artifact_with_plus_repin//:defs.bzl", artifact_with_plus_maven_install = "pinned_maven_install")

artifact_with_plus_maven_install()

maven_install(
    name = "v1_lock_file_format",
    artifacts = [
        # Coordinates that are in no other `maven_install`
        "org.seleniumhq.selenium:selenium-remote-driver:4.8.0",
    ],
    generate_compat_repositories = True,
    maven_install_json = "//tests/custom_maven_install:v1_lock_file_format_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@v1_lock_file_format//:defs.bzl", v1_lock_file_format_pinned_maven_install = "pinned_maven_install")

v1_lock_file_format_pinned_maven_install()

http_file(
    name = "hamcrest_core_for_test",
    downloaded_file_path = "hamcrest-core-1.3.jar",
    sha256 = "66fdef91e9739348df7a096aa384a5685f4e875584cce89386a7a47251c4d8e9",
    urls = [
        "https://repo1.maven.org/maven2/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    ],
)

http_file(
    name = "hamcrest_core_srcs_for_test",
    downloaded_file_path = "hamcrest-core-1.3-sources.jar",
    sha256 = "e223d2d8fbafd66057a8848cc94222d63c3cedd652cc48eddc0ab5c39c0f84df",
    urls = [
        "https://repo1.maven.org/maven2/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3-sources.jar",
    ],
)

http_file(
    name = "gson_for_test",
    downloaded_file_path = "gson-2.9.0.jar",
    sha256 = "c96d60551331a196dac54b745aa642cd078ef89b6f267146b705f2c2cbef052d",
    urls = [
        "https://repo1.maven.org/maven2/com/google/code/gson/gson/2.9.0/gson-2.9.0.jar",
    ],
)

http_file(
    name = "junit_platform_commons_for_test",
    downloaded_file_path = "junit-platform-commons-1.8.2.jar",
    sha256 = "d2e015fca7130e79af2f4608dc54415e4b10b592d77333decb4b1a274c185050",
    urls = [
        "https://repo1.maven.org/maven2/org/junit/platform/junit-platform-commons/1.8.2/junit-platform-commons-1.8.2.jar",
    ],
)

# https://github.com/bazelbuild/rules_jvm_external/issues/865
http_file(
    name = "google_api_services_compute_javadoc_for_test",
    downloaded_file_path = "google-api-services-compute-v1-rev235-1.25.0-javadoc.jar",
    sha256 = "b03be5ee8effba3bfbaae53891a9c01d70e2e3bd82ad8889d78e641b22bd76c2",
    urls = [
        "https://repo1.maven.org/maven2/com/google/apis/google-api-services-compute/v1-rev235-1.25.0/google-api-services-compute-v1-rev235-1.25.0-javadoc.jar",
    ],
)

http_file(
    name = "lombok_for_test",
    downloaded_file_path = "lombok-1.18.22.jar",
    sha256 = "ecef1581411d7a82cc04281667ee0bac5d7c0a5aae74cfc38430396c91c31831",
    urls = [
        "https://repo1.maven.org/maven2/org/projectlombok/lombok/1.18.22/lombok-1.18.22.jar",
    ],
)

# End test dependencies

http_archive(
    name = "bazel_toolchains",
    sha256 = "179ec02f809e86abf56356d8898c8bd74069f1bd7c56044050c2cd3d79d0e024",
    strip_prefix = "bazel-toolchains-4.1.0",
    urls = [
        "https://github.com/bazelbuild/bazel-toolchains/releases/download/4.1.0/bazel-toolchains-4.1.0.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/releases/download/4.1.0/bazel-toolchains-4.1.0.tar.gz",
    ],
)

http_archive(
    name = "bazelci_rules",
    sha256 = "eca21884e6f66a88c358e580fd67a6b148d30ab57b1680f62a96c00f9bc6a07e",
    strip_prefix = "bazelci_rules-1.0.0",
    url = "https://github.com/bazelbuild/continuous-integration/releases/download/rules-1.0.0/bazelci_rules-1.0.0.tar.gz",
)

load("@bazelci_rules//:rbe_repo.bzl", "rbe_preconfig")

# Creates a default toolchain config for RBE.
# Use this as is if you are using the rbe_ubuntu16_04 container,
# otherwise refer to RBE docs.
rbe_preconfig(
    name = "buildkite_config",
    toolchain = "ubuntu1804-bazel-java11",
)

# Located at the end, because it's only used in tests

http_archive(
    name = "com_google_protobuf",
    sha256 = "e07046fbac432b05adc1fd1318c6f19ab1b0ec0655f7f4e74627d9713959a135",
    strip_prefix = "protobuf-21.7",
    url = "https://github.com/protocolbuffers/protobuf/releases/download/v21.7/protobuf-all-21.7.tar.gz",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# When using `bzlmod` this gets pulled in as `protobuf`
http_archive(
    name = "protobuf",
    sha256 = "e07046fbac432b05adc1fd1318c6f19ab1b0ec0655f7f4e74627d9713959a135",
    strip_prefix = "protobuf-21.7",
    url = "https://github.com/protocolbuffers/protobuf/releases/download/v21.7/protobuf-all-21.7.tar.gz",
)

maven_install(
    name = "java_export_exclusion_testing",
    artifacts = [
        "com.google.protobuf:protobuf-java:3.23.1",
    ],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:java_export_exclusion_testing_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@java_export_exclusion_testing//:defs.bzl", _java_export_exclusion_testing_pinned_maven_install = "pinned_maven_install")

_java_export_exclusion_testing_pinned_maven_install()

maven_install(
    name = "override_target_in_deps",
    artifacts = [
        "io.opentelemetry:opentelemetry-sdk:1.28.0",
        "org.slf4j:slf4j-log4j12:1.7.36",
        "redis.clients:jedis:5.0.2",
    ],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:override_target_in_deps_install.json",
    override_targets = {
        # https://github.com/bazelbuild/rules_jvm_external/issues/199
        # This is a transitive dep of `opentelemetry-sdk`
        "io.opentelemetry:opentelemetry-api": "@//tests/integration/override_targets:additional_deps",
        "org.slf4j:slf4j-log4j12": "@override_target_in_deps//:org_slf4j_slf4j_reload4j",
    },
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@override_target_in_deps//:defs.bzl", _override_target_in_deps_maven_install = "pinned_maven_install")

_override_target_in_deps_maven_install()

maven_install(
    name = "same_override_target",
    artifacts = [
        "org.slf4j:slf4j-log4j12:1.7.36",
    ],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:same_override_target_install.json",
    override_targets = {
        "org.slf4j:slf4j-log4j12": "@same_override_target//:org_slf4j_slf4j_reload4j",
    },
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@same_override_target//:defs.bzl", _same_override_target_maven_install = "pinned_maven_install")

_same_override_target_maven_install()

maven_install(
    name = "forcing_versions",
    artifacts = [
        # Specify an ancient version of guava, and force its use. If we try to use `[23.3-jre]` as the version,
        # the resolution will fail when using `coursier`
        maven.artifact(
            artifact = "guava",
            force_version = True,
            group = "com.google.guava",
            version = "23.3-jre",
        ),
        # And something that depends on a more recent version of guava
        "xyz.rogfam:littleproxy:2.1.0",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "coursier_resolved_with_boms",
    artifacts = [
        "com.google.auth:google-auth-library-oauth2-http",
        "com.google.auto:auto-common:1.2.2",
        maven.artifact(
            artifact = "google-cloud-bigquery",
            exclusions = [
                "io.grpc:grpc-auth",
                "io.grpc:grpc-netty",
            ],
            group = "com.google.cloud",
        ),
    ],
    boms = [
        "com.google.cloud:libraries-bom:26.59.0",
    ],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:coursier_resolved_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

load("@coursier_resolved_with_boms//:defs.bzl", _coursier_resolved_maven_install = "pinned_maven_install")

_coursier_resolved_maven_install()

maven_install(
    name = "maven_resolved_with_boms",
    artifacts = [
        # A transitive dependency pulls in a `managedDependencies` section which sets the
        # `xmlpull` version to 1.2.0, which hasn't been publicly released. Maven and Gradle
        # both handle this situation gracefully and correctly resolve to `xmlpull` 1.1.3.1
        "org.drools:drools-mvel:7.53.0.Final",
        "org.optaplanner:optaplanner-core:7.53.0.Final",
        "org.seleniumhq.selenium:selenium-java",
        maven.artifact(
            testonly = True,
            artifact = "auto-value-annotations",
            exclusions = [
                "org.slf4j:slf4j-api",
            ],
            group = "com.google.auto.value",
            version = "1.6.3",
        ),
        maven.artifact(
            artifact = "json-lib",
            classifier = "jdk15",
            group = "net.sf.json-lib",
            version = "2.4",
        ),
    ],
    boms = [
        "org.seleniumhq.selenium:selenium-bom:4.14.1",
    ],
    maven_install_json = "@rules_jvm_external//tests/custom_maven_install:maven_resolved_install.json",
    repositories = [
        "https://repo.spring.io/plugins-release/",  # Requires auth, but we don't have it
        "https://repo1.maven.org/maven2",
    ],
    resolver = "maven",
)

load("@maven_resolved_with_boms//:defs.bzl", _maven_resolved_maven_install = "pinned_maven_install")

_maven_resolved_maven_install()

# https://github.com/bazelbuild/rules_jvm_external/issues/1206
maven_install(
    name = "transitive_dependency_with_type_of_pom",
    # an arbitrary artifact which depends on org.javamoney:moneta:pom
    artifacts = [
        # https://github.com/quarkiverse/quarkus-moneta/blob/2.0.0/runtime/pom.xml#L16-L21
        "io.quarkiverse.moneta:quarkus-moneta:2.0.0",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

# This failure mode is bzlmod only. But the test still runs on Bazel 5/6, which
# is WORKSPACE based, so we add a shim here to keep the test passing until
# WORKSPACE support is no longer needed.
maven_install(
    name = "root_module_can_override",
    artifacts = [
        "com.squareup:javapoet:1.11.1",
        "com.squareup.okhttp3:okhttp:4.12.0",
    ],
    override_targets = {
        "com.squareup.okhttp3:okhttp": "@root_module_can_override//:com_squareup_javapoet",
    },
    repositories = ["https://repo1.maven.org/maven2"],
)

# This is bzlmod only. But the test still runs on Bazel 5/6, which
# is WORKSPACE based, so we add a shim here to keep the test passing until
# WORKSPACE support is no longer needed.
maven_install(
    name = "from_files",
    artifacts = [
        "org.junit.jupiter:junit-jupiter-api:5.12.2",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "pom_exclusion_testing_coursier",
    artifacts = [
        maven.artifact(
            artifact = "guava",
            exclusions = [
                maven.exclusion(
                    artifact = "error_prone_annotations",
                    group = "com.google.errorprone",
                ),
            ],
            group = "com.google.guava",
            version = "31.1-jre",
        ),
    ],
    excluded_artifacts = [
        "log4j:log4j",
    ],
    maven_install_json = "//tests/integration/pom_file:pom_exclusion_testing_coursier_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    resolver = "coursier",
)

maven_install(
    name = "pom_exclusion_testing_maven",
    artifacts = [
        maven.artifact(
            artifact = "guava",
            exclusions = [
                maven.exclusion(
                    artifact = "error_prone_annotations",
                    group = "com.google.errorprone",
                ),
            ],
            group = "com.google.guava",
            version = "31.1-jre",
        ),
    ],
    excluded_artifacts = [
        "log4j:log4j",
    ],
    maven_install_json = "//tests/integration/pom_file:pom_exclusion_testing_maven_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    resolver = "maven",
)

maven_install(
    name = "pom_exclusion_testing_gradle",
    artifacts = [
        maven.artifact(
            artifact = "guava",
            exclusions = [
                maven.exclusion(
                    artifact = "error_prone_annotations",
                    group = "com.google.errorprone",
                ),
            ],
            group = "com.google.guava",
            version = "31.1-jre",
        ),
    ],
    excluded_artifacts = [
        "log4j:log4j",
    ],
    maven_install_json = "//tests/integration/pom_file:pom_exclusion_testing_gradle_install.json",
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    resolver = "gradle",
)

maven_install(
    name = "amend_artifacts",
    artifacts = [
        maven.artifact(
            testonly = False,
            artifact = "guava",
            group = "com.google.guava",
            version = "31.1-jre",
        ),
        maven.artifact(
            testonly = True,
            artifact = "commons-lang3",
            group = "org.apache.commons",
            version = "3.12.0",
        ),
        maven.artifact(
            artifact = "slf4j-api",
            group = "org.slf4j",
            neverlink = True,
            version = "1.7.32",
        ),
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)
