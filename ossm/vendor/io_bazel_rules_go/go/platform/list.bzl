# Copyright 2017 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "//go/private:platforms.bzl",
    _GOARCH = "GOARCH_CONSTRAINTS",
    _GOOS = "GOOS_CONSTRAINTS",
    _GOOS_GOARCH = "GOOS_GOARCH",
    _MSAN_GOOS_GOARCH = "MSAN_GOOS_GOARCH",
    _RACE_GOOS_GOARCH = "RACE_GOOS_GOARCH",
)

GOOS_GOARCH = _GOOS_GOARCH
GOOS = _GOOS
GOARCH = _GOARCH
RACE_GOOS_GOARCH = _RACE_GOOS_GOARCH
MSAN_GOOS_GOARCH = _MSAN_GOOS_GOARCH

def declare_config_settings():
    """Generates config_setting targets.

    declare_config_settings declares a config_setting target for each goos,
    goarch, and valid goos_goarch pair. These targets may be used in select
    expressions. Each target refers to a corresponding constraint_value in
    //go/toolchain.
    """
    for goos in GOOS:
        native.config_setting(
            name = goos,
            constraint_values = [Label("//go/toolchain:" + goos)],
            visibility = ["//visibility:public"],
        )
    for goarch in GOARCH:
        native.config_setting(
            name = goarch,
            constraint_values = [Label("//go/toolchain:" + goarch)],
            visibility = ["//visibility:public"],
        )
    for goos, goarch in GOOS_GOARCH:
        native.config_setting(
            name = goos + "_" + goarch,
            constraint_values = [
                Label("//go/toolchain:" + goos),
                Label("//go/toolchain:" + goarch),
            ],
            visibility = ["//visibility:public"],
        )

    # Setting that determines whether cgo is enabled.
    # This is experimental and may be changed or removed when we migrate
    # to build settings.
    native.config_setting(
        name = "internal_cgo_off",
        constraint_values = [Label("//go/toolchain:cgo_off")],
        visibility = [Label("//:__pkg__")],
    )
