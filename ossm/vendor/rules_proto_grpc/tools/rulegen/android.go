package main

var androidLibraryWorkspaceTemplateString = `load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

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

android_sdk_repository(name = "androidsdk")`

var androidGrpcLibraryWorkspaceTemplate = mustTemplate(androidLibraryWorkspaceTemplateString)

var androidProtoLibraryWorkspaceTemplate = mustTemplate("# The set of dependencies loaded here is excessive for android proto alone\n# (but simplifies our setup)\n" + androidLibraryWorkspaceTemplateString)

var androidLibraryRuleTemplateString = `load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )
`

var androidProtoLibraryRuleTemplate = mustTemplate(androidLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    android_library(
        name = name,
        srcs = [name_pb],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        exports = PROTO_DEPS + kwargs.get("exports", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@com_google_protobuf//:protobuf_javalite",
    Label("//android:well_known_protos"),  # Lite is missing gen_well_known_protos_java from protobuf, compile them manually
]`)

var androidGrpcLibraryRuleTemplate = mustTemplate(androidLibraryRuleTemplateString + `
    # Create {{ .Lang.Name }} library
    android_library(
        name = name,
        srcs = [name_pb],
        deps = GRPC_DEPS + kwargs.get("deps", []),
        exports = GRPC_DEPS + kwargs.get("exports", []),
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@io_grpc_grpc_java//api",
    "@io_grpc_grpc_java//protobuf-lite",
    "@io_grpc_grpc_java//stub",
    "@io_grpc_grpc_java//stub:javax_annotation",
    "@com_google_code_findbugs_jsr305//jar",
    "@com_google_guava_guava//jar",
    "@com_google_protobuf//:protobuf_javalite",
    "@com_google_protobuf//:protobuf_java_util",
    Label("//android:well_known_protos"),  # Lite is missing gen_well_known_protos_java from protobuf, compile them manually
]`)

func makeAndroid() *Language {
	return &Language{
		Dir:  "android",
		Name: "android",
		DisplayName: "Android",
		Notes: mustTemplate("Rules for generating Android protobuf and gRPC ``.jar`` files and libraries using standard Protocol Buffers and `gRPC-Java <https://github.com/grpc/grpc-java>`_. Libraries are created with ``android_library`` from `rules_android <https://github.com/bazelbuild/rules_android>`_"),
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{"all"},
		Rules: []*Rule{
			&Rule{
				Name:             "android_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//android:javalite_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates an Android protobuf ``.jar`` file",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"none"},
			},
			&Rule{
				Name:             "android_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//android:javalite_plugin", "//android:grpc_javalite_plugin"},
				WorkspaceExample: javaGrpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates Android protobuf and gRPC ``.jar`` files",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"none"},
			},
			&Rule{
				Name:             "android_proto_library",
				Kind:             "proto",
				Implementation:   androidProtoLibraryRuleTemplate,
				WorkspaceExample: androidProtoLibraryWorkspaceTemplate,
				BuildExample:     protoLibraryExampleTemplate,
				Doc:              "Generates an Android protobuf library using ``android_library`` from ``rules_android``",
				Attrs:            javaLibraryRuleAttrs,
				SkipTestPlatforms: []string{"none"},
			},
			&Rule{
				Name:             "android_grpc_library",
				Kind:             "grpc",
				Implementation:   androidGrpcLibraryRuleTemplate,
				WorkspaceExample: androidGrpcLibraryWorkspaceTemplate,
				BuildExample:     grpcLibraryExampleTemplate,
				Doc:              "Generates Android protobuf and gRPC library using ``android_library`` from ``rules_android``",
				Attrs:            javaLibraryRuleAttrs,
				SkipTestPlatforms: []string{"none"},
			},
		},
	}
}
