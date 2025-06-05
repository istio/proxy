load("//swift:swift_binary.bzl", "swift_binary")

package(
    default_visibility = ["//test:__subpackages__"],
)

swift_binary(
    name = "main",
    srcs = ["main.swift"],
)

swift_binary(
    name = "custommain",
    srcs = ["custommain.swift"],
)

swift_binary(
    name = "multiplefiles",
    srcs = [
        "file1.swift",
        "file2.swift",
    ],
)
