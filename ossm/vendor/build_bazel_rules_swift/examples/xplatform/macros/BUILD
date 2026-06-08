load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_compiler_plugin.bzl", "swift_compiler_plugin", "universal_swift_compiler_plugin")
load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_test.bzl", "swift_test")

licenses(["notice"])

swift_library(
    name = "stringify",
    srcs = ["Stringify.swift"],
    module_name = "Stringify",
    plugins = [":stringify_macro"],
)

swift_library(
    name = "stringify_universal",
    srcs = ["Stringify.swift"],
    module_name = "StringifyUniversal",
    plugins = [":stringify_macro_universal"],
)

swift_compiler_plugin(
    name = "stringify_macro",
    srcs = [
        "StringifyMacro.swift",
        "StringifyMacroPlugin.swift",
    ],
    module_name = "StringifyMacroPlugin",
    deps = [
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftCompilerPlugin",
        "@SwiftSyntax//:SwiftSyntaxBuilder",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)

universal_swift_compiler_plugin(
    name = "stringify_macro_universal",
    plugin = ":stringify_macro",
)

swift_binary(
    name = "stringify_client",
    srcs = ["StringifyClient.swift"],
    deps = [":stringify"],
)

swift_binary(
    name = "stringify_universal_client",
    srcs = ["StringifyUniversalClient.swift"],
    deps = [":stringify_universal"],
)

swift_test(
    name = "stringify_macro_test",
    srcs = ["StringifyMacroTests.swift"],
    deps = [
        ":stringify_macro",
        "@SwiftSyntax",
        "@SwiftSyntax//:SwiftSyntaxBuilder",
        "@SwiftSyntax//:SwiftSyntaxMacros",
    ],
)
