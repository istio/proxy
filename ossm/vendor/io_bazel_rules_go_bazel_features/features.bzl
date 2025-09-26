bazel_features = struct(
  cc = struct(
    find_cpp_toolchain_has_mandatory_param = True,
  ),
  external_deps = struct(
    # WORKSPACE users have no use for bazel mod tidy.
    bazel_mod_tidy = False,
  ),
)
