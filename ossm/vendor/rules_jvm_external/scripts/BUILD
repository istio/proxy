load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("//private:versions.bzl", "COURSIER_CLI_HTTP_FILE_NAME")
load("//private/rules:artifact.bzl", "artifact")

genrule(
    name = "buildifier-bin",
    srcs = select({
        "@bazel_tools//src/conditions:darwin_arm64": ["@buildifier-macos-arm64//file"],
        "@bazel_tools//src/conditions:darwin_x86_64": ["@buildifier-macos-x86_64//file"],
        "@bazel_tools//src/conditions:linux_aarch64": ["@buildifier-linux-arm64//file"],
        "@bazel_tools//src/conditions:linux_x86_64": ["@buildifier-linux-x86_64//file"],
    }),
    outs = ["buildifier"],
    cmd = "cp $< $@",
)

sh_binary(
    name = "mirror_coursier",
    srcs = [":mirror_coursier.sh"],
    args = [COURSIER_CLI_HTTP_FILE_NAME],
    data = ["@coursier_cli//file"],
    visibility = ["//:__pkg__"],
)

genrule(
    name = "generate_api_reference",
    srcs = [
        "//docs:includes/main_functions_header.md",
        "//docs:defs.md",
        "//docs:includes/spec_functions_header.md",
        "//docs:specs.md",
    ],
    outs = ["api.md"],
    cmd = """cat \
    $(location //docs:includes/main_functions_header.md) \
    $(location //docs:defs.md) \
    $(location //docs:includes/spec_functions_header.md) \
    $(location //docs:specs.md) > $@""",
    visibility = ["//:__pkg__"],
)

java_binary(
    name = "google-java-format",
    jvm_flags = [
        "--add-exports jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED",
        "--add-exports jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED",
        "--add-exports jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED",
        "--add-exports jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED",
        "--add-exports jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED",
    ],
    main_class = "com.google.googlejavaformat.java.Main",
    runtime_deps = [
        artifact(
            "com.google.googlejavaformat:google-java-format",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

genrule(
    name = "format-script",
    outs = ["run-formats.sh"],
    cmd = """cat >$@ << EOL
#!/usr/bin/env bash
set -eufo pipefail

cwd="\\$${BUILD_WORKSPACE_DIRECTORY:=\\$$(pwd)}"
javaformat="\\$$1"
buildifier="\\$$2"

# Format java code
find "\\$$cwd" -type f -name "*.java" |  xargs "\\$${javaformat}" --replace

# Run buildifier
\\$${buildifier} --lint fix --mode fix -r "\\$$cwd"

EOL
""",
    executable = True,
)

sh_binary(
    name = "format",
    srcs = [":format-script"],
    args = [
        "$(location :google-java-format)",
        "$(location :buildifier)",
    ],
    data = [
        ":buildifier",
        ":google-java-format",
    ],
)

sh_binary(
    name = "refresh-prebuilts",
    srcs = [
        "copy-prebuilts.sh",
    ],
    args = [
        "private/tools/prebuilt",
        "$(location //private/tools/java/com/github/bazelbuild/rules_jvm_external:hasher-tool_deploy.jar)",
        "hasher_deploy.jar",
        "$(location //private/tools/java/com/github/bazelbuild/rules_jvm_external/coursier:LockFileConverter_deploy.jar)",
        "lock_file_converter_deploy.jar",
        "$(location //private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:IndexJar_deploy.jar)",
        "index_jar_deploy.jar",
        "$(location //private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:outdated_deploy.jar)",
        "outdated_deploy.jar",
    ],
    data = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external:hasher-tool_deploy.jar",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/coursier:LockFileConverter_deploy.jar",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:IndexJar_deploy.jar",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:outdated_deploy.jar",
    ],
)
