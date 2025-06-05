# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""ios_sticker_pack_extension Starlark tests."""

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

def ios_sticker_pack_extension_test_suite(name):
    """Test suite for ios_extension.

    Args:
      name: the base name to be used in things created by this macro
    """
    apple_verification_test(
        name = "{}_codesign_test".format(name),
        build_type = "simulator",
        target_under_test = "//test/starlark_tests/targets_under_test/ios:sticker_ext",
        verifier_script = "verifier_scripts/codesign_verifier.sh",
        tags = [name],
    )

    infoplist_contents_test(
        name = "{}_plist_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/ios:sticker_ext",
        expected_values = {
            "BuildMachineOSBuild": "*",
            "CFBundleExecutable": "sticker_ext",
            "CFBundleIdentifier": "com.google.example.stickerext",
            "CFBundleName": "sticker_ext",
            "CFBundlePackageType": "XPC!",
            "CFBundleSupportedPlatforms:0": "iPhone*",
            "DTCompiler": "com.apple.compilers.llvm.clang.1_0",
            "DTPlatformBuild": "*",
            "DTPlatformName": "iphone*",
            "DTPlatformVersion": "*",
            "DTSDKBuild": "*",
            "DTSDKName": "iphone*",
            "DTXcode": "*",
            "DTXcodeBuild": "*",
            "LSApplicationIsStickerProvider": "YES",
            "MinimumOSVersion": common.min_os_ios.baseline,
            "NSExtension:NSExtensionPointIdentifier": "com.apple.message-payload-provider",
            "UIDeviceFamily:0": "1",
        },
        tags = [name],
    )

    archive_contents_test(
        name = "{}_strip_bitcode_test".format(name),
        build_type = "device",
        target_under_test = "//test/starlark_tests/targets_under_test/ios:sticker_ext",
        binary_test_file = "$BUNDLE_ROOT/sticker_ext",
        macho_load_commands_not_contain = ["segname __LLVM"],
        tags = [name],
    )

    infoplist_contents_test(
        name = "{}_capability_set_derived_bundle_id_plist_test".format(name),
        target_under_test = "//test/starlark_tests/targets_under_test/ios:sticker_ext_with_capability_set_derived_bundle_id",
        expected_values = {
            "CFBundleIdentifier": "com.bazel.app.example.sticker-ext-with-capability-set-derived-bundle-id",
        },
        tags = [name],
    )

    native.test_suite(
        name = name,
        tags = [name],
    )
