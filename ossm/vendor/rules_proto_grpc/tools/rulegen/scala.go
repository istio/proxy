package main

var scalaProtoWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_config()

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    name = "rules_proto_grpc_scala_maven",
    artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)`)

var scalaGrpcWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_config()

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

load("@io_grpc_grpc_java//:repositories.bzl", "grpc_java_repositories")

grpc_java_repositories()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    name = "rules_proto_grpc_scala_maven",
    artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)`)

var scalaLibraryRuleTemplateString = `load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@io_bazel_rules_scala//scala:scala.bzl", "scala_library")

def {{ .Rule.Name }}(name, **kwargs):  # buildifier: disable=function-docstring
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )
`

var scalaProtoLibraryRuleTemplate = mustTemplate(scalaLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    scala_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        exports = PROTO_DEPS + kwargs.get("exports", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@rules_proto_grpc_scala_maven//:com_google_protobuf_protobuf_java",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_lenses_2_12",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_scalapb_runtime_2_12",
]`)

var scalaGrpcLibraryRuleTemplate = mustTemplate(scalaLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    scala_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        exports = GRPC_DEPS + kwargs.get("exports", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@rules_proto_grpc_scala_maven//:io_grpc_grpc_api",
    "@rules_proto_grpc_scala_maven//:io_grpc_grpc_netty",
    "@rules_proto_grpc_scala_maven//:io_grpc_grpc_protobuf",
    "@rules_proto_grpc_scala_maven//:io_grpc_grpc_stub",
    "@rules_proto_grpc_scala_maven//:com_google_protobuf_protobuf_java",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_lenses_2_12",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_scalapb_runtime_grpc_2_12",
    "@rules_proto_grpc_scala_maven//:com_thesamet_scalapb_scalapb_runtime_2_12",
]`)

func makeScala() *Language {
	return &Language{
		Dir:   "scala",
		Name:  "scala",
		DisplayName: "Scala",
		Notes: mustTemplate("Rules for generating Scala protobuf and gRPC ``.jar`` files and libraries using `ScalaPB <https://github.com/scalapb/ScalaPB>`_. Libraries are created with ``scala_library`` from `rules_scala <https://github.com/bazelbuild/rules_scala>`_"),
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{},
		Rules: []*Rule{
			&Rule{
				Name:             "scala_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//scala:scala_plugin"},
				WorkspaceExample: scalaProtoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates a Scala protobuf ``.jar`` file",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "scala_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//scala:grpc_scala_plugin"},
				WorkspaceExample: scalaGrpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates Scala protobuf and gRPC ``.jar`` file",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "scala_proto_library",
				Kind:             "proto",
				Implementation:   scalaProtoLibraryRuleTemplate,
				WorkspaceExample: scalaProtoWorkspaceTemplate,
				BuildExample:     protoLibraryExampleTemplate,
				Doc:              "Generates a Scala protobuf library using ``scala_library`` from ``rules_scala``",
				Attrs:            javaLibraryRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "scala_grpc_library",
				Kind:             "grpc",
				Implementation:   scalaGrpcLibraryRuleTemplate,
				WorkspaceExample: scalaGrpcWorkspaceTemplate,
				BuildExample:     grpcLibraryExampleTemplate,
				Doc:              "Generates a Scala protobuf and gRPC library using ``scala_library`` from ``rules_scala``",
				Attrs:            javaLibraryRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
		},
	}
}
