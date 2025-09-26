"Helper rule for checking coverage"

load("//js/private:js_binary.bzl", "js_binary_lib")

coverage_fail_test = rule(
    implementation = js_binary_lib.implementation,
    attrs = dict(js_binary_lib.attrs, **{
        "_lcov_merger": attr.label(
            executable = True,
            default = Label("//js/private/test/coverage:fail_merger"),
            cfg = "exec",
        ),
    }),
    test = True,
    toolchains = js_binary_lib.toolchains,
)

coverage_pass_test = rule(
    implementation = js_binary_lib.implementation,
    attrs = dict(js_binary_lib.attrs, **{
        "_lcov_merger": attr.label(
            executable = True,
            default = Label("//js/private/test/coverage:pass_merger"),
            cfg = "exec",
        ),
    }),
    test = True,
    toolchains = js_binary_lib.toolchains,
)
