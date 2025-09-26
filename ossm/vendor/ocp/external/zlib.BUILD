# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

# Description:
#   zlib is a general purpose data compression library.  All the code is thread safe.  The data
#   format used by the zlib library is described by RFCs (Request for Comments) 1950 to 1952 in
#   the files http://tools.ietf.org/html/rfc1950 (zlib format), rfc1951 (deflate format) and
#   rfc1952 (gzip format).

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "zlib",
    srcs = [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "deflate.c",
        "gzclose.c",
        "gzlib.c",
        "gzread.c",
        "gzwrite.c",
        "infback.c",
        "inffast.c",
        "inflate.c",
        "inftrees.c",
        "trees.c",
        "uncompr.c",
        "zutil.c",
    ],
    hdrs = glob(["*.h"]),
    copts = [
        "-Wno-implicit-fallthrough",
        "-Wno-strict-prototypes",
        "-Wno-unused-parameter",
        "-Wno-unused-function",
        "-Wno-cast-qual",
        "-std=c++11",
    ],
    visibility = ["//visibility:public"],
)
