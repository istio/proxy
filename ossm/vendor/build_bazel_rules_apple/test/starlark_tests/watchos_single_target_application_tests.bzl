# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""watchos_application Starlark tests."""

load(
    "//test/starlark_tests/rules:analysis_failure_message_test.bzl",
    "analysis_failure_message_test",
)
load(
    "//test/starlark_tests/rules:analysis_target_actions_test.bzl",
    "analysis_target_actions_test",
)
load(
    "//test/starlark_tests/rules:apple_dsym_bundle_info_test.bzl",
    "apple_dsym_bundle_info_test",
)
load(
    "//test/starlark_tests/rules:apple_verification_test.bzl",
    "apple_verification_test",
)
load(
    "//test/starlark_tests/rules:common_verification_tests.bzl",
    "archive_contents_test",
)
load(
    "//test/starlark_tests/rules:infoplist_contents_test.bzl",
    "infoplist_contents_test",
)
load(
    ":common.bzl",
    "common",
)

def watchos_single_target_application_test_suite(name):
    """Test suite for watchos_single_target_application.

    Args:
      name: the base name to be used in things created by this macro
    """
    analysis_failure_message_test(
        name = "{}_too_low_minimum_os_version_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app_too_low_minos",
        expected_error = "Single-target watchOS applications require a minimum_os_version of 7.0 or greater.",
        tags = [
            name,
        ],
    )

    analysis_failure_message_test(
        name = "{}_unexpected_watch2_extension_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app_with_watch2_ext",
        expected_error = """
Single-target watchOS applications do not support watchOS 2 extensions or their delegates.

Please remove the assigned watchOS 2 app `extension` and make sure a valid watchOS application
delegate is referenced in the single-target `watchos_application`'s `deps`.
""",
        tags = [
            name,
        ],
    )

    apple_verification_test(
        name = "{}_codesign_test".format(name),
        build_type = "simulator",
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        verifier_script = "verifier_scripts/codesign_verifier.sh",
        tags = [
            name,
            "never-on-beta",  # TODO(b/249829891): Remove once internal beta testing issue is fixed.
        ],
    )

    apple_verification_test(
        name = "{}_no_custom_fmwks_test".format(name),
        build_type = "simulator",
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app_with_imported_fmwk",
        verifier_script = "verifier_scripts/no_custom_fmwks_verifier.sh",
        tags = [
            name,
            "never-on-beta",  # TODO(b/249829891): Remove once internal beta testing issue is fixed.
        ],
    )

    apple_verification_test(
        name = "{}_entitlements_simulator_test".format(name),
        build_type = "simulator",
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        verifier_script = "verifier_scripts/entitlements_verifier.sh",
        tags = [name],
    )

    apple_verification_test(
        name = "{}_entitlements_device_test".format(name),
        build_type = "device",
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        verifier_script = "verifier_scripts/entitlements_verifier.sh",
        tags = [name],
    )

    apple_dsym_bundle_info_test(
        name = "{}_dsym_bundle_info_files_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        expected_direct_dsyms = ["dSYMs/single_target_app.app.dSYM"],
        expected_transitive_dsyms = ["dSYMs/single_target_app.app.dSYM"],
        tags = [name],
    )

    infoplist_contents_test(
        name = "{}_plist_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        expected_values = {
            "BuildMachineOSBuild": "*",
            "CFBundleExecutable": "single_target_app",
            "CFBundleIdentifier": "com.google.example",
            "CFBundleName": "single_target_app",
            "CFBundlePackageType": "APPL",
            "CFBundleSupportedPlatforms:0": "WatchSimulator*",
            "DTCompiler": "com.apple.compilers.llvm.clang.1_0",
            "DTPlatformBuild": "*",
            "DTPlatformName": "watchsimulator",
            "DTPlatformVersion": "*",
            "DTSDKBuild": "*",
            "DTSDKName": "watchsimulator*",
            "DTXcode": "*",
            "DTXcodeBuild": "*",
            "MinimumOSVersion": common.min_os_watchos.single_target_app,
            "UIDeviceFamily:0": "4",
            "WKApplication": "true",
        },
        tags = [
            name,
            "never-on-beta",  # TODO(b/249829891): Remove once internal beta testing issue is fixed.
        ],
    )

    # Tests xcasset tool is passed the correct arguments.
    analysis_target_actions_test(
        name = "{}_xcasset_actool_argv".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app",
        target_mnemonic = "AssetCatalogCompile",
        expected_argv = [
            "xctoolrunner actool --compile",
            "--minimum-deployment-target " + common.min_os_watchos.single_target_app,
            "--platform watchsimulator",
        ],
        tags = [
            name,
        ],
    )

    # Post-ABI stability, Swift should not be bundled at all.
    archive_contents_test(
        name = "{}_device_build_ios_swift_watchos_swift_stable_abi_test".format(name),
        build_type = "device",
        not_contains = [
            "$ARCHIVE_ROOT/SwiftSupport/iphoneos/libswiftCore.dylib",
            "$ARCHIVE_ROOT/SwiftSupport/watchos/libswiftCore.dylib",
            "$BUNDLE_ROOT/Frameworks/libswiftCore.dylib",
            "$BUNDLE_ROOT/Watch/app.app/Frameworks/libswiftCore.dylib",
            "$BUNDLE_ROOT/Watch/app.app/PlugIns/ext.appex/Frameworks/libswiftCore.dylib",
        ],
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:ios_with_swift_single_target_watchos_with_swift_stable_abi",
        tags = [
            name,
            "never-on-beta",  # TODO(b/249829891): Remove once internal beta testing issue is fixed.
        ],
    )

    archive_contents_test(
        name = "{}_contains_extension_bundle_test".format(name),
        build_type = "device",
        contains = [
            "$BUNDLE_ROOT/single_target_app_with_extension",
            "$BUNDLE_ROOT/PlugIns/watchos_app_extension.appex/watchos_app_extension",
        ],
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app_with_extension",
        tags = [
            name,
        ],
    )

    infoplist_contents_test(
        name = "{}_capability_set_derived_bundle_id_plist_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/watchos:single_target_app_with_capability_set_derived_bundle_id",
        expected_values = {
            "CFBundleIdentifier": "com.bazel.app.example.watchkitapp",
        },
        tags = [name],
    )

    native.test_suite(
        name = name,
        tags = [
            name,
        ],
    )
