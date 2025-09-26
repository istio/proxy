load("//swift:swift_library.bzl", "swift_library")
load(
    "//test/fixtures:common.bzl",
    "FIXTURE_TAGS",
)
load(
    "//test/rules:swift_library_artifact_collector.bzl",
    "swift_library_artifact_collector",
)

package(
    default_testonly = True,
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

# Checking in pre-built artifacts like a `.swiftinterface` and static libraries
# would require different artifacts for every platform the test might run on.
# Instead, build it on-demand but forward the outputs using the "artifact
# collector" rule below to make them act as if they were pre-built outputs when
# referenced by the `swift_import` rule.
#
# These must be in a separate package than the `swift_import` target because
# that rule propagates its pre-built inputs in `DefaultInfo`.

swift_library(
    name = "private_swiftinterface_library",
    srcs = ["Lib.swift"],
    library_evolution = True,
    module_name = "PrivateSwiftInterface",
    tags = FIXTURE_TAGS,
)

swift_library_artifact_collector(
    name = "private_swiftinterface_artifact_collector",
    private_swiftinterface = "private_swiftinterface_outputs/PrivateSwiftInterface.private.swiftinterface",
    static_library = "private_swiftinterface_outputs/libPrivateSwiftInterface.a",
    swiftdoc = "private_swiftinterface_outputs/PrivateSwiftInterface.swiftdoc",
    swiftinterface = "private_swiftinterface_outputs/PrivateSwiftInterface.swiftinterface",
    tags = FIXTURE_TAGS,
    target = ":private_swiftinterface_library",
    target_compatible_with = ["@platforms//os:macos"],
)

swift_library(
    name = "private_swiftinterface_without_library_evolution",
    srcs = ["Lib.swift"],
    library_evolution = False,
    module_name = "PrivateSwiftInterface",
    tags = FIXTURE_TAGS,
)
