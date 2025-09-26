package main

var dWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@io_bazel_rules_d//d:d.bzl", "d_repositories")

d_repositories()`)

var dProtoLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir}}:d_proto_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_d//d:d.bzl", "d_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Create {{ .Lang.Name }} library
    d_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@com_github_dcarp_protobuf_d//:protobuf",
]`)


func makeD() *Language {
	return &Language{
		Dir:   "d",
		Name:  "d",
		DisplayName: "D",
		Notes: mustTemplate("Rules for generating D protobuf ``.d`` files and libraries using `protobuf-d <https://github.com/dcarp/protobuf-d>`_. Libraries are created with ``d_library`` from `rules_d <https://github.com/bazelbuild/rules_d>`_" + `

.. note:: These rules use the protoc-gen-d plugin, which only supports proto3 .proto files.`),
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{"windows", "macos"},
		Rules: []*Rule{
			&Rule{
				Name:             "d_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//d:d_plugin"},
				WorkspaceExample: dWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates D protobuf ``.d`` files",
				Attrs:            compileRuleAttrs,
			},
// 			&Rule{
// 				Name:             "d_grpc_compile",
// 				Kind:             "grpc",
// 				Implementation:   compileRuleTemplate,
// 				Plugins:          []string{"//d:grpc_d"}, # TODO: Try https://github.com/huntlabs/grpc-dlang
// 				WorkspaceExample: dWorkspaceTemplate,
// 				BuildExample:     grpcCompileExampleTemplate,
// 				Doc:              "Generates D protobuf and gRPC ``.d`` files",
// 				Attrs:            libraryRuleAttrs,
// 			},
			&Rule{
				Name:             "d_proto_library",
				Kind:             "proto",
				Implementation:   dProtoLibraryRuleTemplate,
				WorkspaceExample: dWorkspaceTemplate,
				BuildExample:     protoLibraryExampleTemplate,
				Doc:              "Generates a D protobuf library using ``d_library`` from ``rules_d``",
				Attrs:            compileRuleAttrs,
			},
// 			&Rule{
// 				Name:             "d_grpc_library",
// 				Kind:             "grpc",
// 				Implementation:   dGrpcLibraryRuleTemplate,
// 				WorkspaceExample: dWorkspaceTemplate,
// 				BuildExample:     grpcLibraryExampleTemplate,
// 				Doc:              "Generates a D protobuf and gRPC library using ``d_library`` from ``rules_d``",
// 				Attrs:            libraryRuleAttrs,
// 			},
		},
	}
}
