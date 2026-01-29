workspace(name = "rules_proto_grpc")

load("//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")

#
# Toolchains
#

rules_proto_grpc_toolchains()

#
# Core
#

rules_proto_grpc_repos()

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

#
# Android
#
# Deferred until after Go and C++

#
# Buf
#
load("//buf:repositories.bzl", "buf_repos")

buf_repos()

#
# Go
#
# Load rules_go before running grpc_deps in C++, since that depends on a very old version of
# rules_go
#
load("//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load

io_bazel_rules_go()

bazel_gazelle()

load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

load("//go:repositories.bzl", "go_repos")

go_repos()

#
# Swift
#
# Load build_bazel_rules_swift before running grpc_deps in C++, since that depends on a very old
# version of build_bazel_apple_support
#
load("//swift:repositories.bzl", "swift_repos")

swift_repos()

load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)

swift_rules_dependencies()

#
# C++
#
load("//cpp:repositories.bzl", "cpp_repos")

cpp_repos()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

#
# Android
#
load("//android:repositories.bzl", "android_repos")

android_repos()

load("@rules_jvm_external//:defs.bzl", "maven_install")
load("@io_grpc_grpc_java//:repositories.bzl", "IO_GRPC_GRPC_JAVA_ARTIFACTS", "IO_GRPC_GRPC_JAVA_OVERRIDE_TARGETS", "grpc_java_repositories")

maven_install(
    artifacts = IO_GRPC_GRPC_JAVA_ARTIFACTS,
    generate_compat_repositories = True,
    override_targets = IO_GRPC_GRPC_JAVA_OVERRIDE_TARGETS,
    repositories = [
        "https://repo.maven.apache.org/maven2/",
    ],
)

load("@maven//:compat.bzl", "compat_repositories")

compat_repositories()

grpc_java_repositories()

load("@build_bazel_rules_android//android:sdk_repository.bzl", "android_sdk_repository")

android_sdk_repository(name = "androidsdk")

#
# C#/F#
#
load("//csharp:repositories.bzl", "csharp_repos")

csharp_repos()

load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")

dotnet_repositories()

load(
    "@io_bazel_rules_dotnet//dotnet:defs.bzl",
    "dotnet_register_toolchains",
    "dotnet_repositories_nugets",
)

dotnet_register_toolchains()

dotnet_repositories_nugets()

load("@rules_proto_grpc//csharp/nuget:nuget.bzl", nuget_rules_proto_grpc_packages_csharp = "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages_csharp()

load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", nuget_rules_proto_grpc_packages_fsharp = "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages_fsharp()

#
# D
#
load("//d:repositories.bzl", "d_repos")

d_repos()

load("@io_bazel_rules_d//d:d.bzl", "d_repositories")

d_repositories()

#
# Doc
#
load("//doc:repositories.bzl", "doc_repos")

doc_repos()

#
# Go
#
# Moved to above C++
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies()

#
# gRPC gateway
#
# Uses same dependencies as Go.

load("//grpc-gateway:repositories.bzl", "gateway_repos")

gateway_repos()

load("@com_github_grpc_ecosystem_grpc_gateway_v2//:repositories.bzl", "go_repositories")

go_repositories()

#
# Java
#
load("//java:repositories.bzl", "java_repos")

java_repos()

# grpc_java_repositories already called above in android

#
# JavaScript
#
load("//js:repositories.bzl", "js_repos")

js_repos()

load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")

build_bazel_rules_nodejs_dependencies()

load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")

yarn_install(
    name = "npm",
    package_json = "@rules_proto_grpc//js:requirements/package.json",
    yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
)

#
# Objective-C
#
load("//objc:repositories.bzl", "objc_repos")

objc_repos()

#
# PHP
#
load("//php:repositories.bzl", "php_repos")

php_repos()

#
# Python
#
load("//python:repositories.bzl", "python_repos")

python_repos()

load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "rules_proto_grpc_py3_deps",
    python_interpreter = "python3",
    requirements_lock = "@rules_proto_grpc//python:requirements.txt",
)

load("@rules_proto_grpc_py3_deps//:requirements.bzl", "install_deps")

install_deps()

#
# Ruby
#
load("//ruby:repositories.bzl", "ruby_repos")

ruby_repos()

load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")

rules_ruby_dependencies()

rules_ruby_select_sdk(version = "3.1.1")

load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")

ruby_bundle(
    name = "rules_proto_grpc_bundle",
    gemfile = "@rules_proto_grpc//ruby:Gemfile",
    gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
    includes = {"grpc": ["etc"]},
)

#
# Rust
#
load("//rust:repositories.bzl", "rust_repos")

rust_repos()

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

rules_rust_dependencies()

rust_register_toolchains(edition = "2021")

load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")

crate_universe_dependencies(bootstrap = True)

load("//rust:crate_deps.bzl", "crate_repositories")

crate_repositories()

#
# Scala
#
load("//scala:repositories.bzl", "MAVEN_ARTIFACTS", "scala_repos")

scala_repos()

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_config()

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

maven_install(
    name = "rules_proto_grpc_scala_maven",
    artifacts = MAVEN_ARTIFACTS,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

#
# Swift
#
# Moved to above C++

#
# Misc
#
load("@bazel_gazelle//:deps.bzl", "go_repository")  # buildifier: disable=same-origin-load

go_repository(
    name = "com_github_urfave_cli",
    commit = "44cb242eeb4d76cc813fdc69ba5c4b224677e799",
    importpath = "github.com/urfave/cli",
)
