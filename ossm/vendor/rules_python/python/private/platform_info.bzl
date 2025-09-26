"""Helper to define a struct used to define platform metadata."""

def platform_info(
        *,
        compatible_with = [],
        flag_values = {},
        target_settings = [],
        os_name,
        arch):
    """Creates a struct of platform metadata.

    This is just a helper to ensure structs are created the same and
    the meaning/values are documented.

    Args:
        compatible_with: list[str], where the values are string labels. These
            are the target_compatible_with values to use with the toolchain
        flag_values: dict[str|Label, Any] of config_setting.flag_values
            compatible values. DEPRECATED -- use target_settings instead
        target_settings: list[str], where the values are string labels. These
            are the target_settings values to use with the toolchain.
        os_name: str, the os name; must match the name used in `@platfroms//os`
        arch: str, the cpu name; must match the name used in `@platforms//cpu`

    Returns:
        A struct with attributes and values matching the args.
    """
    return struct(
        compatible_with = compatible_with,
        flag_values = flag_values,
        target_settings = target_settings,
        os_name = os_name,
        arch = arch,
    )
