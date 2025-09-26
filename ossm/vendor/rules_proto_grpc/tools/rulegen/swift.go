package main

var swiftWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)

swift_rules_dependencies()`)

var swiftProtoLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Create {{ .Lang.Name }} library
    swift_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        module_name = kwargs.get("module_name"),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@com_github_apple_swift_protobuf//:SwiftProtobuf",
]`)

var swiftGrpcLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Create {{ .Lang.Name }} library
    swift_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        module_name = kwargs.get("module_name"),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@com_github_apple_swift_protobuf//:SwiftProtobuf",
    "@com_github_grpc_grpc_swift//:GRPC",
]`)

// For swift, produce one library for all protos, since they are all in the same module
var swiftProtoLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "proto_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = [
        "@rules_proto_grpc//example/proto:person_proto",
        "@rules_proto_grpc//example/proto:place_proto",
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var swiftGrpcLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = [
        "@rules_proto_grpc//example/proto:greeter_grpc",
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var swiftLibraryRuleAttrs = append(append([]*Attr(nil), libraryRuleAttrs...), []*Attr{
	&Attr{
		Name:      "module_name",
		Type:      "string",
		Default:   "",
		Doc:       "The name of the Swift module being built.",
		Mandatory: false,
	},
}...)

func makeSwift() *Language {
	return &Language{
		Dir:  "swift",
		Name: "swift",
		DisplayName: "Swift",
		Notes: mustTemplate("Rules for generating Swift protobuf and gRPC ``.swift`` files and libraries using `Swift Protobuf <https://github.com/apple/swift-protobuf>`_ and `Swift gRPC <https://github.com/grpc/grpc-swift>`_"),
		PresubmitEnvVars: map[string]string{
			"CC": "clang",
		},
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{"windows", "linux"},
		Rules: []*Rule{
			&Rule{
				Name:             "swift_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//swift:swift_plugin"},
				WorkspaceExample: swiftWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates Swift protobuf ``.swift`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "swift_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//swift:swift_plugin", "//swift:grpc_swift_plugin"},
				WorkspaceExample: swiftWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates Swift protobuf and gRPC ``.swift`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "swift_proto_library",
				Kind:             "proto",
				Implementation:   swiftProtoLibraryRuleTemplate,
				WorkspaceExample: swiftWorkspaceTemplate,
				BuildExample:     swiftProtoLibraryExampleTemplate,
				Doc:              "Generates a Swift protobuf library using ``swift_library`` from ``rules_swift``",
				Attrs:            swiftLibraryRuleAttrs,
			},
			&Rule{
				Name:             "swift_grpc_library",
				Kind:             "grpc",
				Implementation:   swiftGrpcLibraryRuleTemplate,
				WorkspaceExample: swiftWorkspaceTemplate,
				BuildExample:     swiftGrpcLibraryExampleTemplate,
				Doc:              "Generates a Swift protobuf and gRPC library using ``swift_library`` from ``rules_swift``",
				Attrs:            swiftLibraryRuleAttrs,
			},
		},
	}
}
