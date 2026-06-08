load(
    "//swift:swift_extract_symbol_graph.bzl",
    "swift_extract_symbol_graph",
)
load(
    "//swift:swift_library.bzl",
    "swift_library",
)
load(
    "//test/fixtures:common.bzl",
    "FIXTURE_TAGS",
)

package(
    default_testonly = True,
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "some_module",
    srcs = ["SomeModule.swift"],
    module_name = "SomeModule",
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "some_module_with_extension",
    srcs = ["SomeModuleWithExtension.swift"],
    module_name = "SomeModuleWithExtension",
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "importing_module",
    srcs = ["ImportingModule.swift"],
    module_name = "ImportingModule",
    tags = FIXTURE_TAGS,
    deps = [":some_module"],
)

swift_extract_symbol_graph(
    name = "some_module_symbol_graph",
    tags = FIXTURE_TAGS,
    targets = [":some_module"],
)

swift_extract_symbol_graph(
    name = "some_module_symbol_graph_with_extension_block_symbols",
    emit_extension_block_symbols = "1",
    tags = FIXTURE_TAGS,
    targets = [":some_module_with_extension"],
)

swift_extract_symbol_graph(
    name = "importing_module_symbol_graph",
    tags = FIXTURE_TAGS,
    targets = [":importing_module"],
)

swift_extract_symbol_graph(
    name = "all_symbol_graphs",
    tags = FIXTURE_TAGS,
    targets = [
        ":importing_module",
        ":some_module",
    ],
)
