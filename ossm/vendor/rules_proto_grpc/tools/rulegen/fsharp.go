package main

var fsharpProtoWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")

dotnet_repositories()

load(
    "@io_bazel_rules_dotnet//dotnet:defs.bzl",
    "dotnet_register_toolchains",
    "dotnet_repositories_nugets",
)

dotnet_register_toolchains()

dotnet_repositories_nugets()

load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages()`)

var fsharpGrpcWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

dotnet_repositories()

load(
    "@io_bazel_rules_dotnet//dotnet:defs.bzl",
    "dotnet_register_toolchains",
    "dotnet_repositories_nugets",
)

dotnet_register_toolchains()

dotnet_repositories_nugets()

load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages()`)

var fsharpLibraryRuleTemplateString = `load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_dotnet//dotnet:defs.bzl", "fsharp_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )
`

var fsharpProtoLibraryRuleTemplate = mustTemplate(fsharpLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    fsharp_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@google.protobuf//:lib",
    "@fsharp.core//:lib",
    "@protobuf.fsharp//:lib",
    "@core_sdk_stdlib//:libraryset",
]`)

var fsharpGrpcLibraryRuleTemplate = mustTemplate(fsharpLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    fsharp_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@google.protobuf//:lib",
    "@grpc.net.client//:lib",
    "@grpc.aspnetcore//:lib",
    "@protobuf.fsharp//:lib",
    "@core_sdk_stdlib//:libraryset",
]`)

// For F#, library names need .dll
var fsharpProtoLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:person_proto"],
    deps = ["place_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll"],
)

{{ .Rule.Name }}(
    name = "place_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:place_proto"],
    deps = ["thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll"],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)`)

var fsharpGrpcLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
    deps = ["thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll"],
)`)

func makeFsharp() *Language {
	return &Language{
		Dir:         "fsharp",
		Name:        "fsharp",
		DisplayName: "F#",
		Flags:       commonLangFlags,
		Notes:       mustTemplate("Rules for generating F# protobuf and gRPC ``.fs`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``fsharp_library`` from `rules_dotnet <https://github.com/bazelbuild/rules_dotnet>`_"),
		Rules: []*Rule{
			&Rule{
				Name:             "fsharp_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//fsharp:fsharp_plugin"},
				WorkspaceExample: fsharpProtoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates F# protobuf ``.fs`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "fsharp_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//fsharp:grpc_fsharp_plugin"},
				WorkspaceExample: fsharpGrpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates F# protobuf and gRPC ``.fs`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "fsharp_proto_library",
				Kind:             "proto",
				Implementation:   fsharpProtoLibraryRuleTemplate,
				WorkspaceExample: fsharpProtoWorkspaceTemplate,
				BuildExample:     fsharpProtoLibraryExampleTemplate,
				Doc:              "Generates a F# protobuf library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``",
				Attrs:            libraryRuleAttrs,
			},
			&Rule{
				Name:             "fsharp_grpc_library",
				Kind:             "grpc",
				Implementation:   fsharpGrpcLibraryRuleTemplate,
				WorkspaceExample: fsharpGrpcWorkspaceTemplate,
				BuildExample:     fsharpGrpcLibraryExampleTemplate,
				Doc:              "Generates a F# protobuf and gRPC library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``",
				Attrs:            libraryRuleAttrs,
			},
		},
	}
}
