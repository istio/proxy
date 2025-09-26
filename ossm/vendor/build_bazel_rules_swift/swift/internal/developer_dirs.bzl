""" Functions to fetch information about developer frameworks """

load("@bazel_skylib//lib:paths.bzl", "paths")

def platform_developer_framework_dir(developer_dirs):
    for developer_dir in developer_dirs:
        if developer_dir.developer_path_label == "platform":
            return developer_dir.path
    return None

def swift_developer_lib_dir(developer_dirs):
    """Returns the directory containing extra Swift developer libraries.

    Args:
        developer_dirs: A `list` of `structs`

    Returns:
        The directory containing extra Swift-specific development libraries and
        swiftmodules.
    """
    platform_framework_dir = platform_developer_framework_dir(developer_dirs)
    if platform_framework_dir:
        return paths.join(
            paths.dirname(paths.dirname(platform_framework_dir)),
            "usr",
            "lib",
        )
    return None

def developer_dirs_linkopts(developer_dirs):
    """ Returns linkopts for the provided developer directories.

    Args:
        developer_dirs: A `list` of `structs`

    Returns:
        A `list` of linker options
    """
    swift_developer_lib_dir_path = swift_developer_lib_dir(
        developer_dirs,
    )
    return [
        "-L%s" % swift_developer_lib_dir_path,
    ] + [
        "-F%s" % developer_framework.path
        for developer_framework in developer_dirs
    ]
