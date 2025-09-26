"is_bazel_7_or_greater"

def is_bazel_7_or_greater():
    # Vendored in from https://github.com/aspect-build/bazel-lib/blob/adad7889c925c4f22a2f84568268f0a62e7c2fb0/lib/private/utils.bzl#L208
    # so that rules_js remains compatible with aspect_bazel_lib >= 2.0.0 and < 2.2.0.
    # TODO(2.0): remove this and switch to the upstream function and bump minimum aspect_bazel_lib version to 2.2.0
    return "apple_binary" not in dir(native) and "cc_host_toolchain_alias" not in dir(native)
