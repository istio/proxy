"""`py_zipapp_test` macro.

:::{seealso}

{obj}`features.zipapp_rules` to detect if this rule is available.
:::
"""

load("//python/private:util.bzl", "add_tag")
load("//python/private/zipapp:py_zipapp_rule.bzl", _py_zipapp_test = "py_zipapp_test")

def py_zipapp_test(**kwargs):
    """Builds a Python zipapp from a py_binary/py_test target.

    :::{versionadded} 1.9.0
    :::

    Args:
        **kwargs: Args passed onto {rule}`py_zipapp_test`.
    """
    add_tag(kwargs, "@rules_python//python:py_zipapp_test")
    _py_zipapp_test(
        **kwargs
    )
