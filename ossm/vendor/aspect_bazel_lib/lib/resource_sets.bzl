"""Utilities for rules that expose resource_set on ctx.actions.run[_shell]

Workaround for https://github.com/bazelbuild/bazel/issues/15187

Note, this workaround only provides some fixed values for either CPU or Memory.

Rule authors who are ALSO the BUILD author might know better, and can
write custom resource_set functions for use within their own repository.
This seems to be the use case that Google engineers imagined.
"""

resource_set_values = [
    "cpu_2",
    "cpu_4",
    "default",
    "mem_512m",
    "mem_1g",
    "mem_2g",
    "mem_4g",
    "mem_8g",
    "mem_16g",
    "mem_32g",
]

def _resource_set_cpu_2(_, __):
    return {"cpu": 2}

def _resource_set_cpu_4(_, __):
    return {"cpu": 4}

def _resource_set_mem_512m(_, __):
    return {"memory": 512}

def _resource_set_mem_1g(_, __):
    return {"memory": 1024}

def _resource_set_mem_2g(_, __):
    return {"memory": 2048}

def _resource_set_mem_4g(_, __):
    return {"memory": 4096}

def _resource_set_mem_8g(_, __):
    return {"memory": 8192}

def _resource_set_mem_16g(_, __):
    return {"memory": 16384}

def _resource_set_mem_32g(_, __):
    return {"memory": 32768}

# buildifier: disable=function-docstring
def resource_set(attr):
    if attr.resource_set == "cpu_2":
        return _resource_set_cpu_2
    if attr.resource_set == "cpu_4":
        return _resource_set_cpu_4
    if attr.resource_set == "default":
        return None
    if attr.resource_set == "mem_512m":
        return _resource_set_mem_512m
    if attr.resource_set == "mem_1g":
        return _resource_set_mem_1g
    if attr.resource_set == "mem_2g":
        return _resource_set_mem_2g
    if attr.resource_set == "mem_4g":
        return _resource_set_mem_4g
    if attr.resource_set == "mem_8g":
        return _resource_set_mem_8g
    if attr.resource_set == "mem_16g":
        return _resource_set_mem_16g
    if attr.resource_set == "mem_32g":
        return _resource_set_mem_32g
    fail("unknown resource set", attr.resource_set)

resource_set_attr = {
    "resource_set": attr.string(
        doc = """A predefined function used as the resource_set for actions.

        Used with --experimental_action_resource_set to reserve more RAM/CPU, preventing Bazel overscheduling resource-intensive actions.

        By default, Bazel allocates 1 CPU and 250M of RAM.
        https://github.com/bazelbuild/bazel/blob/058f943037e21710837eda9ca2f85b5f8538c8c5/src/main/java/com/google/devtools/build/lib/actions/AbstractAction.java#L77
        """,
        default = "default",
        values = resource_set_values,
    ),
}
