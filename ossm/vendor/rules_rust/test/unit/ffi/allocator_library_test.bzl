"""Analysis tests for allocator_library platform selection"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def _allocator_library_provides_ccinfo_test_impl(ctx):
    env = analysistest.begin(ctx)

    # Get the target under test
    target_under_test = analysistest.target_under_test(env)

    # Basic test: ensure the target provides CcInfo (this means it analyzed successfully)
    asserts.true(env, CcInfo in target_under_test, "allocator_library should provide CcInfo")

    # The fact that this test passes means the select() logic worked correctly
    # and didn't fail due to missing WASI toolchain tools

    return analysistest.end(env)

# Create analysis test rule that works without needing WASI toolchain
allocator_library_analysis_test = analysistest.make(
    _allocator_library_provides_ccinfo_test_impl,
)

def allocator_library_test_suite(name):
    """Test suite for allocator_library platform behavior"""

    # Test that allocator_library can be analyzed successfully
    # This indirectly tests that our WASI select() fix works
    allocator_library_analysis_test(
        name = "allocator_library_analysis_test",
        target_under_test = "//ffi/cc/allocator_library:allocator_library",
    )

    native.test_suite(
        name = name,
        tests = [
            ":allocator_library_analysis_test",
        ],
    )
