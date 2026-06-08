load("//host:constraints.bzl", "HOST_CONSTRAINTS")

package(default_visibility = ["//visibility:private"])

sh_test(
    name = "host_constraints_test",
    srcs = ["host_constraints_test.sh"],
    env = {"ACTUAL_HOST_CONSTRAINTS": repr(HOST_CONSTRAINTS)},
    env_inherit = ["EXPECTED_HOST_CONSTRAINTS"],
)
