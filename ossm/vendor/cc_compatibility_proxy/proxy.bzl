
load("@rules_cc//cc/private/rules_impl:native.bzl", "NativeCcSharedLibraryInfo")
cc_binary = native.cc_binary
cc_import = native.cc_import
cc_library = native.cc_library
cc_shared_library = native.cc_shared_library
cc_static_library = getattr(native, "cc_static_library", None) # only in Bazel 8+
cc_test = native.cc_test
objc_import = native.objc_import
objc_library = native.objc_library
fdo_prefetch_hints = native.fdo_prefetch_hints
fdo_profile = native.fdo_profile
memprof_profile = getattr(native, "memprof_profile", None) # only in Bazel 7+
propeller_optimize = native.propeller_optimize
cc_toolchain = native.cc_toolchain
cc_toolchain_alias = native.cc_toolchain_alias

CcSharedLibraryInfo = NativeCcSharedLibraryInfo
            