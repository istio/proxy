package main

var csharpProtoWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

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

load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages()`)

var csharpGrpcWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

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

load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")

nuget_rules_proto_grpc_packages()`)

var csharpLibraryRuleTemplateString = `load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_dotnet//dotnet:defs.bzl", "csharp_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )
`

var csharpProtoLibraryRuleTemplate = mustTemplate(csharpLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    csharp_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@google.protobuf//:lib",
    "@core_sdk_stdlib//:libraryset",
]`)

var csharpGrpcLibraryRuleTemplate = mustTemplate(csharpLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    csharp_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@google.protobuf//:lib",
    "@grpc.net.client//:lib",
    "@grpc.aspnetcore//:lib",
    "@core_sdk_stdlib//:libraryset",
]`)

// For C#, library names need .dll
var csharpProtoLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

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


var csharpGrpcLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll",
    protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
    deps = ["thing_{{ .Lang.Name }}_{{ .Rule.Kind }}.dll"],
)`)

func makeCsharp() *Language {
	return &Language{
		Dir:   "csharp",
		Name:  "csharp",
		DisplayName: "C#",
		Flags: commonLangFlags,
		Notes: mustTemplate("Rules for generating C# protobuf and gRPC ``.cs`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``csharp_library`` from `rules_dotnet <https://github.com/bazelbuild/rules_dotnet>`_"),
		Rules: []*Rule{
			&Rule{
				Name:             "csharp_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//csharp:csharp_plugin"},
				WorkspaceExample: csharpProtoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates C# protobuf ``.cs`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "csharp_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//csharp:csharp_plugin", "//csharp:grpc_csharp_plugin"},
				WorkspaceExample: csharpGrpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates C# protobuf and gRPC ``.cs`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "csharp_proto_library",
				Kind:             "proto",
				Implementation:   csharpProtoLibraryRuleTemplate,
				WorkspaceExample: csharpProtoWorkspaceTemplate,
				BuildExample:     csharpProtoLibraryExampleTemplate,
				Doc:              "Generates a C# protobuf library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``",
				Attrs:            libraryRuleAttrs,
			},
			&Rule{
				Name:             "csharp_grpc_library",
				Kind:             "grpc",
				Implementation:   csharpGrpcLibraryRuleTemplate,
				WorkspaceExample: csharpGrpcWorkspaceTemplate,
				BuildExample:     csharpGrpcLibraryExampleTemplate,
				Doc:              "Generates a C# protobuf and gRPC library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``",
				Attrs:            libraryRuleAttrs,
			},
		},
	}
}
