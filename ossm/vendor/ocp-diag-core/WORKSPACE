# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

workspace(name = "ocpdiag")

# First load all the repositories we need.
load("//ocpdiag:build_deps.bzl", "load_deps")
load_deps()

# Some of these repositories have dependencies of their own,
# with getter methods. We need to call these from a separate
# bazel file, since there are bazel load statements that fail
# before the above initial load is complete.
load("//ocpdiag:secondary_build_deps.bzl", "load_secondary_deps")
load_secondary_deps()

# Finally, some dependencies have dependencies that have dependencies
# of their own. We cannot include these in the load_secondary_deps
# file for the same reason we can't include the load_secondary_deps
# functionality in the original load_ocpdiag_deps function.
load("//ocpdiag:tertiary_build_deps.bzl", "load_tertiary_deps")
load_tertiary_deps()
