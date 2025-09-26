load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_module_alias.bzl", "swift_module_alias")

swift_binary(
    name = "hello_world",
    deps = [":main_alias"],
)

swift_library(
    name = "main",
    srcs = ["main.swift"],
)

swift_module_alias(
    name = "main_alias",
    module_name = "Main_Alias",
    deps = [":main"],
)
