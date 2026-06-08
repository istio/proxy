"""A common platform structure for using internally."""

def platform(*, name, arch_name, os_name, config_settings = [], env = {}, marker = "", whl_abi_tags = [], whl_platform_tags = []):
    """A platform structure for using internally.

    Args:
        name: {type}`str` the human friendly name of the platform.
        arch_name: {type}`str` the @platforms//cpu:<arch_name> value.
        os_name: {type}`str` the @platforms//os:<os_name> value.
        config_settings: {type}`list[Label|str]` The list of labels for selecting the
            platform.
        env: {type}`dict[str, str]` the PEP508 environment for marker evaluation.
        marker: {type}`str` the env marker expression that is evaluated to determine if we
            should use the platform. This is useful to turn on certain platforms for
            particular python versions.
        whl_abi_tags: {type}`list[str]` A list of values for matching abi tags.
        whl_platform_tags: {type}`list[str]` A list of values for matching platform tags.

    Returns:
        struct with the necessary values for pipstar implementation.
    """

    # NOTE @aignas 2025-07-08: the least preferred is the first item in the list
    if "any" not in whl_platform_tags:
        # the lowest priority one needs to be the first one
        whl_platform_tags = ["any"] + whl_platform_tags

    whl_abi_tags = whl_abi_tags or ["abi3", "cp{major}{minor}"]
    if "none" not in whl_abi_tags:
        # the lowest priority one needs to be the first one
        whl_abi_tags = ["none"] + whl_abi_tags

    return struct(
        name = name,
        arch_name = arch_name,
        os_name = os_name,
        config_settings = config_settings,
        env = {
            # defaults for env
            "implementation_name": "cpython",
        } | env,
        marker = marker,
        whl_abi_tags = whl_abi_tags,
        whl_platform_tags = whl_platform_tags,
    )
