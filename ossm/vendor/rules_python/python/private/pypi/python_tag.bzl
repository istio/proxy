"A simple utility function to get the python_tag from the implementation name"

load("//python/private:version.bzl", "version")

# Taken from
# https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/#python-tag
_PY_TAGS = {
    # "py": Generic Python (does not require implementation-specific features)
    "cpython": "cp",
    "ironpython": "ip",
    "jython": "jy",
    "pypy": "pp",
    "python": "py",
}
PY_TAG_GENERIC = "py"

def python_tag(implementation_name, python_version = ""):
    """Get the python_tag from the implementation_name.

    Args:
        implementation_name: {type}`str` the implementation name, e.g. "cpython"
        python_version: {type}`str` a version who can be parsed using PEP440 compliant
            parser.

    Returns:
        A {type}`str` that represents the python_tag with a version if the
            python_version is given.
    """
    if python_version:
        v = version.parse(python_version, strict = True)
        suffix = "{}{}".format(
            v.release[0],
            v.release[1] if len(v.release) > 1 else "",
        )
    else:
        suffix = ""

    return "{}{}".format(
        _PY_TAGS.get(implementation_name, implementation_name),
        suffix,
    )
