"Helpers for copy rules"

# Hints for Bazel spawn strategy
COPY_EXECUTION_REQUIREMENTS = {
    # ----------------+-----------------------------------------------------------------------------
    # no-sandbox      | Results in the action or test never being sandboxed; it can still be cached
    #                 | or run remotely.
    # ----------------+-----------------------------------------------------------------------------
    # See https://bazel.google.cn/reference/be/common-definitions?hl=en&authuser=0#common-attributes
    #
    # Sandboxing for this action is wasteful since there is a 1:1 mapping of input file/directory to
    # output file/directory so little room for non-hermetic inputs to sneak in to the execution.
    "no-sandbox": "1",
}
