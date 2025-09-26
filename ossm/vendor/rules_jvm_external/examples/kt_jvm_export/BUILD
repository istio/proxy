load("@rules_jvm_external//:kt_defs.bzl", "kt_jvm_export")

# To export the file, run:
#
# `bazel run //:example-export.publish --define "maven_repo=file://$(pwd)/repository"`
#
# GPG signing will be enabled if you also define `gpg_sign=true`, `maven_user`, and
# `maven_password`

kt_jvm_export(
    name = "example-export",
    maven_coordinates = "com.example:bazel-example:0.0.1",
    runtime_deps = [
        "//src/main/kotlin/com/github/bazelbuild/rulesjvmexternal/example/export",
    ],
)
