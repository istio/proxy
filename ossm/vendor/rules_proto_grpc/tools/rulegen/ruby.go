package main

var rubyProtoWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")

rules_ruby_dependencies()

rules_ruby_select_sdk(version = "3.1.1")

load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")

ruby_bundle(
    name = "rules_proto_grpc_bundle",
    gemfile = "@rules_proto_grpc//ruby:Gemfile",
    gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
    includes = {"grpc": ["etc"]},
)`)

var rubyGrpcWorkspaceTemplate = mustTemplate(`load("@rules_proto_grpc//{{ .Lang.Dir }}:repositories.bzl", rules_proto_grpc_{{ .Lang.Name }}_repos = "{{ .Lang.Name }}_repos")

rules_proto_grpc_{{ .Lang.Name }}_repos()

load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")

rules_ruby_dependencies()

rules_ruby_select_sdk(version = "3.1.1")

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")

ruby_bundle(
    name = "rules_proto_grpc_bundle",
    gemfile = "@rules_proto_grpc//ruby:Gemfile",
    gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
    includes = {"grpc": ["etc"]},
)`)

var rubyLibraryRuleTemplate = mustTemplate(`load("//{{ .Lang.Dir }}:{{ .Lang.Name }}_{{ .Rule.Kind }}_compile.bzl", "{{ .Lang.Name }}_{{ .Rule.Kind }}_compile")
load("//:defs.bzl", "bazel_build_rule_common_attrs", "proto_compile_attrs")
load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_library")

def {{ .Rule.Name }}(name, **kwargs):
    # Compile protos
    name_pb = name + "_pb"
    {{ .Lang.Name }}_{{ .Rule.Kind }}_compile(
        name = name_pb,
        {{ .Common.CompileArgsForwardingSnippet }}
    )

    # Create {{ .Lang.Name }} library
    ruby_library(
        name = name,
        srcs = [name_pb],
        deps = ["@rules_proto_grpc_bundle//:gems"] + kwargs.get("deps", []),
        includes = [native.package_name() + "/" + name_pb],
        {{ .Common.LibraryArgsForwardingSnippet }}
    )`)

func makeRuby() *Language {
	return &Language{
		Dir:   "ruby",
		Name:  "ruby",
		DisplayName: "Ruby",
		Notes: mustTemplate("Rules for generating Ruby protobuf and gRPC ``.rb`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``ruby_library`` from `rules_ruby <https://github.com/bazelruby/rules_ruby>`_"),
		Flags: commonLangFlags,
		SkipTestPlatforms: []string{
			"windows", // CI has no Ruby available for windows
			"linux",  // Ruby fails to run on ubuntu image with "/usr/bin/env: 'ruby': Permission denied"
		},
		Rules: []*Rule{
			&Rule{
				Name:             "ruby_proto_compile",
				Kind:             "proto",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//ruby:ruby_plugin"},
				WorkspaceExample: rubyProtoWorkspaceTemplate,
				BuildExample:     protoCompileExampleTemplate,
				Doc:              "Generates Ruby protobuf ``.rb`` files",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "ruby_grpc_compile",
				Kind:             "grpc",
				Implementation:   compileRuleTemplate,
				Plugins:          []string{"//ruby:ruby_plugin", "//ruby:grpc_ruby_plugin"},
				WorkspaceExample: rubyGrpcWorkspaceTemplate,
				BuildExample:     grpcCompileExampleTemplate,
				Doc:              "Generates Ruby protobuf and gRPC ``.rb`` files",
				Attrs:            compileRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "ruby_proto_library",
				Kind:             "proto",
				Implementation:   rubyLibraryRuleTemplate,
				WorkspaceExample: rubyProtoWorkspaceTemplate,
				BuildExample:     protoLibraryExampleTemplate,
				Doc:              "Generates a Ruby protobuf library using ``ruby_library`` from ``rules_ruby``",
				Attrs:            libraryRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
			&Rule{
				Name:             "ruby_grpc_library",
				Kind:             "grpc",
				Implementation:   rubyLibraryRuleTemplate,
				WorkspaceExample: rubyGrpcWorkspaceTemplate,
				BuildExample:     grpcLibraryExampleTemplate,
				Doc:              "Generates a Ruby protobuf and gRPC library using ``ruby_library`` from ``rules_ruby``",
				Attrs:            libraryRuleAttrs,
				SkipTestPlatforms: []string{"windows"},
			},
		},
	}
}
