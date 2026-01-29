# The Bazel build is primarily for use by Envoy.
#
# Envoy forbids use of std::string_view and std::optional, preferring use of
# Abseil's absl::string_view and absl::optional instead.
#
# In the context of an Envoy build, the Abseil libraries point to whatever
# versions Envoy uses, including some source patches.
#
# To test this library's Bazel build independent of Envoy, we need to specify
# versions of the Abseil libraries, including Envoy's patches. That is what this
# file is for.

# These rules are based on <https://abseil.io/docs/cpp/quickstart>,
# accessed December 6, 2022.
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
  name = "com_google_absl",
  urls = ["https://github.com/abseil/abseil-cpp/archive/98eb410c93ad059f9bba1bf43f5bb916fc92a5ea.zip"],
  sha256 = "aabf6c57e3834f8dc3873a927f37eaf69975d4b28117fc7427dfb1c661542a87",
  strip_prefix = "abseil-cpp-98eb410c93ad059f9bba1bf43f5bb916fc92a5ea",
  patches = ["//:abseil.patch"],
  patch_args = ["-p1"],
)

http_archive(
  name = "bazel_skylib",
  urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz"],
  sha256 = "f7be3474d42aae265405a592bb7da8e171919d74c16f082a5457840f06054728",
)
