load(
    "//swift:swift_interop_hint.bzl",
    "swift_interop_hint",
)
load(
    "//swift:swift_library.bzl",
    "swift_library",
)
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

###############################################################################
# Fixtures for testing complex `cc_library` interop cases.

cc_library(
    name = "prefix_only",
    hdrs = [
        "header.h",
        "header_prefix_only.h",
    ],
    aspect_hints = ["//swift:auto_module"],
    include_prefix = "some/prefix",
    tags = FIXTURE_TAGS,
)

cc_library(
    name = "strip_prefix_only",
    hdrs = [
        "header.h",
        "header_strip_prefix_only.h",
    ],
    aspect_hints = ["//swift:auto_module"],
    strip_include_prefix = "//test",
    tags = FIXTURE_TAGS,
)

cc_library(
    name = "prefix_and_strip_prefix",
    hdrs = [
        "header.h",
        "header_prefix_and_strip_prefix.h",
    ],
    aspect_hints = ["//swift:auto_module"],
    include_prefix = "some/prefix",
    strip_include_prefix = "//test",
    tags = FIXTURE_TAGS,
)

cc_library(
    name = "prefix_and_strip_prefix_with_exclusion",
    hdrs = [
        "header.h",
        "header_with_error.h",
    ],
    aspect_hints = [":prefix_and_strip_prefix_with_exclusion_swift_hint"],
    include_prefix = "some/prefix",
    strip_include_prefix = "//test",
    tags = FIXTURE_TAGS,
)

swift_interop_hint(
    name = "prefix_and_strip_prefix_with_exclusion_swift_hint",
    exclude_hdrs = ["header_with_error.h"],
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "import_prefix_only",
    srcs = ["ImportPrefixOnly.swift"],
    tags = FIXTURE_TAGS,
    deps = [":prefix_only"],
)

swift_library(
    name = "import_strip_prefix_only",
    srcs = ["ImportStripPrefixOnly.swift"],
    tags = FIXTURE_TAGS,
    deps = [":strip_prefix_only"],
)

swift_library(
    name = "import_prefix_and_strip_prefix",
    srcs = ["ImportPrefixAndStripPrefix.swift"],
    tags = FIXTURE_TAGS,
    deps = [":prefix_and_strip_prefix"],
)

swift_library(
    name = "import_prefix_and_strip_prefix_with_exclusion",
    srcs = ["ImportPrefixAndStripPrefixWithExclusion.swift"],
    tags = FIXTURE_TAGS,
    deps = [":prefix_and_strip_prefix_with_exclusion"],
)
