"""Declare runtime dependencies

These are needed for local dev, and users must install them as well.
See https://docs.bazel.build/versions/main/skylark/deploying.html#dependencies
"""

load("//js/private:maybe.bzl", http_archive = "maybe_http_archive")

# buildifier: disable=function-docstring
def rules_js_dependencies():
    http_archive(
        name = "bazel_skylib",
        sha256 = "b8a1527901774180afc798aeb28c4634bdccf19c4d98e7bdd1ce79d1fe9aaad7",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.4.1/bazel-skylib-1.4.1.tar.gz"],
    )

    # TODO(2.0): update to rules_nodejs v6
    http_archive(
        name = "rules_nodejs",
        sha256 = "8fc8e300cb67b89ceebd5b8ba6896ff273c84f6099fc88d23f24e7102319d8fd",
        urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.4/rules_nodejs-core-5.8.4.tar.gz"],
    )

    http_archive(
        name = "aspect_bazel_lib",
        sha256 = "d0529773764ac61184eb3ad3c687fb835df5bee01afedf07f0cf1a45515c96bc",
        strip_prefix = "bazel-lib-1.42.3",
        url = "https://github.com/aspect-build/bazel-lib/releases/download/v1.42.3/bazel-lib-v1.42.3.tar.gz",
    )

    http_archive(
        name = "bazel_features",
        sha256 = "f3082bfcdca73dc77dcd68faace806135a2e08c230b02b1d9fbdbd7db9d9c450",
        strip_prefix = "bazel_features-0.1.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v0.1.0/bazel_features-v0.1.0.tar.gz",
    )
