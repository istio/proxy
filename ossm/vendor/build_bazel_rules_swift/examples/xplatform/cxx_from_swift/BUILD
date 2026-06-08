load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_interop_hint.bzl", "swift_interop_hint")

licenses(["notice"])

# 1. This library has some C++ code that you want to interact with from Swift.
# Use the `swift_interop_hint` rule to enable module map generation and provide
# the module name for these headers, since `cc_library` doesn't do enable this
# by default.
cc_library(
    name = "counter",
    srcs = ["counter.cc"],
    hdrs = ["counter.h"],
    aspect_hints = [":cxx_counter_swift_hint"],
    linkstatic = True,
)

swift_interop_hint(
    name = "cxx_counter_swift_hint",
    module_name = "CxxCounter",
)

# 2. The Swift binary then depends on the `cc_library`. This causes a
# Swift-compatible module map to be created for the `cc_library` so that the
# Swift code can import it. Be sure to enable C++ Interoperability in the Swift
# compiler using the `-cxx-interoperability-mode` build flag.
# https://www.swift.org/documentation/cxx-interop/project-build-setup/#mixing-swift-and-c-using-other-build-systems
swift_binary(
    name = "cxx_from_swift",
    srcs = ["main.swift"],
    copts = ["-cxx-interoperability-mode=default"],
    module_name = "main",
    deps = [":counter"],
)
