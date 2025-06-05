# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

# Description:
#   This is a package for manipulating geometric shapes. Unlike many geometry libraries, S2 is
#   primarily designed to work with spherical geometry, i.e., shapes drawn on a sphere rather
#   than on a planar 2D map. This makes it especially suitable for working with geographic data.

licenses(["notice"])  # Apache License 2.0

exports_files(["LICENSE"])

cc_library(
    name = "bits",
    srcs = [
      "s2/util/bits/bits.cc",
      "s2/util/bits/bit-interleave.cc"
    ],
    hdrs = [
      "s2/util/bits/bits.h",
      "s2/util/bits/bit-interleave.h",
      "s2/base/integral_types.h",
      "s2/base/logging.h",
      "s2/base/log_severity.h",
      "s2/base/port.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/numeric:bits",
      "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "endian",
    hdrs = [
        "s2/util/endian/endian.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/base:config",
      "@com_google_absl//absl/base:endian",
      "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "gtl_util",
    hdrs = [
        "s2/util/gtl/compact_array.h",
        "s2/util/gtl/container_logging.h",
        "s2/util/gtl/dense_hash_set.h",
        "s2/util/gtl/densehashtable.h",
        "s2/util/gtl/hashtable_common.h",
        "s2/util/gtl/legacy_random_shuffle.h",
    ],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings:internal",
    ],
)

cc_library(
    name = "hash",
    hdrs = [
        "s2/util/hash/mix.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/base:endian",
      "@com_google_absl//absl/numeric:int128",
      "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "coding",
    srcs = [
        "s2/util/coding/coder.cc",
        "s2/util/coding/varint.cc",
    ],
    hdrs = [
        "s2/util/coding/coder.h",
        "s2/util/coding/nth-derivative.h",
        "s2/util/coding/transforms.h",
        "s2/util/coding/varint.h",
        "s2/base/port.h",
        "s2/base/casts.h",
        "s2/base/logging.h",
        "s2/base/log_severity.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/numeric:int128",
      "@com_google_absl//absl/utility",
      ":bits",
      ":endian",
      ":gtl_util",
      ":hash",
    ],
)

cc_library(
    name = "mathutil",
    srcs = ["s2/util/math/mathutil.cc"],
    hdrs = [
        "s2/util/math/mathutil.h",
        "s2/base/integral_types.h",
        "s2/base/logging.h",
        "s2/base/log_severity.h",
        "s2/util/bits/bits.h",
        "s2/base/port.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/numeric:bits",
      "@com_google_absl//absl/numeric:int128",
      "@com_google_absl//absl/meta:type_traits",
    ],
)

cc_library(
    name = "vector",
    hdrs = [
      "s2/util/math/vector.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/utility",
    ],
)

cc_library(
    name = "matrix",
    hdrs = [
      "s2/util/math/matrix3x3.h",
      "s2/util/math/vector.h",
    ],
    deps = [
      ":mathutil",
      ":vector",
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/base:core_headers",
      "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "r1interval",
    hdrs = [
        "s2/_fp_contract_off.h",
        "s2/r1interval.h",
    ],
    deps = [
      "@com_google_absl//absl/base",
      "@com_google_absl//absl/log:check",
      ":vector",
    ],
)

cc_library(
    name = "s2cell_id",
    srcs = [
        "s2/r2rect.cc",
        "s2/s1angle.cc",
        "s2/s1chord_angle.cc",
        "s2/s2cell_id.cc",
        "s2/s2coords.cc",
        "s2/s2coords_internal.h",
        "s2/s2latlng.cc",
        "s2/s2pointutil.cc",
    ],
    hdrs = [
        "s2/_fp_contract_off.h",
        "s2/r2.h",
        "s2/r2rect.h",
        "s2/s1angle.h",
        "s2/s1chord_angle.h",
        "s2/s2cell_id.h",
        "s2/s2coords.h",
        "s2/s2error.h",
        "s2/s2latlng.h",
        "s2/s2point.h",
        "s2/s2pointutil.h",
        "s2/s2region.h",
        "s2/s2shape.h",
        "s2/base/integral_types.h",
        "s2/base/logging.h",
        "s2/base/log_severity.h",
        "s2/util/bits/bits.h",
        "s2/base/port.h",
    ],
    textual_hdrs = [
        "s2/_fp_contract_off.h",
        "s2/r1interval.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":r1interval",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        ":bits",
        ":coding",
        ":mathutil",
        ":matrix",
        ":vector",
    ],
)
