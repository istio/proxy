load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@rules_java//java:repositories.bzl", "rules_java_dependencies", "rules_java_toolchains")
load("@rules_jvm_external_deps//:defs.bzl", "pinned_maven_install")

def rules_jvm_external_setup():
    bazel_skylib_workspace()

    bazel_features_deps()

    # When using bazel 5, we have undefined toolchains from rules_js. This should be fine to skip, since we only need
    # it for the `JavaInfo` definition.
    major_version = native.bazel_version.partition(".")[0]
    if major_version != "5":
        rules_java_dependencies()
        rules_java_toolchains()
    pinned_maven_install()
