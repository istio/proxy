# This example tests that Swift code can be called from Objective-C code and
# vice versa when it is linked into the existing native linking rules for Apple
# platforms.

load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_library.bzl", "swift_library")

licenses(["notice"])

objc_library(
    name = "PrintStream",
    srcs = ["OIPrintStream.m"],
    hdrs = ["OIPrintStream.h"],
    target_compatible_with = ["@platforms//os:macos"],
)

swift_library(
    name = "Printer",
    srcs = ["Printer.swift"],
    generated_header_name = "generated_header/Printer-Swift.h",
    generates_header = True,
    deps = [":PrintStream"],
)

objc_library(
    name = "main",
    srcs = ["main.m"],
    target_compatible_with = ["@platforms//os:macos"],
    deps = [":Printer"],
)

swift_binary(
    name = "objc_interop",
    deps = [":main"],
)
