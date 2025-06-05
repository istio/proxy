"""Tests verifying produced binaries"""

load(
    "//test/rules:apple_verification_test.bzl",
    "apple_verification_test",
)

def binary_test_suite(name):
    """Test various aspects of binary generation

    Args:
        name: The prefix of each test name
    """

    apple_verification_test(
        name = "{}_macos_binary_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"macos_cpus": "x86_64"},
        expected_platform_type = "macos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:macos_binary",
    )

    apple_verification_test(
        name = "{}_macos_arm64e_binary_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"macos_cpus": "arm64e"},
        expected_platform_type = "macos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:macos_binary",
    )

    apple_verification_test(
        name = "{}_macos_binary_with_spaces_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"macos_cpus": "x86_64"},
        expected_platform_type = "macos",
        generate_dsym = True,
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:macos_binary_with_spaces",
    )

    apple_verification_test(
        name = "{}_visionos_device_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"visionos_cpus": "arm64"},
        expected_platform_type = "visionos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:visionos_binary",
    )

    apple_verification_test(
        name = "{}_visionos_arm64_simulator_test".format(name),
        tags = [name],
        build_type = "simulator",
        cpus = {"visionos_cpus": "sim_arm64"},
        expected_platform_type = "visionos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:visionos_binary",
    )

    apple_verification_test(
        name = "{}_unused_symbol_is_kept_by_default".format(name),
        build_type = "simulator",
        cpus = {"ios_multi_cpus": "x86_64"},
        compilation_mode = "fastbuild",
        objc_enable_binary_stripping = False,
        verifier_script = "//test:verify_unused_symbol_exists.sh",
        target_under_test = "//test/test_data:ios_app_with_unused_symbol",
        tags = [name],
    )

    apple_verification_test(
        name = "{}_unused_symbol_is_stripped".format(name),
        build_type = "simulator",
        cpus = {"ios_multi_cpus": "x86_64"},
        compilation_mode = "opt",
        objc_enable_binary_stripping = True,
        verifier_script = "//test:verify_stripped_symbols.sh",
        target_under_test = "//test/test_data:ios_app_with_unused_symbol",
        tags = [name],
    )

    apple_verification_test(
        name = "{}_archive_timestamps".format(name),
        build_type = "simulator",
        cpus = {"ios_multi_cpus": "x86_64"},
        verifier_script = "//test:verify_archive_timestamps.sh",
        target_under_test = "//test/test_data:static_lib",
        tags = [name],
    )

    apple_verification_test(
        name = "{}_fat_static_lib".format(name),
        build_type = "simulator",
        cpus = {"ios_multi_cpus": "x86_64,sim_arm64"},
        expected_platform_type = "ios",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:static_lib",
        tags = [name],
    )

    apple_verification_test(
        name = "{}_watchos_device_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"watchos_cpus": "x86_64"},
        expected_platform_type = "watchos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:watch_binary",
    )

    apple_verification_test(
        name = "{}_watchos_device_arm64_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"watchos_cpus": "device_arm64"},
        expected_platform_type = "watchos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:watch_binary",
    )

    apple_verification_test(
        name = "{}_watchos_device_arm64e_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"watchos_cpus": "device_arm64e"},
        expected_platform_type = "watchos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:watch_binary",
    )

    apple_verification_test(
        name = "{}_watchos_simulator_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"watchos_cpus": "arm64"},
        expected_platform_type = "watchos",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:watch_binary",
    )

    apple_verification_test(
        name = "{}_ios_device_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"ios_multi_cpus": "x86_64,sim_arm64"},
        expected_platform_type = "ios",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:ios_binary",
    )

    apple_verification_test(
        name = "{}_ios_simulator_test".format(name),
        tags = [name],
        build_type = "device",
        cpus = {"ios_multi_cpus": "arm64,arm64e"},
        expected_platform_type = "ios",
        verifier_script = "//test/shell:verify_binary.sh",
        target_under_test = "//test/test_data:ios_binary",
    )
