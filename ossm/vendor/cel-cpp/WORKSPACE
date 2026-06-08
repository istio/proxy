workspace(name = "com_google_cel_cpp")

load("//bazel:deps.bzl", "cel_cpp_deps", "cel_cpp_extensions_deps")

cel_cpp_deps()

cel_cpp_extensions_deps()

load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")

rules_cc_dependencies()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies")

rules_proto_dependencies()

load("@rules_proto//proto:setup.bzl", "rules_proto_setup")

rules_proto_setup()

load("@rules_proto//proto:toolchains.bzl", "rules_proto_toolchains")

rules_proto_toolchains()

load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

load("//bazel:deps_extra.bzl", "cel_cpp_deps_extra")

cel_cpp_deps_extra()
