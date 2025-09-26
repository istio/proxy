"""Dummy transitions for testing basic behavior"""

load("//configs:platforms.bzl", "CPU_TO_DEFAULT_PLATFORM_NAME")

_PLATFORM_TYPE_TO_CPUS_FLAG = {
    "ios": "//command_line_option:ios_multi_cpus",
    "macos": "//command_line_option:macos_cpus",
    "tvos": "//command_line_option:tvos_cpus",
    "visionos": "//command_line_option:visionos_cpus",
    "watchos": "//command_line_option:watchos_cpus",
}

_PLATFORM_TYPE_TO_DEFAULT_ARCH = {
    "ios": "x86_64",
    "macos": "x86_64",
    "tvos": "x86_64",
    "visionos": "x86_64",
    "watchos": "x86_64",
}

_CPU_TO_DEFAULT_PLATFORM_FLAG = {
    cpu: "//platforms:{}_platform".format(platform_name)
    for cpu, platform_name in CPU_TO_DEFAULT_PLATFORM_NAME.items()
}

_supports_visionos = hasattr(apple_common.platform_type, "visionos")

def _cpu_string(*, environment_arch, platform_type, settings = {}):
    if platform_type == "ios":
        if environment_arch:
            return "ios_{}".format(environment_arch)
        ios_cpus = settings["//command_line_option:ios_multi_cpus"]
        if ios_cpus:
            return "ios_{}".format(ios_cpus[0])
        cpu_value = settings["//command_line_option:cpu"]
        if cpu_value.startswith("ios_"):
            return cpu_value
        if cpu_value == "darwin_arm64":
            return "ios_sim_arm64"
        return "ios_x86_64"
    if platform_type == "macos":
        if environment_arch:
            return "darwin_{}".format(environment_arch)
        macos_cpus = settings["//command_line_option:macos_cpus"]
        if macos_cpus:
            return "darwin_{}".format(macos_cpus[0])
        cpu_value = settings["//command_line_option:cpu"]
        if cpu_value.startswith("darwin_"):
            return cpu_value
        return "darwin_x86_64"
    if platform_type == "tvos":
        if environment_arch:
            return "tvos_{}".format(environment_arch)
        tvos_cpus = settings["//command_line_option:tvos_cpus"]
        if tvos_cpus:
            return "tvos_{}".format(tvos_cpus[0])
        return "tvos_x86_64"
    if platform_type == "watchos":
        if environment_arch:
            return "watchos_{}".format(environment_arch)
        watchos_cpus = settings["//command_line_option:watchos_cpus"]
        if watchos_cpus:
            return "watchos_{}".format(watchos_cpus[0])
        return "watchos_x86_64"
    if platform_type == "visionos":
        if environment_arch:
            return "visionos_{}".format(environment_arch)
        visionos_cpus = settings["//command_line_option:visionos_cpus"]
        if visionos_cpus:
            return "visionos_{}".format(visionos_cpus[0])
        return "visionos_sim_arm64"

    fail("ERROR: Unknown platform type: {}".format(platform_type))

def _min_os_version_or_none(*, minimum_os_version, platform, platform_type):
    if platform_type == platform:
        return minimum_os_version
    return None

def _command_line_options(*, apple_platforms = [], environment_arch = None, minimum_os_version, platform_type, settings):
    cpu = _cpu_string(
        environment_arch = environment_arch,
        platform_type = platform_type,
        settings = settings,
    )

    output_dictionary = {
        "//command_line_option:apple configuration distinguisher": "applebin_" + platform_type,
        "//command_line_option:apple_platform_type": platform_type,
        "//command_line_option:apple_platforms": apple_platforms,
        # `apple_split_cpu` is used by the Bazel Apple configuration distinguisher to distinguish
        # architecture and environment, therefore we set `environment_arch` when it is available.
        "//command_line_option:apple_split_cpu": environment_arch if environment_arch else "",
        "//command_line_option:compiler": None,
        "//command_line_option:cpu": cpu,
        "//command_line_option:crosstool_top": (
            settings["//command_line_option:apple_crosstool_top"]
        ),
        "//command_line_option:fission": [],
        "//command_line_option:grte_top": None,
        "//command_line_option:platforms": (
            [apple_platforms[0]] if apple_platforms else [settings[_CPU_TO_DEFAULT_PLATFORM_FLAG[cpu]]]
        ),
        "//command_line_option:ios_minimum_os": _min_os_version_or_none(
            minimum_os_version = minimum_os_version,
            platform = "ios",
            platform_type = platform_type,
        ),
        "//command_line_option:macos_minimum_os": _min_os_version_or_none(
            minimum_os_version = minimum_os_version,
            platform = "macos",
            platform_type = platform_type,
        ),
        "//command_line_option:tvos_minimum_os": _min_os_version_or_none(
            minimum_os_version = minimum_os_version,
            platform = "tvos",
            platform_type = platform_type,
        ),
        "//command_line_option:watchos_minimum_os": _min_os_version_or_none(
            minimum_os_version = minimum_os_version,
            platform = "watchos",
            platform_type = platform_type,
        ),
    }

    return output_dictionary

_apple_platform_transition_inputs = [
    "//command_line_option:apple_crosstool_top",
    "//command_line_option:apple_platforms",
    "//command_line_option:cpu",
    "//command_line_option:incompatible_enable_apple_toolchain_resolution",
    "//command_line_option:ios_multi_cpus",
    "//command_line_option:macos_cpus",
    "//command_line_option:platforms",
    "//command_line_option:tvos_cpus",
    "//command_line_option:watchos_cpus",
] + _CPU_TO_DEFAULT_PLATFORM_FLAG.values() + (
    ["//command_line_option:visionos_cpus"] if _supports_visionos else []
)

_apple_rule_base_transition_outputs = [
    "//command_line_option:apple configuration distinguisher",
    "//command_line_option:apple_platform_type",
    "//command_line_option:apple_platforms",
    "//command_line_option:apple_split_cpu",
    "//command_line_option:compiler",
    "//command_line_option:cpu",
    "//command_line_option:crosstool_top",
    "//command_line_option:fission",
    "//command_line_option:grte_top",
    "//command_line_option:ios_minimum_os",
    "//command_line_option:macos_minimum_os",
    "//command_line_option:platforms",
    "//command_line_option:tvos_minimum_os",
    "//command_line_option:watchos_minimum_os",
]

def _apple_platform_split_transition_impl(settings, attr):
    output_dictionary = {}
    if settings["//command_line_option:incompatible_enable_apple_toolchain_resolution"]:
        platforms = (
            settings["//command_line_option:apple_platforms"] or
            settings["//command_line_option:platforms"]
        )
        # Currently there is no "default" platform for Apple-based platforms. If necessary, a
        # default platform could be generated for the rule's underlying platform_type, but for now
        # we work with the assumption that all users of the rules should set an appropriate set of
        # platforms when building Apple targets with `apple_platforms`.

        for index, platform in enumerate(platforms):
            # Create a new, reordered list so that the platform we need to resolve is always first,
            # and the other platforms will follow.
            apple_platforms = list(platforms)
            platform_to_resolve = apple_platforms.pop(index)
            apple_platforms.insert(0, platform_to_resolve)

            if str(platform) not in output_dictionary:
                output_dictionary[str(platform)] = _command_line_options(
                    apple_platforms = apple_platforms,
                    minimum_os_version = attr.minimum_os_version,
                    platform_type = attr.platform_type,
                    settings = settings,
                )

    else:
        platform_type = attr.platform_type
        environment_archs = settings[_PLATFORM_TYPE_TO_CPUS_FLAG[platform_type]]
        if not environment_archs:
            environment_archs = [_PLATFORM_TYPE_TO_DEFAULT_ARCH[platform_type]]
        for environment_arch in environment_archs:
            found_cpu = _cpu_string(
                environment_arch = environment_arch,
                platform_type = platform_type,
                settings = settings,
            )
            if found_cpu in output_dictionary:
                continue

            minimum_os_version = attr.minimum_os_version
            output_dictionary[found_cpu] = _command_line_options(
                environment_arch = environment_arch,
                minimum_os_version = minimum_os_version,
                platform_type = platform_type,
                settings = settings,
            )

    return output_dictionary

apple_platform_split_transition = transition(
    implementation = _apple_platform_split_transition_impl,
    inputs = _apple_platform_transition_inputs,
    outputs = _apple_rule_base_transition_outputs,
)
