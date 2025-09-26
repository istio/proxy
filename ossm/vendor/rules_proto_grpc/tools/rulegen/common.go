package main


var commonLangFlags = []*Flag{}


var compileRuleAttrs = []*Attr{
    &Attr{
		Name:      "protos",
		Type:      "label_list",
		Doc:       "List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)",
		Mandatory: true,
		Providers: []string{"ProtoInfo"},
	},
	&Attr{
		Name:      "options",
		Type:      "string_list_dict",
		Default:   "[]",
		Doc:       "Extra options to pass to plugins, as a dict of plugin label -> list of strings. The key * can be used exclusively to apply to all plugins",
		Mandatory: false,
	},
	&Attr{
		Name:      "verbose",
		Type:      "int",
		Default:   "0",
		Doc:       "The verbosity level. Supported values and results are 0: Show nothing, 1: Show command, 2: Show command and sandbox after running protoc, 3: Show command and sandbox before and after running protoc, 4. Show env, command, expected outputs and sandbox before and after running protoc",
		Mandatory: false,
	},
	&Attr{
		Name:      "prefix_path",
		Type:      "string",
		Default:   `""`,
		Doc:       "Path to prefix to the generated files in the output directory",
		Mandatory: false,
	},
	&Attr{
		Name:      "extra_protoc_args",
		Type:      "string_list",
		Default:   "[]",
		Doc:       "A list of extra command line arguments to pass directly to protoc, not as plugin options",
		Mandatory: false,
	},
	&Attr{
		Name:      "extra_protoc_files",
		Type:      "label_list",
		Default:   "[]",
		Doc:       "List of labels that provide extra files to be available during protoc execution",
		Mandatory: false,
	},
	&Attr{
		Name:      "output_mode",
		Type:      "string",
		Default:   "PREFIXED",
		Doc:       "The output mode for the target. PREFIXED (the default) will output to a directory named by the target within the current package root, NO_PREFIX will output directly to the current package. Using NO_PREFIX may lead to conflicting writes",
		Mandatory: false,
	},
}


var libraryRuleAttrs = append(append([]*Attr(nil), compileRuleAttrs...), []*Attr{
    &Attr{
		Name:      "deps",
		Type:      "label_list",
		Default:   "[]",
		Doc:       "List of labels to pass as deps attr to underlying lang_library rule",
		Mandatory: false,
	},
}...)


var compileRuleTemplate = mustTemplate(`load(
    "//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_attrs",
    "proto_compile_impl",
)

# Create compile rule
{{ .Rule.Name }} = rule(
    implementation = proto_compile_impl,
    attrs = dict(
        proto_compile_attrs,
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [{{ range .Rule.Plugins }}
                Label("{{ . }}"),{{ end }}
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    toolchains = [str(Label("//protobuf:toolchain_type"))],
)`)

// When editing, note that Go and gateway do not use this snippet and have their own local version
var compileArgsForwardingSnippet = `**{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs
        }  # Forward args`

var libraryArgsForwardingSnippet = `**{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # Forward Bazel common args`


var protoWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()`)


var grpcWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()`)


var protoCompileExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:person_proto"],
)

{{ .Rule.Name }}(
    name = "place_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:place_proto"],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)`)


var grpcCompileExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
)`)


var protoLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:person_proto"],
    deps = ["place_{{ .Lang.Name }}_{{ .Rule.Kind }}"],
)

{{ .Rule.Name }}(
    name = "place_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:place_proto"],
    deps = ["thing_{{ .Lang.Name }}_{{ .Rule.Kind }}"],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)`)


var grpcLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
    deps = ["thing_{{ .Lang.Name }}_{{ .Rule.Kind }}"],
)`)
