package main

var bufRuleTemplate = mustTemplate(`load("@rules_proto//proto:defs.bzl", "ProtoInfo")
load(
    "//:defs.bzl",
    "ProtoPluginInfo",
)
load(
    ":buf.bzl",
    "{{ .Rule.Name }}_impl",
)

{{ .Rule.Name }} = rule(
    implementation = {{ .Rule.Name }}_impl,
    attrs = dict({{ range .Rule.Attrs }}
        {{ .Name }} = attr.{{ .Type }}(
            {{ if .Providers }}providers = [{{ range .Providers }}{{ . }}{{ end }}],
            {{ end }}{{ if .Default }}default = {{ .Default }},
            {{ end }}{{ if eq .Name "against_input" }}allow_single_file = [".bin", ".json"],
            {{ end }}mandatory = {{ if .Mandatory }}True{{ else }}False{{ end }},
            doc = "{{ .Doc }}",
        ),{{ end }}
        options = attr.string_list(
            doc = "Extra options to pass to plugins",
        ),
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [{{ range .Rule.Plugins }}
                Label("{{ . }}"),{{ end }}
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    test = True,
    toolchains = [str(Label("//protobuf:toolchain_type"))],
)`)

var bufBreakingExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "{{ .Lang.Name }}_{{ .Rule.Kind }}_lint",
    against_input = "@rules_proto_grpc//buf/example:image.json",
    protos = [
        "@rules_proto_grpc//example/proto:person_proto",
        "@rules_proto_grpc//example/proto:place_proto",
        "@rules_proto_grpc//example/proto:routeguide_proto",
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
)`)

var bufLintExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "person_{{ .Lang.Name }}_{{ .Rule.Kind }}_lint",
    except_rules = ["PACKAGE_VERSION_SUFFIX"],
    protos = ["@rules_proto_grpc//example/proto:person_proto"],
    use_rules = [
        "DEFAULT",
        "COMMENTS",
    ],
)

{{ .Rule.Name }}(
    name = "place_{{ .Lang.Name }}_{{ .Rule.Kind }}_lint",
    except_rules = ["PACKAGE_VERSION_SUFFIX"],
    protos = ["@rules_proto_grpc//example/proto:place_proto"],
    use_rules = [
        "DEFAULT",
        "COMMENTS",
    ],
)

{{ .Rule.Name }}(
    name = "thing_{{ .Lang.Name }}_{{ .Rule.Kind }}_lint",
    except_rules = ["PACKAGE_VERSION_SUFFIX"],
    protos = ["@rules_proto_grpc//example/proto:thing_proto"],
    use_rules = [
        "DEFAULT",
        "COMMENTS",
    ],
)

{{ .Rule.Name }}(
    name = "routeguide_{{ .Lang.Name }}_{{ .Rule.Kind }}_lint",
    except_rules = [
        "PACKAGE_VERSION_SUFFIX",
        "RPC_REQUEST_STANDARD_NAME",
        "RPC_RESPONSE_STANDARD_NAME",
        "SERVICE_SUFFIX",
        "PACKAGE_DIRECTORY_MATCH",
        "RPC_REQUEST_RESPONSE_UNIQUE",
    ],
    protos = ["@rules_proto_grpc//example/proto:routeguide_proto"],
    use_rules = [
        "DEFAULT",
        "COMMENTS",
    ],
)`)

func makeBuf() *Language {
	return &Language{
		Dir:   "buf",
		Name:  "buf",
		DisplayName: "Buf",
		Notes: mustTemplate("Rules for linting and detecting breaking changes in .proto files with `Buf <https://buf.build>`_." + `

Note that these rules behave differently from the other rules in this repo, since these produce no output and are instead used as tests.

Only Linux and Darwin (MacOS) is currently supported by Buf.`),
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{"windows"},
		Rules: []*Rule{
			&Rule{
				Name:             "buf_proto_breaking_test",
				Kind:             "proto",
				Implementation:   bufRuleTemplate,
				Plugins:          []string{"//buf:breaking_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     bufBreakingExampleTemplate,
				Doc:              "Checks .proto files for breaking changes",
				Attrs:            []*Attr{
					&Attr{
						Name:      "protos",
						Type:      "label_list",
						Default:   "[]",
						Doc:       "List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)",
						Mandatory: true,
						Providers: []string{"ProtoInfo"},
					},
					&Attr{
						Name:      "against_input",
						Type:      "label",
						Doc:       "Label of an existing input image file to check against (.json or .bin)",
						Mandatory: true,
					},
					&Attr{
						Name:      "use_rules",
						Type:      "string_list",
						Default:   `["FILE"]`,
						Doc:       "List of Buf breaking rule IDs or categories to use",
						Mandatory: false,
					},
					&Attr{
						Name:      "except_rules",
						Type:      "string_list",
						Default:   "[]",
						Doc:       "List of Buf breaking rule IDs or categories to drop",
						Mandatory: false,
					},
					&Attr{
						Name:      "ignore_unstable_packages",
						Type:      "bool",
						Default:   "False",
						Doc:       "Whether to ignore breaking changes in unstable package versions",
						Mandatory: false,
					},
				},
				Experimental:     true,
				IsTest:           true,
			},
			&Rule{
				Name:             "buf_proto_lint_test",
				Kind:             "proto",
				Implementation:   bufRuleTemplate,
				Plugins:          []string{"//buf:lint_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     bufLintExampleTemplate,
				Doc:              "Lints .proto files",
				Attrs:            []*Attr{
					&Attr{
						Name:      "protos",
						Type:      "label_list",
						Doc:       "List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)",
						Mandatory: true,
						Providers: []string{"ProtoInfo"},
					},
					&Attr{
						Name:      "use_rules",
						Type:      "string_list",
						Default:   `["DEFAULT"]`,
						Doc:       "List of Buf lint rule IDs or categories to use",
						Mandatory: false,
					},
					&Attr{
						Name:      "except_rules",
						Type:      "string_list",
						Default:   "[]",
						Doc:       "List of Buf lint rule IDs or categories to drop",
						Mandatory: false,
					},
					&Attr{
						Name:      "enum_zero_value_suffix",
						Type:      "string",
						Default:   `"_UNSPECIFIED"`,
						Doc:       "Specify the allowed suffix for the zero enum value",
						Mandatory: false,
					},
					&Attr{
						Name:      "rpc_allow_same_request_response",
						Type:      "bool",
						Default:   "False",
						Doc:       "Allow request and response message to be reused in a single RPC",
						Mandatory: false,
					},
					&Attr{
						Name:      "rpc_allow_google_protobuf_empty_requests",
						Type:      "bool",
						Default:   "False",
						Doc:       "Allow request message to be ``google.protobuf.Empty``",
						Mandatory: false,
					},
					&Attr{
						Name:      "rpc_allow_google_protobuf_empty_responses",
						Type:      "bool",
						Default:   "False",
						Doc:       "Allow response message to be ``google.protobuf.Empty``",
						Mandatory: false,
					},
					&Attr{
						Name:      "service_suffix",
						Type:      "string",
						Default:   `"Service"`,
						Doc:       "The suffix to allow for services",
						Mandatory: false,
					},
				},
				Experimental:      true,
				IsTest:            true,
			},
		},
	}
}
