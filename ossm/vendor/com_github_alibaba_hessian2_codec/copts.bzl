"""
Flags specified here must not impact ABI. Code compiled with and without these
opts will be linked together, and in some cases headers compiled with and
without these options will be part of the same program.

We use the same flags as absl.
"""

load(
    "@com_google_absl//absl:copts/GENERATED_copts.bzl",
    "ABSL_GCC_FLAGS",
    "ABSL_GCC_TEST_FLAGS",
    "ABSL_LLVM_FLAGS",
    "ABSL_LLVM_TEST_FLAGS",
    "ABSL_MSVC_FLAGS",
    "ABSL_MSVC_TEST_FLAGS",
)

DEFAULT_COPTS = select({
    "//:windows": ABSL_MSVC_FLAGS,
    "//:llvm_compiler": ABSL_LLVM_FLAGS,
    "//conditions:default": ABSL_GCC_FLAGS,
})

TEST_COPTS = DEFAULT_COPTS + select({
    "//:windows": ABSL_MSVC_TEST_FLAGS,
    "//:llvm_compiler": ABSL_LLVM_TEST_FLAGS,
    "//conditions:default": ABSL_GCC_TEST_FLAGS,
})
