# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Contains a function for loading Riegeli dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def load_riegli_deps():
    maybe(
        http_archive,
        "highwayhash",
        urls = ["https://github.com/google/highwayhash/archive/c13d28517a4db259d738ea4886b1f00352a3cc33.zip"],  # 2022-04-06
        build_file = "@com_google_riegeli//third_party:highwayhash.BUILD",
        sha256 = "76f4c3cbb51bb111a0ba23756cf5eed099ea43fc5ffa7251faeed2a3c2d1adc0",
        strip_prefix = "highwayhash-c13d28517a4db259d738ea4886b1f00352a3cc33",
    )

    maybe(
        http_archive,
        "org_brotli",
        urls = ["https://github.com/google/brotli/archive/3914999fcc1fda92e750ef9190aa6db9bf7bdb07.zip"],  # 2022-11-17
        sha256 = "84a9a68ada813a59db94d83ea10c54155f1d34399baf377842ff3ab9b3b3256e",
        strip_prefix = "brotli-3914999fcc1fda92e750ef9190aa6db9bf7bdb07",
    )

    maybe(
        http_archive,
        "net_zstd",
        urls = ["https://github.com/facebook/zstd/archive/v1.4.5.zip"],  # 2020-05-22
        build_file = "@com_google_riegeli//third_party:net_zstd.BUILD",
        sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
        strip_prefix = "zstd-1.4.5/lib",
    )

    maybe(
        http_archive,
        "snappy",
        urls = ["https://github.com/google/snappy/archive/1.1.8.zip"],  # 2020-01-14
        strip_prefix = "snappy-1.1.8",
        build_file = "@com_google_riegeli//third_party:snappy.BUILD",
        sha256 = "38b4aabf88eb480131ed45bfb89c19ca3e2a62daeb081bdf001cfb17ec4cd303",
    )
