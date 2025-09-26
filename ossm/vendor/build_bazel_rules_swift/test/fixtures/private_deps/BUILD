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
# Fixtures for testing swift_libraries that are private deps of a swift_library

swift_library(
    name = "private_swift",
    srcs = ["Empty.swift"],
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "public_swift",
    srcs = ["Empty.swift"],
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "client_swift_deps",
    srcs = ["Empty.swift"],
    generates_header = True,
    private_deps = [
        ":private_swift",
    ],
    tags = FIXTURE_TAGS,
    deps = [
        ":public_swift",
    ],
)

###############################################################################
# Fixtures for testing cc_libraries that are private deps of a swift_library

cc_library(
    name = "private_cc",
    srcs = ["private.c"],
    hdrs = ["private.h"],
    aspect_hints = ["//swift:auto_module"],
    features = [
        # A bit hacky, but by claiming we don't support PIC, we can get the
        # output libraries in `libraries_to_link.static_library` instead of
        # `pic_static_library`, so that the test doesn't have to worry about
        # the specific toolchain configuration.
        "-pic",
        "-supports_pic",
    ],
    tags = FIXTURE_TAGS,
)

cc_library(
    name = "public_cc",
    srcs = ["public.c"],
    hdrs = ["public.h"],
    aspect_hints = ["//swift:auto_module"],
    features = [
        # See the comment in the target above.
        "-pic",
        "-supports_pic",
    ],
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "client_cc_deps",
    srcs = ["Empty.swift"],
    # Suppress the generated header/module map on platforms that support
    # Objective-C interop so that we don't have to worry about
    # conditionally checking for it in the transitive modules on just those
    # platforms.
    generates_header = False,
    private_deps = [
        ":private_cc",
    ],
    tags = FIXTURE_TAGS,
    deps = [
        ":public_cc",
    ],
)

# TODO(allevato): Add tests for `objc_library` targets that are private deps
# of a `swift_library`. We can't do this today because `ObjcProvider` doesn't
# distinguish compilation and linking info, which means we can't easily merge
# the linking parts that are necessary for `apple_binary` without also merging
# the header/module map information that we don't want to propagate.
