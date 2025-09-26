"Implementation of the js_run_devserver_test rule (the 'test' version of js_run_devserver)"

load("//js/private:js_run_devserver.bzl", "js_run_devserver", "js_run_devserver_lib")

# 'test' version of the _js_run_devserver` rule
_js_run_devserver_test = rule(
    attrs = js_run_devserver_lib.attrs,
    implementation = js_run_devserver_lib.implementation,
    toolchains = js_run_devserver_lib.toolchains,
    test = True,
)

def js_run_devserver_test(
        name,
        tags = [],
        **kwargs):
    """
    'Test' version of the `js_run_devserver` macro.

    Provides the test rule to the macro, along with the 'no-sandbox' tag in
    order to properly simulate the js_run_devserver environment of 'bazel run'.

    Args:
        name: A unique name for this target.

        tags: Additional Bazel tags to supply to the rule.

        **kwargs: All other args for `js_run_devserver`.

            See https://docs.aspect.build/rules/aspect_rules_js/docs/js_run_devserver
    """
    js_run_devserver(
        name,
        # Override the rule to execute
        rule_to_execute = _js_run_devserver_test,
        # 'no-sandbox' needed to simulate 'bazel run' command - normally tests
        # are sandboxed, but sandboxing doesn't exhibit the issue in
        # https://github.com/aspect-build/rules_js/issues/1204
        tags = tags + ["no-sandbox"],
        **kwargs
    )
