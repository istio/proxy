package main

var rustCompileRuleTemplate = mustTemplate(`load(
    "//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_attrs",
)
load(":common.bzl", "ProstProtoInfo", "rust_prost_proto_compile_impl")

# Create compile rule
{{ .Rule.Name }} = rule(
    implementation = rust_prost_proto_compile_impl,
    attrs = dict(
        proto_compile_attrs,
        prost_proto_deps = attr.label_list(
            providers = [ProstProtoInfo],
            mandatory = False,
            doc = "Other protos compiled by prost that our proto directly depends upon. Used to generated externs_path=... options for prost.",
        ),
        declared_proto_packages = attr.string_list(
            mandatory = True,
            doc = "List of labels that provide the ProtoInfo provider (such as proto_library from rules_proto)",
        ),
        crate_name = attr.string(
            mandatory = False,
            doc = "Name of the crate these protos will be compiled into later using rust_library. See rust_prost_proto_library macro for more details.",
        ),
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

var rustWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

rules_rust_dependencies()

rust_register_toolchains(edition = "2021")

load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")

crate_universe_dependencies(bootstrap = True)

load("@rules_proto_grpc//rust:crate_deps.bzl", "crate_repositories")

crate_repositories()`)

var rustLibraryRuleTemplateString = `load("//rust:common.bzl", "create_name_to_label", "prepare_prost_proto_deps", "prost_compile_attrs")
load("//{{ .Lang.Dir }}:{{ .Rule.Base }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Rule.Base }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("//{{ .Lang.Dir }}:rust_fixer.bzl", "rust_proto_crate_fixer", "rust_proto_crate_root")
load("@rules_rust//rust:defs.bzl", "rust_library")

def {{ .Rule.Name }}(name, **kwargs):  # buildifier: disable=function-docstring
    # Compile protos
    name_pb = name + "_pb"
    name_fixed = name_pb + "_fixed"
    name_root = name + "_root"

    prost_proto_deps = kwargs.get("prost_proto_deps", [])
    prost_proto_compiled_targets = prepare_prost_proto_deps(prost_proto_deps)

    {{ .Rule.Base }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        crate_name = name,
        prost_proto_deps = prost_proto_compiled_targets,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in proto_compile_attrs.keys() or
               k in bazel_build_rule_common_attrs or
               k in prost_compile_attrs
        }  # Forward args
    )

    # Fix up imports
    rust_proto_crate_fixer(
        name = name_fixed,
        compilation = name_pb,
    )
`

var rustProstProtoLibraryRuleTemplate = mustTemplate(rustLibraryRuleTemplateString + `
    rust_proto_crate_root(
        name = name_root,
        crate_dir = name_fixed,
    )

    # Create {{ .Rule.Base }} library
    rust_library(
        name = name,
        edition = "2021",
        crate_root = name_root,
        crate_name = kwargs.get("crate_name"),
        srcs = [name_fixed],
        deps = kwargs.get("prost_deps", [create_name_to_label("prost"), create_name_to_label("prost-types")]) +
               kwargs.get("pbjson_deps", [create_name_to_label("pbjson-types"), create_name_to_label("pbjson")]) +
               kwargs.get("serde_deps", [create_name_to_label("serde")]) +
               kwargs.get("deps", []) +
               prost_proto_deps,
        proc_macro_deps = [kwargs.get("prost_derive_dep", create_name_to_label("prost-derive"))],
        {{ .Common.LibraryArgsForwardingSnippet }}
    )`)

var rustTonicGrpcLibraryRuleTemplate = mustTemplate(rustLibraryRuleTemplateString + `
    rust_proto_crate_root(
        name = name_root,
        crate_dir = name_fixed,
        mod_file = kwargs.get("mod_file"),
    )

    # Create {{ .Rule.Base }} library
    rust_library(
        name = name,
        edition = "2021",
        crate_root = name_root,
        crate_name = kwargs.get("crate_name"),
        srcs = [name_fixed],
        deps = kwargs.get("prost_deps", [create_name_to_label("prost"), create_name_to_label("prost-types")]) +
               kwargs.get("pbjson_deps", [create_name_to_label("pbjson-types"), create_name_to_label("pbjson")]) +
               kwargs.get("serde_deps", [create_name_to_label("serde")]) +
               kwargs.get("tonic_deps", [create_name_to_label("tonic")]) +
               kwargs.get("deps", []) +
               prost_proto_deps,
        proc_macro_deps = [kwargs.get("prost_derive_dep", create_name_to_label("prost-derive"))],
        {{ .Common.LibraryArgsForwardingSnippet }}
    )`)

var rustProtoCompileExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        "//rust:rust_prost_plugin": ["type_attribute=.other.space.Person=#[derive(Eq\\,Hash)]"],
    },
    protos = ["@rules_proto_grpc//example/proto:person_proto"],
)

{{ .Rule.Name }}(
    name = "place_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        "//rust:rust_prost_plugin": ["type_attribute=.other.space.Place=#[derive(Eq\\,Hash)]"],
    },
    protos = ["@rules_proto_grpc//example/proto:place_proto"],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        # Known limitation, can't derive if the type contains a pbjson type.
        # "//rust:rust_prost_plugin": ["type_attribute=.other.space.Thing=#[derive(Eq\\,Hash)]"],
    },
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
)`)

var rustGrpcCompileExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    protos = [
        "@rules_proto_grpc//example/proto:greeter_grpc",
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var rustProtoLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_place_{{ .Rule.Base }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        "//rust:rust_prost_plugin": [
            "type_attribute=.example.proto.Person=#[derive(Eq\\,Hash)]",
            "type_attribute=.example.proto.Place=#[derive(Eq\\,Hash)]",
        ],
    },
    prost_proto_deps = [
        ":thing_{{ .Rule.Base }}_{{ .Rule.Kind }}",
    ],
    protos = [
        "@rules_proto_grpc//example/proto:person_proto",
        "@rules_proto_grpc//example/proto:place_proto",
    ],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Rule.Base }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        # Known limitation, can't derive if the type contains a pbjson type.
        # "//rust:rust_prost_plugin": ["type_attribute=.example.proto.Thing=#[derive(Eq\\,Hash)]"],
    },
    protos = [
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var rustGrpcLibraryExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "rust_prost_proto_library", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "greeter_{{ .Rule.Base }}_{{ .Rule.Kind }}",
    declared_proto_packages = ["example.proto"],
    options = {
        "//rust:rust_prost_plugin": [
            "type_attribute=.example.proto.Person=#[derive(Eq\\,Hash)]",
            "type_attribute=.example.proto.Place=#[derive(Eq\\,Hash)]",
            # "type_attribute=.example.proto.Thing=#[derive(Eq\\,Hash)]",
        ],
    },
    prost_proto_deps = [
        ":thing_prost",
    ],
    protos = [
        "@rules_proto_grpc//example/proto:greeter_grpc",
        "@rules_proto_grpc//example/proto:person_proto",
        "@rules_proto_grpc//example/proto:place_proto",
        # "@rules_proto_grpc//example/proto:thing_proto",
    ],
)

# See https://github.com/rules-proto-grpc/rules_proto_grpc/pull/265/#issuecomment-1577920311
rust_prost_proto_library(
    name = "thing_prost",
    declared_proto_packages = ["example.proto"],
    protos = [
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var rustProstLibraryRuleAttrs = append(append([]*Attr{}, libraryRuleAttrs...), []*Attr{
	&Attr{
		Name:      "prost_deps",
		Type:      "label_list",
		Default:   `["//rust/crates:prost", "//rust/crates:prost-types"]`,
		Doc:       "The prost dependencies that the rust library should depend on.",
		Mandatory: false,
	},
	&Attr{
		Name:      "prost_derive_dep",
		Type:      "label",
		Default:   `//rust/crates:prost-derive`,
		Doc:       "The prost-derive dependency that the rust library should depend on.",
		Mandatory: false,
	},
}...)

var rustTonicLibraryRuleAttrs = append(append([]*Attr{}, rustProstLibraryRuleAttrs...), []*Attr{
	&Attr{
		Name:      "tonic_deps",
		Type:      "label",
		Default:   `[//rust/crates:tonic]`,
		Doc:       "The tonic dependencies that the rust library should depend on.",
		Mandatory: false,
	},
}...)

func makeRust() *Language {
	return &Language{
		Dir:               "rust",
		Name:              "rust",
		DisplayName:       "Rust",
		Notes:             mustTemplate("Rules for generating Rust protobuf and gRPC ``.rs`` files and libraries using `prost <https://github.com/tokio-rs/prost>`_ and `tonic <https://github.com/hyperium/tonic>`_. Libraries are created with ``rust_library`` from `rules_rust <https://github.com/bazelbuild/rules_rust>`_."),
		Flags:             commonLangFlags,
		SkipTestPlatforms: []string{"windows", "macos"}, // CI has no rust toolchain for windows and is broken on mac
		Rules: []*Rule{
			&Rule{
				Name:             "rust_prost_proto_compile",
				Base:             "rust_prost",
				Kind:             "proto",
				Implementation:   rustCompileRuleTemplate,
				Plugins:          []string{"//rust:rust_prost_plugin", "//rust:rust_crate_plugin", "//rust:rust_serde_plugin"},
				WorkspaceExample: rustWorkspaceTemplate,
				BuildExample:     rustProtoCompileExampleTemplate,
				Doc:              "Generates Rust protobuf ``.rs`` files using prost",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "rust_tonic_grpc_compile",
				Base:             "rust_tonic",
				Kind:             "grpc",
				Implementation:   rustCompileRuleTemplate,
				Plugins:          []string{"//rust:rust_prost_plugin", "//rust:rust_crate_plugin", "//rust:rust_serde_plugin", "//rust:rust_tonic_plugin",},
				WorkspaceExample: rustWorkspaceTemplate,
				BuildExample:     rustGrpcCompileExampleTemplate,
				Doc:              "Generates Rust protobuf and gRPC ``.rs`` files using prost and tonic",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "rust_prost_proto_library",
				Base:             "rust_prost",
				Kind:             "proto",
				Implementation:   rustProstProtoLibraryRuleTemplate,
				WorkspaceExample: rustWorkspaceTemplate,
				BuildExample:     rustProtoLibraryExampleTemplate,
				Doc:              "Generates a Rust prost protobuf library using ``rust_library`` from ``rules_rust``",
				Attrs:            rustProstLibraryRuleAttrs,
			},
			&Rule{
				Name:             "rust_tonic_grpc_library",
				Base:             "rust_tonic",
				Kind:             "grpc",
				Implementation:   rustTonicGrpcLibraryRuleTemplate,
				WorkspaceExample: rustWorkspaceTemplate,
				BuildExample:     rustGrpcLibraryExampleTemplate,
				Doc:              "Generates a Rust prost protobuf and tonic gRPC library using ``rust_library`` from ``rules_rust``",
				Attrs:            rustTonicLibraryRuleAttrs,
			},
		},
	}
}
