load(
    "//swift:swift_library.bzl",
    "swift_library",
)

swift_library(
    name = "test_observer",
    testonly = True,
    srcs = [
        "BazelXMLTestObserver.swift",
        "BazelXMLTestObserverRegistration.swift",
        "StringInterpolation+XMLEscaping.swift",
    ],
    module_name = "BazelTestObservation",
    visibility = ["//visibility:public"],
)
