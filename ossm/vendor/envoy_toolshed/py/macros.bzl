load("@rules_python//python:defs.bzl", "py_binary", "py_library")

def entry_point(
        name,
        pkg,
        entry_point_script = "@envoy_toolshed//py:entry_point.py",
        entry_point_alias = None,
        script = None,
        data = None,
        deps = None,
        args = None,
        visibility = None):
    """This macro provides the convenience of using an `entry_point` while
    also being able to create a rule with associated `args` and `data`, as is
    possible with the normal `py_binary` rule.

    We may wish to remove this macro should https://github.com/bazelbuild/rules_python/issues/600
    be resolved.

    The `script` and `pkg` args are passed directly to the `entry_point`.

    By default, the pip `entry_point` from `@base_pip3` is used. You can provide
    a custom `entry_point` if eg you want to provide an `entry_point` with dev
    requirements, or from some other requirements set.

    A `py_binary` is dynamically created to wrap the `entry_point` with provided
    `args` and `data`.
    """
    if not entry_point_alias:
        fail("entry_point_alias must be defined")

    actual_entry_point = entry_point_alias(
        pkg = pkg,
        script = script or pkg,
    )
    entry_point_py = "entry_point_%s_main.py" % name
    entry_point_wrapper = "entry_point_%s_wrapper" % name
    entry_point_path = "$(location %s)" % entry_point_script
    entry_point_alias = "$(location %s)" % actual_entry_point

    native.genrule(
        name = entry_point_wrapper,
        cmd = """
        sed s#_ENTRY_POINT_ALIAS_#%s# %s > \"$@\"
        """ % (entry_point_alias, entry_point_path),
        tools = [
            actual_entry_point,
            entry_point_script,
        ],
        outs = [entry_point_py],
    )

    py_binary(
        name = name,
        srcs = [entry_point_wrapper, actual_entry_point],
        main = entry_point_py,
        args = (args or []),
        data = (data or []),
        deps = (deps or []),
        visibility = (visibility or []),
    )
