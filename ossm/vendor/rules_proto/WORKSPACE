workspace(name = "rules_proto")

load("//proto:repositories.bzl", "rules_proto_dependencies")

rules_proto_dependencies()

load("//proto:setup.bzl", "rules_proto_setup")

rules_proto_setup()

load("//proto:toolchains.bzl", "rules_proto_toolchains")

rules_proto_toolchains()

# Dev-only dependencies. USERS SHOULD NOT INSTALL THESE!
load(":dev_deps.bzl", "rules_proto_dev_deps")

rules_proto_dev_deps()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

load("@bazelci_rules//:rbe_repo.bzl", "rbe_preconfig")

rbe_preconfig(
    name = "buildkite_config",
    toolchain = "ubuntu1804-bazel-java11",
)

load("@io_bazel_stardoc//:setup.bzl", "stardoc_repositories")

stardoc_repositories()

load("@rules_java//java:rules_java_deps.bzl", "rules_java_dependencies")

rules_java_dependencies()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@io_bazel_stardoc//:deps.bzl", "stardoc_external_deps")

stardoc_external_deps()

load("@stardoc_maven//:defs.bzl", stardoc_pinned_maven_install = "pinned_maven_install")

stardoc_pinned_maven_install()
