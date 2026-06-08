# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""
Contains build rules for ocpdiag hardware diagnostic tests. (go/ocpdiag-intro)
"""

licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

exports_files(["LICENSE"])

config_setting(
    name = "ovss",
    values = {"define": "release=ovss"},
    visibility = ["//ocpdiag:__subpackages__"],
)
