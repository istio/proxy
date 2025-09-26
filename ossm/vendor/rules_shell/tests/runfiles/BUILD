load("@rules_shellcheck//:def.bzl", "shellcheck_test")
load("//shell:sh_test.bzl", "sh_test")

sh_test(
    name = "runfiles_test",
    srcs = ["runfiles_test.bash"],
    deps = ["//shell/runfiles"],
)

shellcheck_test(
    name = "runfiles_shellcheck_test",
    data = ["//shell/runfiles:runfiles.bash"],
    format = "gcc",
    severity = "warning",
)
