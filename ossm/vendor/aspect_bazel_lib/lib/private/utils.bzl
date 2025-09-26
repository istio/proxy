"""General utility functions"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", _http_archive = "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def _propagate_well_known_tags(tags = []):
    """Returns a list of tags filtered from the input set that only contains the ones that are considered "well known"

    These are listed in Bazel's documentation:
    https://docs.bazel.build/versions/main/test-encyclopedia.html#tag-conventions
    https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes

    Args:
        tags: List of tags to filter

    Returns:
        List of tags that only contains the well known set
    """

    WELL_KNOWN_TAGS = [
        "no-sandbox",
        "no-cache",
        "no-remote-cache",
        "no-remote-exec",
        "no-remote",
        "local",
        "requires-network",
        "block-network",
        "requires-fakeroot",
        "exclusive",
        "manual",
        "external",
    ]

    # cpu:n tags allow setting the requested number of CPUs for a test target.
    # More info at https://docs.bazel.build/versions/main/test-encyclopedia.html#other-resources
    CPU_PREFIX = "cpu:"

    return [
        tag
        for tag in tags
        if tag in WELL_KNOWN_TAGS or tag.startswith(CPU_PREFIX)
    ]

def _to_label(param):
    """Converts a string to a Label. If Label is supplied, the same label is returned.

    Args:
        param: a string representing a label or a Label

    Returns:
        a Label
    """
    root_repo = "@@" if _is_bazel_6_or_greater() else "@"
    param_type = type(param)
    if param_type == "string":
        if param.startswith("@"):
            return Label(param)
        if param.startswith("//"):
            return Label("{}{}".format(root_repo, param))
        if param.startswith(":"):
            param = param[1:]
        return Label("{}//{}:{}".format(root_repo, native.package_name(), param))
    elif param_type == "Label":
        return param
    else:
        msg = "Expected 'string' or 'Label' but got '{}'".format(param_type)
        fail(msg)

def _consistent_label_str(ctx, label):
    """Generate a consistent label string for all Bazel versions.

    Starting in Bazel 6, the workspace name is empty for the local workspace and there's no other
    way to determine it. This behavior differs from Bazel 5 where the local workspace name was fully
    qualified in str(label).

    This utility function is meant for use in rules and requires the rule context to determine the
    user's workspace name (`ctx.workspace_name`).

    Args:
        ctx: The rule context.
        label: A Label.

    Returns:
        String representation of the label including the repository name if the label is from an
        external repository. For labels in the user's repository the label will start with `@//`.
    """
    return "@{}//{}:{}".format(
        "" if label.workspace_name == ctx.workspace_name else label.workspace_name,
        label.package,
        label.name,
    )

def _is_external_label(param):
    """Returns True if the given Label (or stringy version of a label) represents a target outside of the workspace

    Args:
        param: a string or label

    Returns:
        a bool
    """
    if not _is_bazel_6_or_greater() and str(param).startswith("@@//"):
        # Work-around for https://github.com/bazelbuild/bazel/issues/16528
        return False
    return len(_to_label(param).workspace_root) > 0

# Path to the root of the workspace
def _path_to_workspace_root():
    """ Returns the path to the workspace root under bazel

    Returns:
        Path to the workspace root
    """
    return "/".join([".."] * len(native.package_name().split("/")))

# Like glob() but returns directories only
def _glob_directories(include, **kwargs):
    all = native.glob(include, exclude_directories = 0, **kwargs)
    files = native.glob(include, **kwargs)
    directories = [p for p in all if p not in files]
    return directories

def _file_exists(path):
    """Check whether a file exists.

    Useful in macros to set defaults for a configuration file if it is present.
    This can only be called during the loading phase, not from a rule implementation.

    Args:
        path: a label, or a string which is a path relative to this package
    """
    label = _to_label(path)
    file_abs = "%s/%s" % (label.package, label.name)
    file_rel = file_abs[len(native.package_name()) + 1:]
    file_glob = native.glob([file_rel], exclude_directories = 1, allow_empty = True)
    return len(file_glob) > 0

def _default_timeout(size, timeout):
    """Provide a sane default for *_test timeout attribute.

    The [test-encyclopedia](https://bazel.build/reference/test-encyclopedia) says:

    > Tests may return arbitrarily fast regardless of timeout.
    > A test is not penalized for an overgenerous timeout, although a warning may be issued:
    > you should generally set your timeout as tight as you can without incurring any flakiness.

    However Bazel's default for timeout is medium, which is dumb given this guidance.

    It also says:

    > Tests which do not explicitly specify a timeout have one implied based on the test's size as follows

    Therefore if size is specified, we should allow timeout to take its implied default.
    If neither is set, then we can fix Bazel's wrong default here to avoid warnings under
    `--test_verbose_timeout_warnings`.

    This function can be used in a macro which wraps a testing rule.

    Args:
        size: the size attribute of a test target
        timeout: the timeout attribute of a test target

    Returns:
        "short" if neither is set, otherwise timeout
    """

    if size == None and timeout == None:
        return "short"

    return timeout

def _is_bazel_6_or_greater():
    """Detects if the Bazel version being used is greater than or equal to 6 (including Bazel 6 pre-releases and RCs).

    Detecting Bazel 6 or greater is particularly useful in rules as slightly different code paths may be needed to
    support bzlmod which was added in Bazel 6.

    Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
    be used in rules and BUILD files.

    An alternate approach to make the Bazel version available in BUILD files and rules would be to
    use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
    which contains the bazel_version in the exported `host` struct:

    WORKSPACE:
    ```
    load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
    host_repo(name = "aspect_bazel_lib_host")
    ```

    BUILD.bazel:
    ```
    load("@aspect_bazel_lib_host//:defs.bzl", "host")
    print(host.bazel_version)
    ```

    That approach, however, incurs a cost in the user's WORKSPACE.

    Returns:
        True if the Bazel version being used is greater than or equal to 6 (including pre-releases and RCs)
    """

    # Hacky way to check if the we're using at least Bazel 6. Would be nice if there was a ctx.bazel_version instead.
    # native.bazel_version only works in repository rules.
    return "apple_binary" not in dir(native)

def _is_bazel_7_or_greater():
    """Detects if the Bazel version being used is greater than or equal to 7 (including Bazel 7 pre-releases and RCs).

    Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
    be used in rules and BUILD files.

    An alternate approach to make the Bazel version available in BUILD files and rules would be to
    use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
    which contains the bazel_version in the exported `host` struct:

    WORKSPACE:
    ```
    load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
    host_repo(name = "aspect_bazel_lib_host")
    ```

    BUILD.bazel:
    ```
    load("@aspect_bazel_lib_host//:defs.bzl", "host")
    print(host.bazel_version)
    ```

    That approach, however, incurs a cost in the user's WORKSPACE.

    Returns:
        True if the Bazel version being used is greater than or equal to 7 (including pre-releases and RCs)
    """

    # Hacky way to check if the we're using at least Bazel 7. Would be nice if there was a ctx.bazel_version instead.
    # native.bazel_version only works in repository rules.
    return "apple_binary" not in dir(native) and "cc_host_toolchain_alias" not in dir(native)

def is_bzlmod_enabled():
    """Detect the value of the --enable_bzlmod flag"""
    return str(Label("@//:BUILD.bazel")).startswith("@@")

def _maybe_http_archive(**kwargs):
    """Adapts a maybe(http_archive, ...) to look like an http_archive.

    This makes WORKSPACE dependencies easier to read and update.

    Typical usage looks like,

    ```
    load("//lib:utils.bzl", http_archive = "maybe_http_archive")

    http_archive(
        name = "aspect_rules_js",
        sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
        strip_prefix = "rules_js-1.6.2",
        url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
    )
    ```

    instead of the classic maybe pattern of,

    ```
    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
    load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

    maybe(
        http_archive,
        name = "aspect_rules_js",
        sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
        strip_prefix = "rules_js-1.6.2",
        url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
    )
    ```

    Args:
      **kwargs: all arguments to pass-forward to http_archive
    """
    maybe(_http_archive, **kwargs)

_COMMON_RULE_ATTRIBUTES = [
    "compatible_with",
    "deprecation",
    "distribs",
    "exec_compatible_with",
    "exec_properties",
    "features",
    "restricted_to",
    "tags",
    "target_compatible_with",
    "testonly",
    "toolchains",
    "visibility",
]

_COMMON_TEST_RULE_ATTRIBUTES = _COMMON_RULE_ATTRIBUTES + [
    "args",
    "env",
    "env_inherit",
    "size",
    "timeout",
    "flaky",
    "shard_count",
    "local",
]

_COMMON_BINARY_RULE_ATTRIBUTES = _COMMON_RULE_ATTRIBUTES + [
    "args",
    "env",
    "output_licenses",
]

def _propagate_common_rule_attributes(attrs):
    """Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all rules

    These are listed in Bazel's documentation:
    https://bazel.build/reference/be/common-definitions#common-attributes

    Args:
        attrs: Dict of parameters to filter

    Returns:
        The dict of parameters, containing only common attributes
    """

    return {
        k: attrs[k]
        for k in attrs
        if k in _COMMON_RULE_ATTRIBUTES
    }

def _propagate_common_test_rule_attributes(attrs):
    """Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all test rules

    These are listed in Bazel's documentation:
    https://bazel.build/reference/be/common-definitions#common-attributes
    https://bazel.build/reference/be/common-definitions#common-attributes-tests

    Args:
        attrs: Dict of parameters to filter

    Returns:
        The dict of parameters, containing only common test attributes
    """

    return {
        k: attrs[k]
        for k in attrs
        if k in _COMMON_TEST_RULE_ATTRIBUTES
    }

def _propagate_common_binary_rule_attributes(attrs):
    """Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all binary rules

    These are listed in Bazel's documentation:
    https://bazel.build/reference/be/common-definitions#common-attributes
    https://bazel.build/reference/be/common-definitions#common-attributes-binary

    Args:
        attrs: Dict of parameters to filter

    Returns:
        The dict of parameters, containing only common binary attributes
    """

    return {
        k: attrs[k]
        for k in attrs
        if k in _COMMON_RULE_ATTRIBUTES or k in _COMMON_BINARY_RULE_ATTRIBUTES
    }

utils = struct(
    default_timeout = _default_timeout,
    file_exists = _file_exists,
    glob_directories = _glob_directories,
    is_bazel_6_or_greater = _is_bazel_6_or_greater,
    is_bazel_7_or_greater = _is_bazel_7_or_greater,
    is_bzlmod_enabled = is_bzlmod_enabled,
    is_external_label = _is_external_label,
    maybe_http_archive = _maybe_http_archive,
    path_to_workspace_root = _path_to_workspace_root,
    propagate_well_known_tags = _propagate_well_known_tags,
    propagate_common_rule_attributes = _propagate_common_rule_attributes,
    propagate_common_test_rule_attributes = _propagate_common_test_rule_attributes,
    propagate_common_binary_rule_attributes = _propagate_common_binary_rule_attributes,
    to_label = _to_label,
    consistent_label_str = _consistent_label_str,
)
