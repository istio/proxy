load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_envoyproxy_protoc_gen_validate//bazel:repositories.bzl", "pgv_dependencies")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@rules_python//python:repositories.bzl", "py_repositories", "python_register_toolchains")

# go version for rules_go
GO_VERSION = "1.24.6"

# Python version for rules_python
PYTHON_VERSION = "3.12"

def xds_dependency_imports(go_version = GO_VERSION):
    protobuf_deps()
    go_rules_dependencies()
    go_register_toolchains(go_version = go_version)
    gazelle_dependencies(go_sdk = "go_sdk")
    pgv_dependencies()

    # Initialize rules_python for WORKSPACE mode
    py_repositories()
    python_register_toolchains(
        name = "python_%s" % PYTHON_VERSION,
        python_version = PYTHON_VERSION,
    )

    # Needed for grpc's @com_github_grpc_grpc//bazel:python_rules.bzl
    # Used in place of calling grpc_deps() because it needs to be called before
    # loading `grpc_extra_deps.bzl` - which is not allowed in a method def context.
    native.bind(
        name = "protocol_compiler",
        actual = "@com_google_protobuf//:protoc",
    )

    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        go = True,
        python = True,
        grpc = True,
    )

    # These dependencies, like most of the Go in this repository, exist only for the API.
    # These repos also have transient dependencies - `build_external` allows them to use them.
    # TODO(phlax): remove `build_external` and pin all transients
    go_repository(
        name = "org_golang_x_mod",
        importpath = "golang.org/x/mod",
        sum = "h1:SernR4v+D55NyBH2QiEQrlBAnj1ECL6AGrA5+dPaMY8=",
        version = "v0.15.0",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_x_tools",
        importpath = "golang.org/x/tools",
        sum = "h1:FvmRgNOcs3kOa+T20R1uhfP9F6HgG2mfxDv1vrx1Htc=",
        version = "v0.17.0",
        # Using vendored mode to avoid having to resolve all transitive dependencies manually
        build_external = "vendored",
        build_directives = [
            "gazelle:resolve go golang.org/x/mod/semver @org_golang_x_mod//semver:go_default_library",
            "gazelle:resolve go golang.org/x/mod/module @org_golang_x_mod//module:go_default_library",
        ],
    )
    go_repository(
        name = "com_github_iancoleman_strcase",
        importpath = "github.com/iancoleman/strcase",
        sum = "h1:nTXanmYxhfFAMjZL34Ov6gkzEsSJZ5DbhxWjvSASxEI=",
        version = "v0.3.0",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:Mb7Mrk043xzHgnRM88suvJFwzVrRfHEHJEl5/71CKw0=",
        version = "v0.34.0",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:zyQAAkrwaneQ066sspRyJaG9VNi/YJ1NfzcGB3hZ/qo=",
        version = "v0.21.0",
        build_external = "external",
    )
    go_repository(
        name = "com_github_spf13_afero",
        importpath = "github.com/spf13/afero",
        sum = "h1:EaGW2JJh15aKOejeuJ+wpFSHnbd7GE6Wvp3TsNhb6LY=",
        version = "v1.10.0",
        build_external = "external",
    )
    go_repository(
        name = "com_github_lyft_protoc_gen_star_v2",
        importpath = "github.com/lyft/protoc-gen-star/v2",
        sum = "h1:sIXJOMrYnQZJu7OB7ANSF4MYri2fTEGIsRLz6LwI4xE=",
        version = "v2.0.4-0.20230330145011-496ad1ac90a4",
        build_external = "external",
        build_directives = [
            "gazelle:resolve go golang.org/x/tools/imports @org_golang_x_tools//imports:go_default_library",
        ],
    )
    go_repository(
        name = "org_golang_google_genproto_googleapis_api",
        importpath = "google.golang.org/genproto/googleapis/api",
        sum = "h1:hjSy6tcFQZ171igDaN5QHOw2n6vx40juYbC/x67CEhc=",
        version = "v0.0.0-20240903143218-8af14fe29dc1",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_genproto_googleapis_rpc",
        importpath = "google.golang.org/genproto/googleapis/rpc",
        sum = "h1:pPJltXNxVzT4pK9yD8vR9X75DaWYYmLGMsEvBfFQZzQ=",
        version = "v0.0.0-20240903143218-8af14fe29dc1",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_protobuf",
        importpath = "google.golang.org/protobuf",
        sum = "h1:AYd7cD/uASjIL6Q9LiTjz8JLcrh/88q5UObnmY3aOOE=",
        version = "v1.36.10",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable",
        importpath = "google.golang.org/grpc",
        sum = "h1:aHQeeJbo8zAkAa3pRzrVjZlbz6uSfeOXlJNQM0RAbz0=",
        version = "v1.68.0",
    )

# Old name for backward compatibility.
# TODO(roth): Remove this once callers are migrated to the new name.
def udpa_dependency_imports(go_version = GO_VERSION):
    xds_dependency_imports(go_version = go_version)
