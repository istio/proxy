
load("@rules_cc//cc/private/rules_impl:native_cc_common.bzl", "native_cc_common")
load("@rules_cc//cc/private/rules_impl:native_providers.bzl", "NativeCcInfo")
load("@rules_cc//cc/private/rules_impl:native_providers.bzl", "NativeDebugPackageInfo")
load("@rules_cc//cc/private/rules_impl:native_providers.bzl", "NativeCcToolchainConfigInfo")
load("@rules_cc//cc/private/rules_impl:native_providers.bzl", "NativeCcSharedLibraryInfo")
cc_common = native_cc_common
CcInfo = NativeCcInfo
DebugPackageInfo = NativeDebugPackageInfo
CcToolchainConfigInfo = NativeCcToolchainConfigInfo
ObjcInfo = apple_common.Objc
new_objc_provider = apple_common.new_objc_provider
CcSharedLibraryInfo = NativeCcSharedLibraryInfo
            