package main

var grpcGatewayWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load

io_bazel_rules_go()

bazel_gazelle()

load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_gateway_repos = "gateway_repos")

rules_proto_grpc_gateway_repos()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(
    version = "1.17.1",
)

load("@com_github_grpc_ecosystem_grpc_gateway_v2//:repositories.bzl", "go_repositories")

go_repositories()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies()`)

var grpcGatewayLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:gateway_grpc_compile.bzl", "gateway_grpc_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
load("//go:go_grpc_library.bzl", "GRPC_DEPS")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    gateway_{{ .Rule.Kind }}_compile(
        name = name_pb,
        prefix_path = kwargs.get("prefix_path", kwargs.get("importpath", "")),
        **{
            k: v
            for (k, v) in kwargs.items()
            if (k in proto_compile_attrs.keys() and k != "prefix_path") or
               k in bazel_build_rule_common_attrs
        }  # Forward args
    )

    # Create go library
    go_library(
        name = name,
        srcs = [name_pb],
        deps = kwargs.get("go_deps", []) + GATEWAY_DEPS + GRPC_DEPS + kwargs.get("deps", []),
        importpath = kwargs.get("importpath"),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GATEWAY_DEPS = [
    "@org_golang_google_protobuf//proto:go_default_library",
    "@org_golang_google_grpc//grpclog:go_default_library",
    "@org_golang_google_grpc//metadata:go_default_library",
    "@com_github_grpc_ecosystem_grpc_gateway_v2//runtime:go_default_library",
    "@com_github_grpc_ecosystem_grpc_gateway_v2//utilities:go_default_library",
    "@go_googleapis//google/api:annotations_go_proto",
]`)

var grpcGatewayCompileExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "api_gateway_grpc",
    protos = ["@rules_proto_grpc//{{ .Lang.Dir }}/example/api:api_proto"],
)`)

var grpcGatewayLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "api_gateway_library",
    importpath = "github.com/rules-proto-grpc/rules_proto_grpc/grpc-gateway/examples/api",
    protos = ["@rules_proto_grpc//{{ .Lang.Dir }}/example/api:api_proto"],
)`)

func makeGrpcGateway() *Language {
	return &Language{
		Dir:         "grpc-gateway",
		Name:        "grpc-gateway",
		DisplayName: "grpc-gateway",
		Flags:       commonLangFlags,
		Rules: []*Rule{
			&Rule{
				Name:             "gateway_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//grpc-gateway:grpc_gateway_plugin", "//go:grpc_go_plugin", "//go:go_plugin"},
				WorkspaceExample: grpcGatewayWorkspaceTemplate,
				BuildExample:     grpcGatewayCompileExampleTemplate,
				Doc:              "Generates grpc-gateway ``.go`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:              "gateway_openapiv2_compile",
				Kind:              "grpc",
				Implementation:    compileRuleTemplate,
				Plugins:           []string{"//grpc-gateway:openapiv2_plugin"},
				WorkspaceExample:  grpcGatewayWorkspaceTemplate,
				BuildExample:      grpcGatewayCompileExampleTemplate,
				Doc:               "Generates grpc-gateway OpenAPI v2 ``.json`` files",
				Attrs:             compileRuleAttrs,
				SkipTestPlatforms: []string{"windows"}, // gRPC go lib rules fail on windows due to bad path
			},
			&Rule{
				Name:              "gateway_grpc_library",
				Kind:              "grpc",
				Implementation:    grpcGatewayLibraryRuleTemplate,
				WorkspaceExample:  grpcGatewayWorkspaceTemplate,
				BuildExample:      grpcGatewayLibraryExampleTemplate,
				Doc:               "Generates grpc-gateway library files",
				Attrs:             goLibraryRuleAttrs,
				SkipTestPlatforms: []string{"windows"}, // gRPC go lib rules fail on windows due to bad path
			},
		},
	}
}
