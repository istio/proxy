# Overview

This document aims to provide context to future contributors on why we decided to rewrite
the `swift_proto_library` and `swift_grpc_library` rules into the new
`swift_proto_library` and `swift_proto_library_group` rules,
as well as a guide for consumers of the deprecated rules to migrate to the new rules.

On this page:
  * [Why rewrite?](#why-rewrite)
  * [How to Migrate](#how-to-migrate)

## Why rewrite?

The new `swift_proto_library` rule was created to address several issues with the old
`swift_proto_library` and `swift_grpc_library` rules which would be difficult if not impossible
to fix while maintaining full backwards compatibility.

### 1. The Swift Protoc Gen Aspect

Firstly, the old `swift_proto_library` rule used the `swift_protoc_gen_aspect` aspect
which traversed its direct `proto_library` dependencies and their transitive graph of `proto_library` dependencies,
and compiled all of these into swift modules.

This meant that a single `swift_proto_library` target could generate and and compile swift source files
for *all* of the `proto_library` targets in potentially a very large dependency graph,
which could span across multiple repositories.

In practice, though, consumers typically preferred to have more control over the mapping of `proto_library` to `swift_proto_library` targets.
Additionally, this aspect meant that the module name and other compilation options could not be configured on a per-target basis,
since the aspect needed to be able to determine all of this information by just looking at the providers of the `proto_library` targets.


Using the `deprecated_grpc` example in this repository to demonstrate,
consumers had to import the generated service swift proto module with:

```
import examples_xplatform_deprecated_grpc_echo_server_services_swift
```

But with the new `swift_proto_library` rule,
they can assign an arbitrary module name to the target and import it with:

```
import ServiceServer
```

This removes the guesswork of trying to determine what the module name will be,
and it allows engineers to work with the new `swift_proto_library` targets exactly the same
way they would if these were `swift_library` targets they had written by hand,
except the source files are generated for them.

### 2. Implicit, internal proto compiler and runtime dependencies.

The second main issue the new implementation is intended to address is the specific proto compiler,
proto compiler plugin, and compile time proto library dependencies the old rules required.

The old rules depended internally on the SwiftProtobuf and GRPC targets defined in the targets under
the `third_party` directory. This was problematic for consumers who wanted to use different versions
of protoc, the protoc plugins or compile time dependencies.

Specifically, this issue came to a head when implementing support for proto targets in
`rules_swift_package_manager`. There was a lengthy slack conversation here on the topic:
https://bazelbuild.slack.com/archives/CD3QY5C2X/p1692055426375909

The problem was that some Swift packages have package dependencies on protobuf
libraries which contain the same code (though perhaps a different version) as the protobuf
targets defined in this repository which were dependencies of those rules.

This meant that consumers could *either* use the Swift package dependencies *or*
`swift_proto_library` / `swift_grpc_library` but not both or else they would encounter linker errors.

The new swift_proto_library rule is completely configurable in terms of:
- the proto compiler target
- the proto compiler plugin target(s)
- the swift compile-time dependency target(s)

This is accomplished with a new `swift_proto_compiler` rule
which is passed as an attribute to the new `swift_proto_library` rule.

The `swift_proto_compiler` rule represents the combination of a protoc binary,
protoc plugin binary (e.g. the protoc plugin built from the `SwiftProtobuf` or `grpc-swift` repositories),
and the set of options passed to the protoc plugin.

We provide pre-configured `swift_proto_compiler` targets for:
- swift protos
- swift server protos
- swift client protos

3rd parties are also welcome to create their own `swift_proto_compiler` targets configured based on their needs.
In the case of `rules_swift_package_manager`, this enables the creation of a `swift_proto_compiler` target
configured to use the same dependencies as the other Swift packages.

Finally, we also no longer need a separate `swift_grpc_library` rule,
because we can just configure a `swift_proto_library` to use the GRPC plugin instead.
This is how the analagous `go_proto_library` works as well.

## How to Migrate

NOTE: If you required the capability of the the deprecated `swift_proto_library`
to generate the protos transitively for multiple `proto_library` targets from a single `swift_proto_library` target,
you will need to to use the `swift_proto_library_group` rule.
See the relevant section below for more information.

## 1. Swift Proto Library

Given two `proto_library` targets for `foo.proto` and `bar.proto`,
where `bar.proto` depends on `foo.proto` and the `api.proto` from the well known types:

```
proto_library(
    name = "foo_proto",
    srcs = ["foo.proto"],
)

proto_library(
    name = "bar_proto",
    srcs = ["bar.proto"],
    deps = [
        "@com_google_protobuf//:api_proto",
        ":foo_proto",
    ],
)
```

Which had the old `swift_proto_library` targets:

```
swift_proto_library(
    name = "foo_swift_proto",
    deps = [":foo_proto"],
)

swift_proto_library(
    name = "bar_swift_proto",
    deps = [":bar_proto"],
)
```

These can be migrated to the following new `swift_proto_library` targets:

```
swift_proto_library(
    name = "foo_swift_proto",
    protos = [":foo_proto"],
)

swift_proto_library(
    name = "bar_swift_proto",
    protos = [":bar_proto"],
    deps = [":foo_swift_proto"]
)
```

Note that you must declare deps between the new `swift_proto_library` targets
parallel to those between the `proto_library` targets, and that the autogenerated Swift module names
of the new targets are derived from the target labels of the `swift_proto_library` targets,
not the `proto_library` targets as they were before.

If you wish, you may set the `module_name` attribute on the new `swift_proto_library` targets
as you would any other `swift_library` target.

## 2. Swift GRPC Library

Given the following `proto_library` target where `service.proto` contains the definition
for the Echo service with an RPC named Echo which takes an EchoRequest and returns an EchoResponse:

```
proto_library(
    name = "service_proto",
    srcs = ["service.proto"],
)
```

With the following deprecated `swift_proto_library` and `swift_grpc_library` targets:

```
swift_proto_library(
    name = "service_proto_swift",
    deps = [":service_proto"],
)

swift_grpc_library(
    name = "service_client_services_swift",
    srcs = [":service_proto"],
    flavor = "client",
    deps = [":service_proto_swift"],
)

swift_grpc_library(
    name = "service_client_test_stubs_swift",
    srcs = [":service_proto"],
    flavor = "client_stubs",
    deps = [":service_client_services_swift"],
)

swift_grpc_library(
    name = "service_server_services_swift",
    srcs = [":service_proto"],
    flavor = "server",
    deps = [":service_proto_swift"],
)
```

These can be migrated to the following new `swift_proto_library` targets:

```
swift_proto_library(
    name = "service_server_swift_proto",
    compilers = [
        "@build_bazel_rules_swift//proto/compilers:swift_proto",
        "@build_bazel_rules_swift//proto/compilers:swift_server_proto",
    ],
    protos = [":service_proto"],
)

swift_proto_library(
    name = "service_client_swift_proto",
    compilers = [
        "@build_bazel_rules_swift//proto/compilers:swift_client_proto",
        "@build_bazel_rules_swift//proto/compilers:swift_proto",
    ],
    protos = [":service_proto"],
)

swift_proto_library(
    name = "service_test_client_swift_proto",
    additional_compiler_info = {
        "ExtraModuleImports": "examples_xplatform_grpc_service_client_swift_proto",
    },
    compiler_deps = [
        ":service_client_swift_proto",
    ],
    compilers = ["@build_bazel_rules_swift//proto/compilers:swift_test_client_proto"],
    protos = [":service_proto"],
)
```

Note here that we don't need the intermediate `swift_proto_library` target,
and that we must pass a `swift_proto_compiler` target via the compilers attribute
which is configured with a `grpc-swift` protoc plugin.

Also note that the `client_stubs` flavor uses the `swift_test_client_proto` grpc compiler
and requires additional plugin options and compiler dependencies.

This configuration was added to ensure continuity of rule capabilities,
but when possible we recommend migrating off of these targets.
This is because TestClient protoc plugin option was deprecated with Swift 5.6,
as the test clients do not conform to Sendable.
This capability may be removed in a future major version update.

The alternative recommended by the `grpc-swift` authors is to register a mock server on localhost.

## 3. Swift Proto Library Group

To aid consumers of the old rules in their migration to the new rules,
we introduced the `swift_proto_library_group` rule which has an aspect similar to the old rule,
while still allowing for pluggable swift proto compilers.

One drawback of the aspect approach that is still present here is the inability
to configure the module names or other attributes of the generated modules
because they must be derived directly from the `proto_library` targets.

Specific usecases for this rule include:
- You have a lot of protos and do not want to handwrite a lot of `swift_proto_library` targets,
  or generate them with a tool like Gazelle.
- You have `proto_library` targets which are not visible to the `swift_proto_library` target,
  and you need to generate the protos for them transitively by tranversing the graph from a visible `proto_library` target.

Example of use:

```
proto_library(
    name = "request_proto",
    srcs = [
        "request.proto",
    ]
)

proto_library(
    name = "response_proto",
    srcs = [
        "response.proto",
    ],
    deps = [
        ":request_proto",
    ],
)

swift_proto_library_group(
    name = "proto_library_group_swift_proto",
    compiler = "//proto/compilers:swift_proto",
    proto = ":response_proto",
)
```

In this example, `proto_library_group_swift_proto` depends on `response_proto` which in turn depends on `request_proto`.
The Swift source files for `response_proto` and transitively `request_proto` will be generated and compiled if you build the
`proto_library_group_swift_proto` target.

NOTE: You can mix-and-match `swift_proto_library` targets and `swift_proto_library_group` targets.
The most common case for this is having a `swift_proto_library_group` target for your non-GRPC protos,
and then a `swift_proto_library_group` target which depends on it and generates the services,
similar to the old `swift_grpc_library` rule depending on the old `swift_proto_library` rule.

See `//examples/xplatform/proto_library_group/service:service_server_swift_proto` for a concrete example.

## F.A.Q.

As consumers raise questions during their migrations to the new `swift_proto_library` rule,
we will add their answers here as a reference.

### Can I group protos to reduce number of modules?

Yes, the new rule does not enforce any specific mapping of protos or proto packages to swift modules.
You can compile each proto into a separate swift module, group protos into modules based on packages,
or just compile all of your protos into one large module.

### Can I use a completely custom protobuf compiler?

Yes, you can use an arbitrary non-protoc Swift protobuf compiler by implementing your own rule
which provides `SwiftProtoCompilerInfo`, and passing this to the `swift_proto_library` target's
`compilers` attribute.

The only real requirement is that it has a function wich accepts a list of `ProtoInfo` providers,
and produces a list Swift source files from these which the `swift_proto_library` rule compiles.
See `//examples/xplatform/custom_swift_proto_compiler/compile` for an example of how to do this.

### How can I handle protos with the same file name but different paths?

Within a Swift module, source file names must be unique.
By default, the generated source files will mirror the structure of their respective proto files.

For example, the following target relative proto paths:
`foo/bar.proto`
`bar.proto`

Will produce the following target relative source file paths:
`foo/bar.swift`
`bar.swift`

This will result in a compiler error.

To circumvent this issue, the `SwiftProtobuf` `protoc` plugin accepts the `PathToUnderscores` option,
which incorporates the directory structure into the generated file names.

When set, the following target relative source file paths will be produced instead:
`foo_bar.swift`
`bar.swift`

Which will compile without issue.

In the deprecated `swift_proto_library` rule, there was a `feature` that set the `PathToUnderscores` option internally.
In the new rules, this is not necessary because you can pass arbitrary options directly to the `swift_proto_compiler`,
and its respective `protoc` plugin.

To see an example, check out `//examples/xplatform/proto:example_path_to_underscores_proto_swift`:

```
swift_proto_library(
    name = "example_path_to_underscores_proto_swift",
    additional_compiler_info = {
        "FileNaming": "PathToUnderscores",
    },
    protos = [":example_path_to_underscores_proto"],
)
```

You can apply this option to individual `swift_proto_library` targets via the `additional_compiler_info` attribute,
or you can apply them to all of your targets through a custom `swift_proto_compiler` target via the `plugin_options` attribute.

See the documentation for those rules and their respective attributes for more information.
