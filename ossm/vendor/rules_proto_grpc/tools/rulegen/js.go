package main

var jsWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")

build_bazel_rules_nodejs_dependencies()

load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")

yarn_install(
    name = "npm",
    package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
    yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
)`)

var jsProtoLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Resolve deps
    deps = [
        dep.replace("@npm", kwargs.get("deps_repo", "@npm"))
        for dep in PROTO_DEPS
    ]

    # Create {{ .Lang.Name }} library
    js_library(
        name = name,
        srcs = [name_pb],
        deps = deps + kwargs.get("deps", []),
        package_name = kwargs.get("package_name", name),
        strip_prefix = name_pb if not kwargs.get("legacy_path") else None,
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

PROTO_DEPS = [
    "@npm//google-protobuf",
]`)

var nodeGrpcLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:js_grpc_node_compile.bzl", "js_grpc_node_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    js_grpc_node_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Resolve deps
    deps = [
        dep.replace("@npm", kwargs.get("deps_repo", "@npm"))
        for dep in GRPC_DEPS
    ]

    # Create {{ .Lang.Name }} library
    js_library(
        name = name,
        srcs = [name_pb],
        deps = deps + kwargs.get("deps", []),
        package_name = kwargs.get("package_name", name),
        strip_prefix = name_pb if not kwargs.get("legacy_path") else None,
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@npm//google-protobuf",
    "@npm//@grpc/grpc-js",
]`)

var jsGrpcWebLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:js_grpc_web_compile.bzl", "js_grpc_web_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    js_grpc_web_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Resolve deps
    deps = [
        dep.replace("@npm", kwargs.get("deps_repo", "@npm"))
        for dep in GRPC_DEPS
    ]

    # Create {{ .Lang.Name }} library
    js_library(
        name = name,
        srcs = [name_pb],
        deps = deps + kwargs.get("deps", []),
        package_name = kwargs.get("package_name", name),
        strip_prefix = name_pb if not kwargs.get("legacy_path") else None,
        {{ .Common.LibraryArgsForwardingSnippet }}
    )

GRPC_DEPS = [
    "@npm//google-protobuf",
    "@npm//grpc-web",
]`)

var jsLibraryRuleAttrs = append(append([]*Attr(nil), libraryRuleAttrs...), []*Attr{
	&Attr{
		Name:      "package_name",
		Type:      "string",
		Default:   "",
		Doc:       "The package name to use for the library. If unprovided, the target name is used.",
		Mandatory: false,
	},
	&Attr{
		Name:      "deps_repo",
		Type:      "string",
		Default:   "@npm",
		Doc:       "The repository to load the dependencies from, if you don't use ``@npm``",
		Mandatory: false,
	},
	&Attr{
		Name:      "legacy_path",
		Type:      "bool",
		Default:   "False",
		Doc:       "Use the legacy <name>_pb path segment from the generated library require path.",
		Mandatory: false,
	},
}...)

var jsDependencyNote = `

.. note:: You must add the required dependencies to your package.json file:

   .. code-block:: json

      "dependencies": {
        "@grpc/grpc-js": "1.7.3",
        "google-protobuf": "3.21.2",
        "grpc-tools": "1.11.3",
        "grpc-web": "1.4.2",
        "ts-protoc-gen": "0.15.0"
      }
`

func makeJavaScript() *Language {
	return &Language{
		Dir:   "js",
		Name:  "js",
		DisplayName: "JavaScript",
		Notes: mustTemplate("Rules for generating JavaScript protobuf, gRPC-node and gRPC-Web ``.js`` and ``.d.ts`` files using standard Protocol Buffers and gRPC." + jsDependencyNote),
		Flags: commonLangFlags,
		Aliases: map[string]string{
			"nodejs_proto_compile": "js_proto_compile",
			"nodejs_proto_library": "js_proto_library",
			"nodejs_grpc_compile": "js_grpc_node_compile",
			"nodejs_grpc_library": "js_grpc_node_library",
		},
		Rules: []*Rule{
			&Rule{
				Name:             "js_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//js:js_plugin", "//js:ts_plugin"},
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates JavaScript protobuf ``.js`` and ``.d.ts`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "js_grpc_node_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//js:js_plugin", "//js:ts_plugin", "//js:grpc_node_plugin", "//js:grpc_node_ts_plugin"},
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates JavaScript protobuf and gRPC-node ``.js`` and ``.d.ts`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "js_grpc_web_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//js:js_plugin", "//js:ts_plugin", "//js:grpc_web_js_plugin"},
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates JavaScript protobuf and gRPC-Web ``.js`` and ``.d.ts`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "js_proto_library",
				Kind:             "proto",
				Implementation:   jsProtoLibraryRuleTemplate,
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     protoLibraryExampleTemplate,
				Doc:              "Generates a JavaScript protobuf library using ``js_library`` from ``rules_nodejs``",
				Attrs:            jsLibraryRuleAttrs,
			},
			&Rule{
				Name:             "js_grpc_node_library",
				Kind:             "grpc",
				Implementation:   nodeGrpcLibraryRuleTemplate,
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     grpcLibraryExampleTemplate,
				Doc:              "Generates a Node.js protobuf + gRPC-node library using ``js_library`` from ``rules_nodejs``",
				Attrs:            jsLibraryRuleAttrs,
			},
			&Rule{
				Name:             "js_grpc_web_library",
				Kind:             "grpc",
				Implementation:   jsGrpcWebLibraryRuleTemplate,
				WorkspaceExample: jsWorkspaceTemplate,
				BuildExample:     grpcLibraryExampleTemplate,
				Doc:              "Generates a JavaScript protobuf + gRPC-Web library using ``js_library`` from ``rules_nodejs``",
				Attrs:            jsLibraryRuleAttrs,
			},
		},
	}
}
