load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_library_group.bzl", "swift_library_group")
load("//swift:swift_test.bzl", "swift_test")

swift_library(
    name = "TestHelpers",
    srcs = ["TestHelper.swift"],
    always_include_developer_search_paths = True,
    module_name = "TestHelpers",
    tags = ["manual"],
)

swift_library(
    name = "StringHelpers",
    srcs = ["String+RandomExtensions.swift"],
    module_name = "StringHelpers",
    tags = ["manual"],
    visibility = ["//:__subpackages__"],
)

swift_library_group(
    name = "Helpers",
    tags = ["manual"],
    deps = [
        ":StringHelpers",
        ":TestHelpers",
    ],
)

swift_test(
    name = "DemoHelpersTest",
    srcs = [
        "DemoTestHelperTest.swift",
        "String+RandomExtensionsTests.swift",
    ],
    deps = [":Helpers"],
)
