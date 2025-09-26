load(
    "//swift:swift_interop_hint.bzl",
    "swift_interop_hint",
)
load(
    "//swift:swift_library.bzl",
    "swift_library",
)
load(
    "//test/fixtures:common.bzl",
    "FIXTURE_TAGS",
    "forward_swift_info_from_swift_clang_module_aspect",
)

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "import_module_name_swift",
    srcs = ["ImportModuleName.swift"],
    tags = FIXTURE_TAGS,
    deps = [":cc_lib_custom_module_name"],
)

cc_library(
    name = "cc_lib_custom_module_name",
    hdrs = [
        "header1.h",
        "header2.h",
    ],
    aspect_hints = [":cc_lib_custom_module_name_hint"],
    tags = FIXTURE_TAGS,
)

swift_interop_hint(
    name = "cc_lib_custom_module_name_hint",
    module_name = "ModuleName",
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "import_submodule_swift",
    srcs = ["ImportSubmodule.swift"],
    tags = FIXTURE_TAGS,
    deps = [":cc_lib_submodule"],
)

cc_library(
    name = "cc_lib_submodule",
    hdrs = [
        "header1.h",
        "header2.h",
    ],
    aspect_hints = [":cc_lib_submodule_hint"],
    tags = FIXTURE_TAGS,
)

swift_interop_hint(
    name = "cc_lib_submodule_hint",
    module_map = "module.modulemap",
    module_name = "TopModule",
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "invalid_swift",
    srcs = ["ImportSubmodule.swift"],
    tags = FIXTURE_TAGS,
    deps = [":cc_lib_invalid"],
)

cc_library(
    name = "cc_lib_invalid",
    hdrs = [
        "header1.h",
        "header2.h",
    ],
    aspect_hints = [":cc_lib_invalid_hint"],
    tags = FIXTURE_TAGS,
)

swift_interop_hint(
    name = "cc_lib_invalid_hint",
    module_map = "module.modulemap",
    tags = FIXTURE_TAGS,
)

forward_swift_info_from_swift_clang_module_aspect(
    name = "objc_library_suppressed",
    tags = FIXTURE_TAGS,
    target = ":objc_library_suppressed_lib",
    target_compatible_with = ["@platforms//os:macos"],
)

objc_library(
    name = "objc_library_suppressed_lib",
    hdrs = ["header1.h"],
    aspect_hints = ["//swift:no_module"],
    tags = FIXTURE_TAGS,
    target_compatible_with = ["@platforms//os:macos"],
)

forward_swift_info_from_swift_clang_module_aspect(
    name = "objc_library_with_swift_dep_suppressed",
    tags = FIXTURE_TAGS,
    target = ":objc_library_with_swift_dep_suppressed_lib",
    target_compatible_with = ["@platforms//os:macos"],
)

objc_library(
    name = "objc_library_with_swift_dep_suppressed_lib",
    hdrs = ["header1.h"],
    aspect_hints = ["//swift:no_module"],
    tags = FIXTURE_TAGS,
    target_compatible_with = ["@platforms//os:macos"],
    deps = [":empty_lib"],
)

swift_library(
    name = "empty_lib",
    srcs = ["Empty.swift"],
    tags = FIXTURE_TAGS,
)
