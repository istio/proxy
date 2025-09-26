load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@rules_java//java:defs.bzl", "java_library")
load("//:defs.bzl", "artifact", "java_export")
load("//private/rules:maven_project_jar.bzl", "maven_project_jar")

java_test(
    name = "PublishShapeTest",
    srcs = ["PublishShapeTest.java"],
    data = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:MavenPublisher_deploy.jar",
    ],
    jvm_flags = [
        "-Ddeploy.jar=$(location //private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:MavenPublisher_deploy.jar)",
    ],
    test_class = "com.jvm.external.jvm_export.PublishShapeTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:MavenPublisher_deploy.jar",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_test(
    name = "ProjectJarTest",
    srcs = ["ProjectJarTest.java"],
    data = [
        ":project",
    ],
    jvm_flags = [
        "-Dlocation=\"$(location :project)\"",
    ],
    test_class = "com.jvm.external.jvm_export.ProjectJarTest",
    deps = [
        ":project",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)

maven_project_jar(
    name = "project",
    target = ":main",
)

java_library(
    name = "main",
    srcs = ["Main.java"],
    tags = [
        "maven_coordinates=com.example:example:0.0.1",
    ],
    deps = [":dep"],
)

java_library(
    name = "dep",
    srcs = ["Dependency.java"],
    deps = [
        artifact("com.google.guava:guava"),
    ],
)

java_export(
    name = "deploy-env",
    srcs = [
        "Main.java",
    ],
    deploy_env = [
        ":dep",
    ],
    javadocopts = [
        "-windowtitle",
        "My Deploy Env",
    ],
    maven_coordinates = "com.example:lib:1.0.0",
    deps = [
        ":dep",
    ],
)

java_export(
    name = "without-docs",
    srcs = ["Main.java"],
    maven_coordinates = "com.example:no-docs:1.0.0",
    tags = [
        "no-javadocs",
    ],
    deps = [
        ":dep",
    ],
)

java_export(
    name = "with-manifest",
    srcs = ["Main.java"],
    manifest_entries = {
        "Cheese": "cheddar",
        "Second-Cheese": "brie",
    },
    maven_coordinates = "com.example:manifested:1.0.0",
    deps = [
        ":dep",
    ],
)

genrule(
    name = "list-deploy-env-classes",
    srcs = [
        ":deploy-env-project",
    ],
    outs = ["classes.txt"],
    cmd = "for SRC in $(SRCS); do jar tf $$SRC >> $@; done",
)

sh_test(
    name = "check-deploy-env",
    srcs = [
        "check-deploy-env.sh",
    ],
    data = [
        ":classes.txt",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)

filegroup(
    name = "deploy-env-sources",
    srcs = [":deploy-env-project"],
    output_group = "maven_source",
)

genrule(
    name = "list-deploy-env-sources",
    srcs = [
        ":deploy-env-sources",
    ],
    outs = ["sources.txt"],
    cmd = "for SRC in $(SRCS); do jar tf $$SRC >> $@; done",
)

sh_test(
    name = "check-deploy-env-sources",
    srcs = [
        "check-deploy-env-sources.sh",
    ],
    data = [
        ":sources.txt",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)

proto_library(
    name = "example_proto",
    srcs = ["example.proto"],
    deps = [
        # It's important that we pull our dep from `@com_google_protobuf`
        "@protobuf//:wrappers_proto",
    ],
)

java_proto_library(
    name = "example-proto-library",
    deps = [":example_proto"],
)

java_export(
    name = "with-proto-dep",
    maven_coordinates = "com.example:example-with-proto:0.0.1",
    tags = [
        # Because we know that there's a dep on protobuf, but we've not
        # told bazel where to find it. Code will compile, but the javadoc
        # generation will fail because it wants more! More! More!
        "no-javadocs",
    ],
    runtime_deps = [":example-proto-library"],
)

java_export(
    name = "with-added-dependency",
    excluded_workspaces = {
        "protobuf": artifact(
            "com.google.protobuf:protobuf-java",
            repository_name = "java_export_exclusion_testing",
        ),
    },
    maven_coordinates = "com.example:example-with-proto:0.0.1",
    runtime_deps = [":example-proto-library"],
)

sh_test(
    name = "check-excluded-packages-are-excluded",
    srcs = [
        "check-excluded-packages-are-excluded.sh",
    ],
    data = [
        ":with-added-dependency-pom",
        ":with-proto-dep",
        ":with-proto-dep-pom",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)

genrule(
    name = "extract-extra-manifest-lines",
    srcs = [
        ":with-manifest",
    ],
    outs = [
        "manifested_MANIFEST.MF",
    ],
    cmd = "$(location //tests/com/github/bazelbuild/rules_jvm_external/manifest:Print) $< | grep -E 'Cheese' | sort > $@",
    toolchains = ["@bazel_tools//tools/jdk:current_host_java_runtime"],
    tools = [
        "//tests/com/github/bazelbuild/rules_jvm_external/manifest:Print",
    ],
)

diff_test(
    name = "check-manifest-lines-added",
    file1 = "check-manifest-lines-added-MANIFEST.MF.golden",
    file2 = ":extract-extra-manifest-lines",
)
