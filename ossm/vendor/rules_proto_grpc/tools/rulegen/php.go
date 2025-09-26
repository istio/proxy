package main

func makePhp() *Language {
	return &Language{
		Dir:   "php",
		Name:  "php",
		DisplayName: "PHP",
		Notes: mustTemplate("Rules for generating PHP protobuf and gRPC ``.php`` files and libraries using standard Protocol Buffers and gRPC"),
		Flags: commonLangFlags,
		Rules: []*Rule{
			&Rule{
				Name:             "php_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//php:php_plugin"},
				WorkspaceExample: protoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates PHP protobuf ``.php`` files",
				Attrs:            compileRuleAttrs,
			},
			&Rule{
				Name:             "php_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//php:php_plugin", "//php:grpc_php_plugin"},
				WorkspaceExample: grpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates PHP protobuf and gRPC ``.php`` files",
				Attrs:            compileRuleAttrs,
			},
		},
	}
}
