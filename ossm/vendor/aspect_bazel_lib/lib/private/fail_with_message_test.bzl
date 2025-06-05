"Test rule that always fails and prints a message"

def _fail_with_message_test_impl(ctx):
    fail(ctx.attr.message)

fail_with_message_test = rule(
    attrs = {
        "message": attr.string(mandatory = True),
    },
    implementation = _fail_with_message_test_impl,
    test = True,
)
