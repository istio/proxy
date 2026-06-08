package main

var docCustomRuleTemplateString = mustTemplate(`load(
    "//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_attrs",
)
load("//internal:compile.bzl", "proto_compile")

# Create compile rule
def {{ .Rule.Name }}_impl(ctx):  # buildifier: disable=function-docstring
    # Load attrs that we pass as args
    options = ctx.attr.options
    extra_protoc_args = getattr(ctx.attr, "extra_protoc_args", [])
    extra_protoc_files = ctx.files.extra_protoc_files

    # Make mutable
    options = {k: v for (k, v) in options.items()}
    extra_protoc_files = [] + extra_protoc_files

    # Mutate args with template
    options["*"] = [
        ctx.file.template.path,
        ctx.attr.name,
    ]
    extra_protoc_files.append(ctx.file.template)

    # Execute with extracted attrs
    return proto_compile(ctx, options, extra_protoc_args, extra_protoc_files)

{{ .Rule.Name }} = rule(
    implementation = {{ .Rule.Name }}_impl,
    attrs = dict(
        proto_compile_attrs,
        template = attr.label(
            allow_single_file = True,
            doc = "The documentation template file.",
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

var docCustomExampleTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:defs.bzl", "{{ .Rule.Name }}")

{{ .Rule.Name }}(
    name = "greeter_{{ .Lang.Name }}_{{ .Rule.Kind }}.txt",
    output_mode = "NO_PREFIX",
    protos = [
        "@rules_proto_grpc//example/proto:greeter_grpc",
        "@rules_proto_grpc//example/proto:thing_proto",
    ],
    template = "template.txt",
)`)

var docTemplateRuleAttrs = append(append([]*Attr(nil), compileRuleAttrs...), []*Attr{
	&Attr{
		Name:      "template",
		Type:      "label",
		Default:   "None",
		Doc:       "The documentation template file.",
		Mandatory: true,
	},
}...)

func makeDoc() *Language {
	return &Language{
		Dir:   "doc",
		Name:  "doc",
		DisplayName: "Documentation",
		Notes: mustTemplate("Rules for generating protobuf Markdown, JSON, HTML or DocBook documentation with `protoc-gen-doc <https://github.com/pseudomuto/protoc-gen-doc>`_"),
		Flags: commonLangFlags,
		Rules: []*Rule{
			&Rule{
				Name:             "doc_docbook_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//doc:docbook_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates DocBook ``.xml`` documentation file",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "doc_html_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//doc:html_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates ``.html`` documentation file",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "doc_json_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//doc:json_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates ``.json`` documentation file",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "doc_markdown_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//doc:markdown_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates Markdown ``.md`` documentation file",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "doc_template_compile",
				Kind:             "proto",
				Implementation:   docCustomRuleTemplateString,
				Plugins:          []string{"//doc:template_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     docCustomExampleTemplate,
				Doc:              "Generates documentation file using Go template file",
				Attrs:            docTemplateRuleAttrs,
				Experimental:     true,
			},
		},
	}
}
