"""This file provides all user facing functions.
"""

load("//internal:rules.bzl", _shellcheck_test = "shellcheck_test")

def shellcheck_test(name, data, **kwargs):
    """shellcheck_test takes the files to be checked as 'data'

    Args:
        name: The name of the rule.
        data: The list of files to be checked using shellcheck.
        **kwargs: Forwarded kwargs to the underlying rule.
    """
    kwargs.pop("expect_fail", True)
    return _shellcheck_test(
        name = name,
        data = data,
        **kwargs
    )
