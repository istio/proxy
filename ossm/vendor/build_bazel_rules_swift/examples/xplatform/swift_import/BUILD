load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_import.bzl", "swift_import")
load("//swift:swift_library.bzl", "swift_library")
load(
    "//test/rules:swift_library_artifact_collector.bzl",
    "swift_library_artifact_collector",
)

swift_binary(
    name = "hello_world",
    deps = [":main"],
)

swift_library(
    name = "main",
    srcs = ["main.swift"],
    deps = [
        ":import",
    ],
)

swift_import(
    name = "import",
    archives = ["dep_outputs/libdep.a"],
    module_name = "dep",
    swiftmodule = "dep_outputs/dep.swiftmodule",
)

swift_library_artifact_collector(
    name = "dep_artifact_collector",
    static_library = "dep_outputs/libdep.a",
    swiftmodule = "dep_outputs/dep.swiftmodule",
    target = ":dep",
)

swift_library(
    name = "dep",
    srcs = ["dep.swift"],
    module_name = "dep",
)
