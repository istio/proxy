# Standard constraint_settings and constraint_values for items common to rules_swift and rules_apple.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

filegroup(
    name = "srcs",
    srcs = ["BUILD"],
)

# Constraint indicating the target environment (e.g. device, simulator).
constraint_setting(
    name = "target_environment",
    default_constraint_value = ":device",
    visibility = ["//visibility:private"],
)

constraint_value(
    name = "device",
    constraint_setting = ":target_environment",
)

constraint_value(
    name = "simulator",
    constraint_setting = ":target_environment",
)

constraint_value(
    name = "catalyst",
    constraint_setting = ":target_environment",
)

# Constraint indicating if the platform belongs to an "Apple" platform (iOS, watchOS, tvOS, etc.)
constraint_setting(
    name = "target_vendor",
    default_constraint_value = ":apple",
    visibility = ["//visibility:private"],
)

constraint_value(
    name = "apple",
    constraint_setting = ":target_vendor",
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = True,
    srcs = glob(["**"]),
    visibility = ["//:__pkg__"],
)
