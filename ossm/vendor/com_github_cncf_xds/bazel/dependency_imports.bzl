load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
load("@com_envoyproxy_protoc_gen_validate//bazel:repositories.bzl", "pgv_dependencies")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

# go version for rules_go
GO_VERSION = "1.20.2"

def xds_dependency_imports(go_version = GO_VERSION):
    rules_proto_dependencies()
    rules_proto_toolchains()
    protobuf_deps()
    go_rules_dependencies()
    go_register_toolchains(go_version = go_version)
    gazelle_dependencies()
    pgv_dependencies()

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
        name = "com_github_iancoleman_strcase",
        importpath = "github.com/iancoleman/strcase",
        sum = "h1:nTXanmYxhfFAMjZL34Ov6gkzEsSJZ5DbhxWjvSASxEI=",
        version = "v0.3.0",
        build_external = "external",
        # project_url = "https://pkg.go.dev/github.com/iancoleman/strcase",
        # last_update = "2020-11-22"
        # use_category = ["api"],
        # source = "https://github.com/bufbuild/protoc-gen-validate/blob/v1.0.4/dependencies.bzl#L170-L175"
    )
    go_repository(
        name = "org_golang_x_net",
        importpath = "golang.org/x/net",
        sum = "h1:aCL9BSgETF1k+blQaYUBx9hJ9LOGP3gAVemcZlf1Kpo=",
        version = "v0.20.0",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_x_text",
        importpath = "golang.org/x/text",
        sum = "h1:ScX5w1eTa3QqT8oi6+ziP7dTV1S2+ALU0bI+0zXKWiQ=",
        version = "v0.14.0",
        build_external = "external",
    )
    go_repository(
        name = "com_github_spf13_afero",
        importpath = "github.com/spf13/afero",
        sum = "h1:EaGW2JJh15aKOejeuJ+wpFSHnbd7GE6Wvp3TsNhb6LY=",
        version = "v1.10.0",
        build_external = "external",
        # project_url = "https://pkg.go.dev/github.com/spf13/afero",
        # last_update = "2021-03-20"
        # use_category = ["api"],
        # source = "https://github.com/bufbuild/protoc-gen-validate/blob/v1.0.4/dependencies.bzl#L257-L262"
    )
    go_repository(
        name = "com_github_lyft_protoc_gen_star_v2",
        importpath = "github.com/lyft/protoc-gen-star/v2",
        sum = "h1:/3+/2sWyXeMLzKd1bX+ixWKgEMsULrIivpDsuaF441o=",
        version = "v2.0.3",
        build_external = "external",
        # project_url = "https://pkg.go.dev/github.com/lyft/protoc-gen-star",
        # last_update = "2023-01-06"
        # use_category = ["api"],
        # source = "https://github.com/bufbuild/protoc-gen-validate/blob/v1.0.4/dependencies.bzl#L220-L225"
    )
    go_repository(
        name = "org_golang_google_genproto_googleapis_api",
        importpath = "google.golang.org/genproto/googleapis/api",
        sum = "h1:DoPTO70H+bcDXcd39vOqb2viZxgqeBeSGtZ55yZU4/Q=",
        version = "v0.0.0-20230822172742-b8732ec3820d",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_genproto_googleapis_rpc",
        importpath = "google.golang.org/genproto/googleapis/rpc",
        sum = "h1:uvYuEyMHKNt+lT4K3bN6fGswmK8qSvcreM3BwjDh+y4=",
        version = "v0.0.0-20230822172742-b8732ec3820d",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_protobuf",
        importpath = "google.golang.org/protobuf",
        sum = "h1:pPC6BG5ex8PDFnkbrGU3EixyhKcQ2aDuBS36lqK/C7I=",
        version = "v1.32.0",
        build_external = "external",
    )
    go_repository(
        name = "org_golang_google_grpc",
        build_file_proto_mode = "disable",
        importpath = "google.golang.org/grpc",
        sum = "h1:Z5Iec2pjwb+LEOqzpB2MR12/eKFhDPhuqW91O+4bwUk=",
        version = "v1.59.0",
    )

# Old name for backward compatibility.
# TODO(roth): Remove this once callers are migrated to the new name.
def udpa_dependency_imports(go_version = GO_VERSION):
    xds_dependency_imports(go_version = go_version)
