load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_test.bzl", "swift_test")

swift_library(
    name = "test_filter_lib",
    testonly = True,
    srcs = [
        "FailTests.swift",
        "PassFailTests.swift",
        "PassTests.swift",
        "TestHelper.swift",
    ],
    module_name = "test_filter",
)

# Verify that tests fail as expected without test filtering.
swift_test(
    name = "test_filter__baseline",
    env = {
        "EXPECT_FAILURE": "TRUE",
    },
    module_name = "test_filter__baseline",
    deps = [":test_filter_lib"],
)

# Verify that test scope is filtered by TARGET.TEST_CLASS as expected.
swift_test(
    name = "test_filter__feature__target_class",
    env = {
        "TESTBRIDGE_TEST_ONLY": "test_filter.PassTests",
    },
    module_name = "test_filter__feature__target_class",
    deps = [":test_filter_lib"],
)

# Verify that test scope is filtered by TARGET.TEST_CLASS.TEST_METHOD as expected.
swift_test(
    name = "test_filter__feature__target_class_method",
    env = {
        "TESTBRIDGE_TEST_ONLY": "test_filter.PassFailTests/test_pass",
    },
    module_name = "test_filter__feature__target_class_method",
    deps = [":test_filter_lib"],
)
